#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ProgressionModel.h"
#include "PlaybackSettings.h"

/**
 * Top bar: title, key/scale, preset picker, Close|Open toggle.
 */

class HeaderBar : public juce::Component
{
public:
    HeaderBar();
    ~HeaderBar() override = default;

    void setKey (int tonicPitchClass, bool isMajor);
    void setVoicingClose (bool close);
    void setPlayStyle (reharm::ArpPattern pattern);
    void setArpRate (reharm::ArpRate rate);

    /** Rebuilds the presetBox with the built-in presets plus a "User" section
        listing the given names (id = 1000 + index into names). */
    void setUserPresets (const juce::StringArray& names);

    std::function<void (int tonicPitchClass, bool isMajor)> onKeyChanged;
    std::function<void (const reharm::PresetProgression&)> onPresetSelected;
    std::function<void (bool closeVoicing)> onVoicingChanged;
    std::function<void (const juce::String& name)> onUserPresetSelected;
    std::function<void()> onSaveRequested;
    std::function<void (reharm::ArpPattern)> onPlayStyleChanged;
    std::function<void (reharm::ArpRate)> onArpRateChanged;


    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    juce::ComboBox presetBox;
    juce::TextButton saveButton;
    juce::StringArray userPresetNames;
    SegmentedToggle voicingToggle { "Close", "Open" };
    juce::ComboBox playStyleBox;
    juce::ComboBox arpRateBox;


    void handleKeyOrScaleChanged();
    void handlePresetChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
