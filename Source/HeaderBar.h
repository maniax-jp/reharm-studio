#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ProgressionModel.h"
#include "PlaybackSettings.h"

/**
 * Top bar: title, key/scale, preset picker, play-mode menu.
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

    /** Stores user preset names for the next preset popup menu.
        Built-in presets are always listed; user names appear under a "User" section. */
    void setUserPresets (const juce::StringArray& names);

    std::function<void (int tonicPitchClass, bool isMajor)> onKeyChanged;
    std::function<void (const reharm::PresetProgression&)> onPresetSelected;
    std::function<void (bool closeVoicing)> onVoicingChanged;
    std::function<void (const juce::String& name)> onUserPresetSelected;
    std::function<void (const juce::String& name)> onUserPresetDeleteRequested;
    std::function<void()> onSaveRequested;
    std::function<void (reharm::ArpPattern)> onPlayStyleChanged;
    std::function<void (reharm::ArpRate)> onArpRateChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    juce::TextButton presetButton;
    juce::TextButton saveButton;
    juce::TextButton playModeButton;
    juce::StringArray userPresetNames;
    juce::String pendingDeleteName;

    bool voicingClose = true;
    reharm::ArpPattern pattern = reharm::ArpPattern::Off;
    reharm::ArpRate rate = reharm::ArpRate::Eighth;

    void handleKeyOrScaleChanged();
    void showPresetMenu();
    void handlePresetMenuResult (int resultId);
    void showPlayModeMenu();
    void handlePlayModeMenuResult (int resultId);
    void updatePlayModeButtonLabel();
    static juce::String rateLabel (reharm::ArpRate r);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
