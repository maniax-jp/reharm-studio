#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "DisplayState.h"
#include "ProgressionModel.h"
#include "HarmonyAnalyzer.h"

/**
 * 2x4 grid of bar cards with chord cells. Paints all content; hit-tests clicks.
 */
class SequencerView : public juce::Component
{
public:
    SequencerView();
    ~SequencerView() override = default;

    void setModel (reharm::ProgressionModel* model);
    void setDisplayState (DisplayState* state);
    void setAnalyses (const std::vector<reharm::ChordAnalysis>& analyses);
    void setPlayingFlatIndex (int flatIndex);

    /** Mark layout bounds dirty; recomputed lazily on next paint. */
    void markLayoutDirty() noexcept { layoutDirty = true; }

    /** Fired when a cell is clicked (after empty-cell default chord placement if needed). */
    std::function<void (int bar, int slot)> onCellSelected;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void resized() override;

private:
    reharm::ProgressionModel* model = nullptr;
    DisplayState* display = nullptr;
    std::vector<reharm::ChordAnalysis> analyses;
    int playingFlatIndex = -1;
    bool layoutDirty = true;

    static constexpr int kMaxBars = reharm::ProgressionModel::maxBars;
    static constexpr int kCols = 4;
    static constexpr int kRows = 2;
    static constexpr int kGap = 12;
    static constexpr int kPad = 16;

    juce::Rectangle<int> barBounds[kMaxBars];
    juce::Rectangle<int> cellBounds[kMaxBars][4];
    juce::Rectangle<int> subBounds[kMaxBars][3]; // [1|2|4] segments

    void recomputeLayout();
    void paintBar (juce::Graphics& g, int barIndex, juce::Rectangle<int> bounds, bool ghost);
    void paintCell (juce::Graphics& g, int barIndex, int slotIndex,
                    juce::Rectangle<int> bounds, bool ghost);
    int flatIndexFor (int bar, int slot) const;
    void hitTest (juce::Point<int> pos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerView)
};
