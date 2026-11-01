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

    juce::Slider gainSlider, latencySlider;
    juce::Label gainLabel { {}, "Gain" }, latencyLabel { {}, "Buffer" };
    juce::ToggleButton passButton { "Pass through track input" }, muteButton { "Mute" };
    juce::Label statusLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment, latencyAttachment;
    std::unique_ptr<ButtonAttachment> passAttachment, muteAttachment;

    juce::StringArray deviceNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputTapEditor)
};
