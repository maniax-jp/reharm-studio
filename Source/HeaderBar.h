#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ProgressionModel.h"
#include "PlaybackSettings.h"

/**
 * Top bar: title, key/scale, preset picker.
 */

class HeaderBar : public juce::Component
{
public:
    HeaderBar();
    ~HeaderBar() override = default;

    void setKey (int tonicPitchClass, bool isMajor);

    /** Stores user preset names for the next preset popup menu.
        Built-in presets are always listed; user names appear under a "User" section. */
    void setUserPresets (const juce::StringArray& names);

    std::function<void (int tonicPitchClass, bool isMajor)> onKeyChanged;
    std::function<void (const reharm::PresetProgression&)> onPresetSelected;
    std::function<void (const juce::String& name)> onUserPresetSelected;
    std::function<void (const juce::String& name)> onUserPresetDeleteRequested;
    std::function<void()> onSaveRequested;
    std::function<void()> onSettingsRequested;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    ComboButton presetButton;
    ComboButton saveButton;
    GearButton settingsButton;
    juce::StringArray userPresetNames;
    juce::String pendingDeleteName;

    void handleKeyOrScaleChanged();
    void showPresetMenu();
    void handlePresetMenuResult (int resultId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
