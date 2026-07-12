#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ProgressionModel.h"

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

    std::function<void (int tonicPitchClass, bool isMajor)> onKeyChanged;
    std::function<void (const reharm::PresetProgression&)> onPresetSelected;
    std::function<void (bool closeVoicing)> onVoicingChanged;


    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    juce::ComboBox presetBox;
    SegmentedToggle voicingToggle { "Close", "Open" };


    void handleKeyOrScaleChanged();
    void handlePresetChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
