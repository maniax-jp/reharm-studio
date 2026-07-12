#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "HarmonyAnalyzer.h"

/**
 * Horizontal strip of pattern-detection chips.
 */
class AnalysisStrip : public juce::Component
{
public:
    AnalysisStrip();
    ~AnalysisStrip() override = default;

    /** Update chip list. Converts flat indices to bar numbers using the model. */
    void setPatterns (const std::vector<reharm::DetectedPattern>& patterns,
                      const reharm::ProgressionModel& model);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Chip
    {
        juce::String text;
        bool emphasize = false; // accent fill for full-preset match
        juce::Rectangle<int> bounds;
    };

    std::vector<Chip> chips;
    juce::Viewport viewport;
    juce::Component content;

    void layoutChips();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisStrip)
};
