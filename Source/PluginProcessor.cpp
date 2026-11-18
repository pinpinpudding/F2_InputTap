#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
InputTapProcessor::InputTapProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "InputTap", createLayout())
{
    gainParam    = params.getRawParameterValue ("gain");
    latencyParam = params.getRawParameterValue ("latency");
    passParam    = params.getRawParameterValue ("passthrough");
    muteParam    = params.getRawParameterValue ("mute");
    panParam     = params.getRawParameterValue ("pan");
}

InputTapProcessor::~InputTapProcessor()
{
    cancelPendingUpdate();
    selectDevice ({});
}

juce::AudioProcessorValueTreeState::ParameterLayout InputTapProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain",
                    NormalisableRange<float> (-60.0f, 24.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "latency", 1 }, "Buffer",
                    NormalisableRange<float> (3.0f, 200.0f, 1.0f, 0.5f), 20.0f,
                    AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pan", 1 }, "Pan",
                    NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                    AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int)
                        {
                            const int i = juce::roundToInt (v);
                            return i == 0 ? juce::String ("C")
                                          : juce::String (std::abs (i)) + (i < 0 ? "L" : "R");
                        })
                        .withValueFromStringFunction ([] (const juce::String& t)
                        {
                            const auto s = t.trim().toUpperCase();
                            if (s == "C" || s.isEmpty()) return 0.0f;
                            const float n = std::abs (s.retainCharacters ("0123456789.").getFloatValue());
                            return s.containsChar ('L') || s.startsWithChar ('-') ? -n : n;
                        })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "passthrough", 1 }, "Pass Through Track Input", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "mute", 1 }, "Mute Device", false));

    return layout;
}

bool InputTapProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

//==============================================================================
void InputTapProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;

    // Large enough for any resampling ratio we will meet
    readTemp.setSize (2, InputSubscriber::capacity, false, true, true);
    outTemp.setSize (2, juce::jmax (samplesPerBlock, 4096), false, true, true);

    for (auto& g : gainSmoothed)
        g.reset (sampleRate, 0.02);

    updateGainTargets();
    for (auto& g : gainSmoothed)
        g.setCurrentAndTargetValue (g.getTargetValue());

    flushRequested = true;
}

void InputTapProcessor::updateGainTargets() noexcept
{
    const float gain = muteParam->load() > 0.5f ? 0.0f
                                                : juce::Decibels::decibelsToGain (gainParam->load());

    // Balance law: centre = unity on both sides, the opposite side fades out as you pan
    const float pan = juce::jlimit (-1.0f, 1.0f, panParam->load() / 100.0f);
    gainSmoothed[0].setTargetValue (gain * (pan > 0.0f ? 1.0f - pan : 1.0f));
    gainSmoothed[1].setTargetValue (gain * (pan < 0.0f ? 1.0f + pan : 1.0f));
}

void InputTapProcessor::discardAll() noexcept
{
    subscriber.fifo.finishedRead (subscriber.fifo.getNumReady());
    for (auto& i : interpolators)
        i.reset();
    priming = true;
    smoothedFill = 0.0;
}

void InputTapProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIns  = getTotalNumInputChannels();
    const int numOuts = getTotalNumOutputChannels();

    for (int ch = numIns; ch < numOuts; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (passParam->load() < 0.5f)
        buffer.clear();

    if (flushRequested.exchange (false))
        discardAll();

    const double srcRate = subscriber.sourceRate.load();
    auto& fifo = subscriber.fifo;

    if (srcRate <= 0.0 || hostRate <= 0.0 || numSamples == 0)
    {
        discardAll();
        bufferedMs = 0.0f;
        return;
    }

    updateGainTargets();

    const double baseRatio = srcRate / hostRate;           // input samples per output sample
    const int maxNeeded = (int) std::ceil (numSamples * baseRatio * 1.01) + 8;

    // Never aim below one host block plus a small safety margin, whatever the user set
    const double target = juce::jmax (srcRate * latencyParam->load() / 1000.0,
                                      maxNeeded + srcRate * 0.015);

    int ready = fifo.getNumReady();

    // Host was paused / not calling us: too much audio piled up -> jump back to target latency
    if (ready > (int) (target * 3.0) + maxNeeded * 2)
    {
        fifo.finishedRead (ready - (int) target);
        ready = fifo.getNumReady();
        smoothedFill = target;
    }

    if (priming)
    {
        if (ready < (int) target + maxNeeded)
        {
            bufferedMs = (float) (ready * 1000.0 / srcRate);
            return; // still filling up -> silence from device
        }

        priming = false;
        smoothedFill = ready;
    }

    // Drift compensation: nudge the resampling ratio so the FIFO stays near the target fill
    smoothedFill += 0.02 * ((double) ready - smoothedFill);
    const double error = (smoothedFill - target) / target;
    const double correction = juce::jlimit (-0.005, 0.005, error * 0.01);
    const double ratio = baseRatio * (1.0 + correction);

    bufferedMs = (float) (ready * 1000.0 / srcRate);

    int done = 0;

    while (done < numSamples)
    {
        const int chunk = juce::jmin (numSamples - done, outTemp.getNumSamples());
        const int needed = juce::jmin ((int) std::ceil (chunk * ratio) + 8, readTemp.getNumSamples());
        const int available = fifo.getNumReady();

        if (available < needed)
        {
            underruns.fetch_add (1, std::memory_order_relaxed);
            priming = true;   // refill to the target before playing again
            for (auto& i : interpolators)
                i.reset();
            return;
        }

        // Peek 'needed' samples, then consume only what the interpolator used
        int start1, size1, start2, size2;
        fifo.prepareToRead (needed, start1, size1, start2, size2);

        for (int ch = 0; ch < 2; ++ch)
        {
            if (size1 > 0) readTemp.copyFrom (ch, 0, subscriber.buffer, ch, start1, size1);
            if (size2 > 0) readTemp.copyFrom (ch, size1, subscriber.buffer, ch, start2, size2);
        }

        int used = 0;
        for (int ch = 0; ch < 2; ++ch)
            used = interpolators[ch].process (ratio, readTemp.getReadPointer (ch), outTemp.getWritePointer (ch),
                                              chunk, size1 + size2, 0);

        fifo.finishedRead (juce::jmin (used, size1 + size2));

        // Apply smoothed gain and pan
        for (int ch = 0; ch < 2; ++ch)
            gainSmoothed[ch].applyGain (outTemp.getWritePointer (ch), chunk);

        if (numOuts == 1)
        {
            buffer.addFrom (0, done, outTemp, 0, 0, chunk, 0.5f);
            buffer.addFrom (0, done, outTemp, 1, 0, chunk, 0.5f);
        }
        else
        {
            for (int ch = 0; ch < numOuts; ++ch)
                buffer.addFrom (ch, done, outTemp, juce::jmin (ch, 1), 0, chunk);
        }

        done += chunk;
    }
}

//==============================================================================
void InputTapProcessor::selectDevice (const juce::String& deviceName)
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (stream != nullptr)
    {
        stream->removeSubscriber (&subscriber);   // device thread stops writing to us
        stream.reset();
        hub->release (currentDevice);
    }

    subscriber.sourceRate = 0.0;
    flushRequested = true;
    underruns = 0;
    subscriber.overflows = 0;
    currentDevice = deviceName;
    lastError.clear();

    if (deviceName.isEmpty())
        return;

    juce::String error;
    stream = hub->acquire (deviceName, error);

    if (stream == nullptr)
    {
        lastError = error;
        return;
    }

    stream->addSubscriber (&subscriber);
}

void InputTapProcessor::reconnect()
{
    const auto name = currentDevice;
    selectDevice ({});
    hub->getInputDeviceNames (true);
    selectDevice (name);
}

InputTapProcessor::Status InputTapProcessor::getStatus() const
{
    Status s;
    s.deviceOpen = stream != nullptr;
    s.running = stream != nullptr && stream->isRunning();
    s.deviceRate = stream != nullptr ? stream->getSampleRate() : 0.0;
    s.bufferedMs = bufferedMs.load();
    s.underruns = underruns.load();
    s.overflows = subscriber.overflows.load();
    s.instancesOnDevice = stream != nullptr ? stream->getNumSubscribers() : 0;
    s.error = lastError.isNotEmpty() ? lastError : (stream != nullptr ? stream->getLastError() : juce::String());
    return s;
}

//==============================================================================
void InputTapProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    {
        const juce::ScopedLock sl (pendingLock);
        state.setProperty ("device", pendingDevice.isNotEmpty() ? pendingDevice : currentDevice, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void InputTapProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (params.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const auto device = state.getProperty ("device").toString();
            params.replaceState (state);

            {
                const juce::ScopedLock sl (pendingLock);
                pendingDevice = device;
            }

            // Hosts may call this from any thread; open the device on the message thread
            triggerAsyncUpdate();
        }
    }
}

void InputTapProcessor::handleAsyncUpdate()
{
    juce::String device;
    {
        const juce::ScopedLock sl (pendingLock);
        device = pendingDevice;
        pendingDevice.clear();
    }

    if (device != currentDevice || stream == nullptr)
        selectDevice (device);

    if (auto* ed = dynamic_cast<InputTapEditor*> (getActiveEditor()))
        ed->refreshDeviceList (false);
}

//==============================================================================
juce::AudioProcessorEditor* InputTapProcessor::createEditor()
{
    return new InputTapEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InputTapProcessor();
}
