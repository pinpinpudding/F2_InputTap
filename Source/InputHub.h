#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <map>
#include <memory>

//==============================================================================
/** One plugin instance's private stereo FIFO.
    The device thread writes into it, the plugin's audio thread reads from it. */
class InputSubscriber
{
public:
    static constexpr int capacity = 1 << 16; // ~1.3 s at 48 kHz

    InputSubscriber();

    /** Called on the input device's thread. */
    void push (const float* const* data, int numChannels, int numSamples) noexcept;

    juce::AbstractFifo fifo { capacity };
    juce::AudioBuffer<float> buffer { 2, capacity };

    std::atomic<double> sourceRate { 0.0 };   // sample rate of the input device
    std::atomic<int> overflows { 0 };
};

//==============================================================================
/** One opened input device. Shared by every plugin instance in this process
    that selected the same device, so the device is opened only once. */
class InputDeviceStream : private juce::AudioIODeviceCallback
{
public:
    InputDeviceStream (juce::AudioIODeviceType& type, const juce::String& deviceName);
    ~InputDeviceStream() override;

    /** Opens (or re-opens) the device. Returns an error message, or empty on success. */
    juce::String open();
    void close();

    void addSubscriber (InputSubscriber*);
    void removeSubscriber (InputSubscriber*);
    int getNumSubscribers() const;

    bool isRunning() const noexcept          { return running.load(); }
    double getSampleRate() const noexcept    { return sampleRate.load(); }
    int getNumInputChannels() const noexcept { return numInputs.load(); }
    juce::String getLastError() const;

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& message) override;

    juce::AudioIODeviceType& type;
    const juce::String name;
    std::unique_ptr<juce::AudioIODevice> device;

    mutable juce::SpinLock subscriberLock;
    juce::Array<InputSubscriber*> subscribers;

    std::atomic<bool> running { false };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> numInputs { 0 };

    mutable juce::CriticalSection errorLock;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputDeviceStream)
};

//==============================================================================
/** Process-wide registry of opened input devices (use via juce::SharedResourcePointer).
    All methods must be called on the message thread. */
class InputHub
{
public:
    InputHub();
    ~InputHub();

    juce::String getDriverName() const;
    juce::StringArray getInputDeviceNames (bool rescan);

    /** Returns the shared stream for a device, opening it if needed. */
    std::shared_ptr<InputDeviceStream> acquire (const juce::String& deviceName, juce::String& error);

    /** Call once for every successful acquire(). Closes the device when nobody uses it. */
    void release (const juce::String& deviceName);

private:
    struct Entry
    {
        std::shared_ptr<InputDeviceStream> stream;
        int refs = 0;
    };

    std::unique_ptr<juce::AudioIODeviceType> type;
    std::map<juce::String, Entry> streams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputHub)
};
