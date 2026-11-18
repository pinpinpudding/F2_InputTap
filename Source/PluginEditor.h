#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class InputTapEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit InputTapEditor (InputTapProcessor&);
    ~InputTapEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void refreshDeviceList (bool rescan);

private:
    void timerCallback() override;

    InputTapProcessor& proc;

    juce::Label deviceLabel { {}, "Input device" };
    juce::ComboBox deviceBox;
    juce::TextButton rescanButton { "Rescan" }, reconnectButton { "Reconnect" };

    void setupSliderRow (juce::Slider&, juce::TextButton& resetButton, const juce::String& paramID);
    void resetParameter (const juce::String& paramID);

    juce::Slider gainSlider, latencySlider, panSlider;
    juce::Label gainLabel { {}, "Gain" }, latencyLabel { {}, "Buffer" }, panLabel { {}, "Pan" };
    juce::TextButton gainReset { "Reset" }, latencyReset { "Reset" }, panReset { "Reset" };
    juce::ToggleButton passButton { "Pass through track input" }, muteButton { "Mute" };
    juce::Label statusLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment, latencyAttachment, panAttachment;
    std::unique_ptr<ButtonAttachment> passAttachment, muteAttachment;

    juce::StringArray deviceNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputTapEditor)
};
