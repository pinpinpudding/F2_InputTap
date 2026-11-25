#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "InputHub.h"

class InputTapProcessor : public juce::AudioProcessor,
                          private juce::AsyncUpdater
{
public:
    InputTapProcessor();
    ~InputTapProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "F2_InputTap_vst3"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // Message-thread API for the editor
    juce::StringArray getInputDeviceNames (bool rescan) { return hub->getInputDeviceNames (rescan); }
    juce::String getDriverName() const                  { return hub->getDriverName(); }
    void selectDevice (const juce::String& deviceName);
    void reconnect();
    juce::String getCurrentDevice() const               { return currentDevice; }

    struct Status
    {
        bool deviceOpen = false, running = false;
        double deviceRate = 0, bufferedMs = 0;
        int underruns = 0, overflows = 0, instancesOnDevice = 0;
        juce::String error;
    };
    Status getStatus() const;

    juce::AudioProcessorValueTreeState params;

private:
    void handleAsyncUpdate() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void discardAll() noexcept;

    juce::SharedResourcePointer<InputHub> hub;
    std::shared_ptr<InputDeviceStream> stream;   // message thread only
    juce::String currentDevice, lastError;

    juce::CriticalSection pendingLock;
    juce::String pendingDevice;

    InputSubscriber subscriber;                  // written by device thread, read by audio thread

    // audio-thread state
    double hostRate = 44100.0;
    bool priming = true;
    double smoothedFill = 0.0;
    juce::LagrangeInterpolator interpolators[2];
    juce::AudioBuffer<float> readTemp, outTemp;
    juce::SmoothedValue<float> gainSmoothed[2];   // per channel: gain x pan

    std::atomic<bool> flushRequested { true };
    std::atomic<int> underruns { 0 };
    std::atomic<float> bufferedMs { 0.0f };

    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* latencyParam = nullptr;
    std::atomic<float>* passParam = nullptr;
    std::atomic<float>* muteParam = nullptr;
    std::atomic<float>* panParam = nullptr;

    void updateGainTargets() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputTapProcessor)
};
