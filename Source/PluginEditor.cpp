#include "PluginEditor.h"

InputTapEditor::InputTapEditor (InputTapProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    addAndMakeVisible (deviceLabel);
    addAndMakeVisible (deviceBox);
    addAndMakeVisible (rescanButton);
    addAndMakeVisible (reconnectButton);

    deviceBox.setTextWhenNothingSelected ("(none)");
    deviceBox.onChange = [this]
    {
        const int idx = deviceBox.getSelectedId() - 2; // id 1 = "(none)"
        const auto name = juce::isPositiveAndBelow (idx, deviceNames.size()) ? deviceNames[idx] : juce::String();

        if (name != proc.getCurrentDevice())
            proc.selectDevice (name);
    };

    rescanButton.onClick    = [this] { refreshDeviceList (true); };
    reconnectButton.onClick = [this] { proc.reconnect(); refreshDeviceList (false); };

    setupSliderRow (gainSlider,    gainReset,    "gain");
    setupSliderRow (latencySlider, latencyReset, "latency");
    setupSliderRow (panSlider,     panReset,     "pan");

    addAndMakeVisible (gainLabel);
    addAndMakeVisible (latencyLabel);
    addAndMakeVisible (panLabel);
    addAndMakeVisible (passButton);
    addAndMakeVisible (muteButton);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (statusLabel);

    gainAttachment    = std::make_unique<SliderAttachment> (proc.params, "gain", gainSlider);
    latencyAttachment = std::make_unique<SliderAttachment> (proc.params, "latency", latencySlider);
    panAttachment     = std::make_unique<SliderAttachment> (proc.params, "pan", panSlider);

    // Pan text (e.g. "25L", "C", "40R") comes from the parameter; gain/latency get a unit suffix
    gainSlider.setTextValueSuffix (" dB");
    latencySlider.setTextValueSuffix (" ms");
    passAttachment    = std::make_unique<ButtonAttachment> (proc.params, "passthrough", passButton);
    muteAttachment    = std::make_unique<ButtonAttachment> (proc.params, "mute", muteButton);

    refreshDeviceList (false);
    setSize (480, 320);
    startTimerHz (8);
}

InputTapEditor::~InputTapEditor()
{
    stopTimer();
}

void InputTapEditor::setupSliderRow (juce::Slider& s, juce::TextButton& resetButton, const juce::String& paramID)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 22);
    addAndMakeVisible (s);

    resetButton.setTooltip ("Reset to default");
    resetButton.onClick = [this, paramID] { resetParameter (paramID); };
    addAndMakeVisible (resetButton);
}

void InputTapEditor::resetParameter (const juce::String& paramID)
{
    if (auto* p = proc.params.getParameter (paramID))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

void InputTapEditor::refreshDeviceList (bool rescan)
{
    deviceNames = proc.getInputDeviceNames (rescan);
    const auto current = proc.getCurrentDevice();

    if (current.isNotEmpty() && ! deviceNames.contains (current))
        deviceNames.add (current); // keep a saved device visible even if unplugged

    deviceBox.clear (juce::dontSendNotification);
    deviceBox.addItem ("(none)", 1);

    for (int i = 0; i < deviceNames.size(); ++i)
        deviceBox.addItem (deviceNames[i], i + 2);

    const int idx = deviceNames.indexOf (current);
    deviceBox.setSelectedId (idx >= 0 ? idx + 2 : 1, juce::dontSendNotification);
}

void InputTapEditor::timerCallback()
{
    const auto s = proc.getStatus();
    juce::String text;

    text << "Driver: " << proc.getDriverName() << "\n";

    if (! s.deviceOpen)
    {
        text << (s.error.isNotEmpty() ? "Error: " + s.error : juce::String ("No device selected"));
    }
    else
    {
        text << (s.running ? "Running" : "Stopped") << "  |  "
             << juce::String (s.deviceRate, 0) << " Hz  |  buffered "
             << juce::String (s.bufferedMs, 1) << " ms\n"
             << "Underruns: " << s.underruns << "  |  Overflows: " << s.overflows
             << "  |  Instances on this device: " << s.instancesOnDevice;

        if (s.error.isNotEmpty())
            text << "\nError: " << s.error;
    }

    statusLabel.setText (text, juce::dontSendNotification);
}

void InputTapEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2127));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("F2_InputTap", 14, 8, 300, 24, juce::Justification::centredLeft);
}

void InputTapEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (30);

    auto row = r.removeFromTop (26);
    deviceLabel.setBounds (row.removeFromLeft (90));
    reconnectButton.setBounds (row.removeFromRight (80));
    row.removeFromRight (6);
    rescanButton.setBounds (row.removeFromRight (64));
    row.removeFromRight (6);
    deviceBox.setBounds (row);

    auto layoutRow = [&r] (juce::Label& label, juce::Slider& slider, juce::TextButton& reset)
    {
        auto line = r.removeFromTop (26);
        label.setBounds (line.removeFromLeft (90));
        reset.setBounds (line.removeFromRight (64));
        line.removeFromRight (6);
        slider.setBounds (line);
        r.removeFromTop (6);
    };

    r.removeFromTop (6);
    layoutRow (gainLabel,    gainSlider,    gainReset);
    layoutRow (latencyLabel, latencySlider, latencyReset);
    layoutRow (panLabel,     panSlider,     panReset);

    r.removeFromTop (8);
    row = r.removeFromTop (24);
    passButton.setBounds (row.removeFromLeft (240));
    muteButton.setBounds (row);

    r.removeFromTop (10);
    statusLabel.setBounds (r);
}
