#include "InputHub.h"

//==============================================================================
InputSubscriber::InputSubscriber()
{
    buffer.clear();
}

void InputSubscriber::push (const float* const* data, int numChannels, int numSamples) noexcept
{
    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    for (int ch = 0; ch < 2; ++ch)
    {
        // Mono devices are copied to both channels
        const float* src = numChannels > 0 ? data[juce::jmin (ch, numChannels - 1)] : nullptr;

        if (src != nullptr)
        {
            if (size1 > 0) buffer.copyFrom (ch, start1, src, size1);
            if (size2 > 0) buffer.copyFrom (ch, start2, src + size1, size2);
        }
        else
        {
            if (size1 > 0) buffer.clear (ch, start1, size1);
            if (size2 > 0) buffer.clear (ch, start2, size2);
        }
    }

    fifo.finishedWrite (size1 + size2);

    if (size1 + size2 < numSamples)
        overflows.fetch_add (1, std::memory_order_relaxed); // reader isn't keeping up (e.g. host paused)
}

//==============================================================================
InputDeviceStream::InputDeviceStream (juce::AudioIODeviceType& t, const juce::String& deviceName)
    : type (t), name (deviceName)
{
}

InputDeviceStream::~InputDeviceStream()
{
    close();
}

juce::String InputDeviceStream::open()
{
    close();

    device.reset (type.createDevice ({}, name)); // input only

    juce::String error;

    if (device == nullptr)
    {
        error = "Could not create device: " + name;
    }
    else
    {
        const int channels = juce::jmin (2, device->getInputChannelNames().size());

        if (channels == 0)
        {
            error = "Device has no input channels";
        }
        else
        {
            juce::BigInteger inputBits;
            inputBits.setRange (0, channels, true);

            const auto rates = device->getAvailableSampleRates();
            double rate = 48000.0;

            if (! rates.isEmpty() && ! rates.contains (48000.0))
                rate = rates.contains (44100.0) ? 44100.0 : rates[0];

            error = device->open (inputBits, {}, rate, device->getDefaultBufferSize());

            if (error.isEmpty())
            {
                numInputs = channels;
                device->start (this);
            }
        }
    }

    if (error.isNotEmpty())
        device.reset();

    const juce::ScopedLock sl (errorLock);
    lastError = error;
    return error;
}

void InputDeviceStream::close()
{
    if (device != nullptr)
    {
        device->stop();
        device->close();
        device.reset();
    }

    running = false;
}

void InputDeviceStream::addSubscriber (InputSubscriber* s)
{
    s->sourceRate = sampleRate.load();
    const juce::SpinLock::ScopedLockType sl (subscriberLock);
    subscribers.addIfNotAlreadyThere (s);
}

void InputDeviceStream::removeSubscriber (InputSubscriber* s)
{
    const juce::SpinLock::ScopedLockType sl (subscriberLock);
    subscribers.removeFirstMatchingValue (s);
}

int InputDeviceStream::getNumSubscribers() const
{
    const juce::SpinLock::ScopedLockType sl (subscriberLock);
    return subscribers.size();
}

juce::String InputDeviceStream::getLastError() const
{
    const juce::ScopedLock sl (errorLock);
    return lastError;
}

void InputDeviceStream::audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                                          float* const* out, int numOut,
                                                          int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int i = 0; i < numOut; ++i)
        if (out[i] != nullptr)
            juce::FloatVectorOperations::clear (out[i], numSamples);

    const juce::SpinLock::ScopedLockType sl (subscriberLock);

    for (auto* s : subscribers)
        s->push (in, numIn, numSamples);
}

void InputDeviceStream::audioDeviceAboutToStart (juce::AudioIODevice* d)
{
    sampleRate = d->getCurrentSampleRate();

    const juce::SpinLock::ScopedLockType sl (subscriberLock);
    for (auto* s : subscribers)
        s->sourceRate = sampleRate.load();

    running = true;
}

void InputDeviceStream::audioDeviceStopped()
{
    running = false;
}

void InputDeviceStream::audioDeviceError (const juce::String& message)
{
    running = false;
    const juce::ScopedLock sl (errorLock);
    lastError = message;
}

//==============================================================================
InputHub::InputHub()
{
   #if JUCE_WINDOWS
    // WASAPI shared mode: several clients (FL Studio itself, other apps,
    // and every InputTap instance) can use the same microphone at once.
    type.reset (juce::AudioIODeviceType::createAudioIODeviceType_WASAPI (juce::WASAPIDeviceMode::shared));
   #endif

    if (type == nullptr)
    {
        juce::AudioDeviceManager temp;
        juce::OwnedArray<juce::AudioIODeviceType> types;
        temp.createAudioDeviceTypes (types);

        if (! types.isEmpty())
            type.reset (types.removeAndReturn (0));
    }

    if (type != nullptr)
        type->scanForDevices();
}

InputHub::~InputHub()
{
    streams.clear(); // closes all devices
}

juce::String InputHub::getDriverName() const
{
    return type != nullptr ? type->getTypeName() : juce::String ("(no driver)");
}

juce::StringArray InputHub::getInputDeviceNames (bool rescan)
{
    if (type == nullptr)
        return {};

    if (rescan)
        type->scanForDevices();

    return type->getDeviceNames (true);
}

std::shared_ptr<InputDeviceStream> InputHub::acquire (const juce::String& deviceName, juce::String& error)
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (type == nullptr)
    {
        error = "No audio driver available";
        return {};
    }

    auto& entry = streams[deviceName];

    if (entry.stream == nullptr)
    {
        auto stream = std::make_shared<InputDeviceStream> (*type, deviceName);
        error = stream->open();

        if (error.isNotEmpty())
        {
            streams.erase (deviceName);
            return {};
        }

        entry.stream = std::move (stream);
    }
    else if (! entry.stream->isRunning())
    {
        entry.stream->open(); // try to recover a lost device (keeps existing subscribers)
    }

    ++entry.refs;
    return entry.stream;
}

void InputHub::release (const juce::String& deviceName)
{
    JUCE_ASSERT_MESSAGE_THREAD

    auto it = streams.find (deviceName);
    if (it == streams.end())
        return;

    if (--it->second.refs <= 0)
        streams.erase (it);
}
