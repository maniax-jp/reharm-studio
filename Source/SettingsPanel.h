#pragma once

#include <JuceHeader.h>
#include "Theme.h"

/**
 * Contents of the settings screen, opened from the header's gear button or the
 * application menu. UI only: a pick is reported through onMidiInputSelected and
 * comes back through setMidiInputs, so the combo box never shows a selection
 * the app does not actually have.
 */
class SettingsPanel : public juce::Component,
                      private juce::Timer
{
public:
    SettingsPanel();
    ~SettingsPanel() override = default;

    /** devices: inputs currently connected. selectedIdentifier: the input that
        is actually open (empty when none is). selectedName: what the user
        picked — kept visible, greyed out, when that device is not connected
        any more, so the choice is not silently downgraded to "off". */
    void setMidiInputs (const juce::Array<juce::MidiDeviceInfo>& devices,
                        const juce::String& selectedIdentifier,
                        const juce::String& selectedName);

    /** An empty identifier means the user picked "no MIDI input". */
    std::function<void (const juce::String& identifier, const juce::String& name)> onMidiInputSelected;

    /** Asks for the device list again, so a keyboard plugged in while this
        screen is open shows up on its own. */
    std::function<void()> onRefreshRequested;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void handleSelectionChanged();

    /** Index into midiDevices, or -1 when the identifier is not connected. */
    int indexOfIdentifier (const juce::String& identifier) const;

    juce::Label midiCaption;
    juce::ComboBox midiInputBox;

    juce::Array<juce::MidiDeviceInfo> midiDevices;
    juce::String selectedIdentifier;
    juce::String selectedName;
    bool listPopulated = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
