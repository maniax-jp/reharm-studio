#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ProgressionModel.h"
#include "HarmonyAnalyzer.h"

/**
 * Bottom editor for the selected chord: root, bass, quality, substitutions.
 * Collapses to height 0 when nothing is selected (managed by parent layout).
 */
class ChordEditorPanel : public juce::Component
{
public:
    ChordEditorPanel();
    ~ChordEditorPanel() override = default;

    void setModel (reharm::ProgressionModel* model);
    void setDisplayState (DisplayState* state);

    /** Refresh controls and substitution chips from the current selection. */
    void refreshFromSelection();

    std::function<void()> onEdited; // optional; model.onChanged already fires

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    reharm::ProgressionModel* model = nullptr;
    DisplayState* display = nullptr;

    juce::TextButton clearButton { "Clear" };

    static constexpr int kNumRoots = 12;
    static constexpr int kNumBass = 13; // -- + 12
    static constexpr int kNumQualities = static_cast<int> (reharm::ChordQuality::NumQualities);
    static constexpr int kQualityCols = 7;

    juce::Rectangle<int> rootPills[kNumRoots];
    juce::Rectangle<int> bassPills[kNumBass];
    juce::Rectangle<int> qualityPills[kNumQualities];

    struct SubChip
    {
        reharm::SubstitutionCandidate candidate;
        juce::Rectangle<int> bounds;
    };
    std::vector<SubChip> subChips;

    std::optional<reharm::Chord> currentChord() const;
    void applyChord (const reharm::Chord& c);
    void clearSlot();
    void rebuildSubChips();

    void paintPill (juce::Graphics& g, juce::Rectangle<int> r,
                    const juce::String& label, bool active);
    void paintSubChip (juce::Graphics& g, const SubChip& chip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordEditorPanel)
};
