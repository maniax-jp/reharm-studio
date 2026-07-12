#include "SequencerView.h"
#include "Localization.h"

SequencerView::SequencerView()
{
    setOpaque (true);
}

void SequencerView::setModel (reharm::ProgressionModel* m)
{
    model = m;
    recomputeLayout();
    repaint();
}

void SequencerView::setDisplayState (DisplayState* state)
{
    display = state;
    repaint();
}

void SequencerView::setAnalyses (const std::vector<reharm::ChordAnalysis>& a)
{
    analyses = a;
    repaint();
}

void SequencerView::setPlayingFlatIndex (int flatIndex)
{
    if (playingFlatIndex != flatIndex)
    {
        playingFlatIndex = flatIndex;
        repaint();
    }
}

int SequencerView::flatIndexFor (int bar, int slot) const
{
    if (model == nullptr)
        return -1;

    int idx = 0;
    for (int b = 0; b < model->getNumBars(); ++b)
    {
        const int n = model->getBar (b).numSlots();
        if (b == bar)
        {
            if (slot < 0 || slot >= n)
                return -1;
            return idx + slot;
        }
        idx += n;
    }
    return -1;
}

void SequencerView::recomputeLayout()
{
    auto area = getLocalBounds().reduced (kPad);
    if (area.isEmpty())
        return;

    const int colW = (area.getWidth() - kGap * (kCols - 1)) / kCols;
    const int rowH = (area.getHeight() - kGap * (kRows - 1)) / kRows;

    for (int i = 0; i < kMaxBars; ++i)
    {
        const int row = i / kCols;
        const int col = i % kCols;
        const int x = area.getX() + col * (colW + kGap);
        const int y = area.getY() + row * (rowH + kGap);
        barBounds[i] = { x, y, colW, rowH };

        // Header row for bar number + subdivision
        auto barInner = barBounds[i].reduced (10, 8);
        auto header = barInner.removeFromTop (22);

        // Subdivision segments on the right of header: three 16x20 boxes
        const int segW = 16;
        const int segH = 20;
        const int segGap = 2;
        const int segsTotal = segW * 3 + segGap * 2;
        auto segArea = header.removeFromRight (segsTotal).withSizeKeepingCentre (segsTotal, segH);
        for (int s = 0; s < 3; ++s)
        {
            subBounds[i][s] = { segArea.getX() + s * (segW + segGap), segArea.getY(), segW, segH };
        }

        // Chord cells
        barInner.removeFromTop (6);
        const int numSlots = (model != nullptr && i < model->getNumBars())
                                 ? model->getBar (i).numSlots()
                                 : 1;
        const int cellGap = 6;
        const int cellW = numSlots > 0
                              ? (barInner.getWidth() - cellGap * (numSlots - 1)) / numSlots
                              : barInner.getWidth();

        for (int s = 0; s < 4; ++s)
        {
            if (s < numSlots)
                cellBounds[i][s] = { barInner.getX() + s * (cellW + cellGap),
                                     barInner.getY(), cellW, barInner.getHeight() };
            else
                cellBounds[i][s] = {};
        }
    }
}

void SequencerView::resized()
{
    recomputeLayout();
}

void SequencerView::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    if (model == nullptr)
        return;

    recomputeLayout();

    const int numBars = model->getNumBars();

    for (int i = 0; i < kMaxBars; ++i)
    {
        const bool ghost = i >= numBars;
        paintBar (g, i, barBounds[i], ghost);
    }
}

void SequencerView::paintBar (juce::Graphics& g, int barIndex,
                              juce::Rectangle<int> bounds, bool ghost)
{
    juce::Graphics::ScopedSaveState ss (g);
    if (ghost)
        g.setOpacity (0.5f);

    // Card background
    g.setColour (theme::surface);
    g.fillRoundedRectangle (bounds.toFloat(), 12.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 12.0f, 1.0f);

    // Bar number
    auto header = bounds.reduced (10, 8).removeFromTop (22);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText (juce::String (barIndex + 1), header.removeFromLeft (24),
                juce::Justification::centredLeft, false);

    // Subdivision [1|2|4]
    static const int subValues[3] = { 1, 2, 4 };
    const int currentSub = (! ghost && model != nullptr)
                               ? model->getBar (barIndex).numSlots()
                               : 1;

    for (int s = 0; s < 3; ++s)
    {
        auto r = subBounds[barIndex][s].toFloat();
        const bool active = (subValues[s] == currentSub) && ! ghost;
        g.setColour (active ? theme::accent : theme::surfaceHi);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (active ? theme::text : theme::textDim);
        g.setFont (theme::font (10.0f, true));
        g.drawText (juce::String (subValues[s]), r.toNearestInt(),
                    juce::Justification::centred, false);
    }

    if (ghost)
    {
        // Ghost: single empty "+" cell
        auto barInner = bounds.reduced (10, 8);
        barInner.removeFromTop (28);
        g.setColour (theme::textDim.withAlpha (0.6f));
        g.setFont (theme::font (22.0f));
        g.drawText ("+", barInner, juce::Justification::centred, false);

        // Dashed border for ghost cell area
        juce::PathStrokeType stroke (1.0f);
        float dashes[] = { 4.0f, 4.0f };
        juce::Path path;
        path.addRoundedRectangle (barInner.toFloat().reduced (2.0f), 8.0f);
        g.setColour (theme::border.brighter (0.3f));
        stroke.createDashedStroke (path, path, dashes, 2);
        g.strokePath (path, juce::PathStrokeType (1.0f));
        return;
    }

    const int numSlots = model->getBar (barIndex).numSlots();
    for (int s = 0; s < numSlots; ++s)
        paintCell (g, barIndex, s, cellBounds[barIndex][s], false);
}

void SequencerView::paintCell (juce::Graphics& g, int barIndex, int slotIndex,
                               juce::Rectangle<int> bounds, bool /*ghost*/)
{
    if (bounds.isEmpty() || model == nullptr)
        return;

    const auto chord = model->getChord (barIndex, slotIndex);
    const bool selected = display != nullptr
                          && display->selectedBar == barIndex
                          && display->selectedSlot == slotIndex;
    const int flatIdx = flatIndexFor (barIndex, slotIndex);
    const bool isPlaying = (playingFlatIndex >= 0 && flatIdx == playingFlatIndex);

    auto r = bounds.toFloat();

    // Soft glow for playing cell (outer translucent strokes)
    if (isPlaying)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (theme::playing.withAlpha (0.12f * (float) (4 - i)));
            g.drawRoundedRectangle (r.expanded ((float) i * 1.5f), 8.0f + (float) i, 2.0f);
        }
    }

    if (! chord.has_value())
    {
        // Empty: dashed border + "+"
        g.setColour (theme::surfaceHi.withAlpha (0.4f));
        g.fillRoundedRectangle (r, 8.0f);

        juce::Path path;
        path.addRoundedRectangle (r.reduced (1.0f), 8.0f);
        float dashes[] = { 4.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, path, dashes, 2);
        g.setColour (selected ? theme::accent : theme::textDim.withAlpha (0.5f));
        g.strokePath (dashed, juce::PathStrokeType (selected ? 2.0f : 1.0f));

        g.setColour (theme::textDim);
        g.setFont (theme::font (20.0f));
        g.drawText ("+", bounds, juce::Justification::centred, false);
        return;
    }

    // Filled cell background
    g.setColour (selected ? theme::accentSoft : theme::surfaceHi);
    g.fillRoundedRectangle (r, 8.0f);

    // Non-diatonic top line
    const reharm::ChordAnalysis* analysis = nullptr;
    if (flatIdx >= 0 && flatIdx < (int) analyses.size())
        analysis = &analyses[(size_t) flatIdx];

    if (analysis != nullptr && ! analysis->diatonic)
    {
        g.setColour (theme::nonDiatonic);
        g.fillRoundedRectangle (r.getX(), r.getY(), r.getWidth(), 3.0f, 1.5f);
        // Clean top of rounded rect: just a 3px strip
        g.fillRect (r.getX() + 4.0f, r.getY(), r.getWidth() - 8.0f, 3.0f);
    }

    // Selection / playing border
    if (isPlaying)
    {
        g.setColour (theme::playing);
        g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 2.0f);
    }
    else if (selected)
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 2.0f);
    }

    const auto& key = model->getKey();
    const bool showDegree = display != nullptr && display->showDegree;
    const juce::String name = reharm::ChordModel::chordName (*chord);
    const juce::String degree = reharm::ChordModel::degreeName (*chord, key);
    const juce::String primary = showDegree ? degree : name;
    const juce::String secondary = showDegree ? name : degree;

    // Primary chord label
    g.setColour (theme::text);
    g.setFont (theme::font (22.0f, true));
    auto textArea = bounds.reduced (6, 8);
    g.drawFittedText (primary, textArea.removeFromTop (textArea.getHeight() / 2 + 4),
                      juce::Justification::centred, 1);

    // Secondary
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawFittedText (secondary, textArea, juce::Justification::centred, 1);

    // Non-diatonic badge (top-left)
    if (analysis != nullptr && ! analysis->diatonic && analysis->label.isNotEmpty())
    {
        g.setFont (theme::font (11.0f, true));
        const int bw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), reharm::loc::trLabel (analysis->label))) + 10;
        const int bh = 16;
        auto badge = juce::Rectangle<float> (r.getX() + 4.0f, r.getY() + 6.0f, (float) bw, (float) bh);
        g.setColour (theme::nonDiatonic.withAlpha (0.20f));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (theme::nonDiatonic);
        g.drawText (reharm::loc::trLabel (analysis->label), badge.toNearestInt(), juce::Justification::centred, false);
    }
}

void SequencerView::hitTest (juce::Point<int> pos)
{
    if (model == nullptr)
        return;

    const int numBars = model->getNumBars();

    for (int i = 0; i < kMaxBars; ++i)
    {
        if (! barBounds[i].contains (pos))
            continue;

        // Ghost bar: activate up to this bar
        if (i >= numBars)
        {
            model->setNumBars (i + 1);
            return;
        }

        // Subdivision segments
        static const reharm::BarSubdivision subs[3] = {
            reharm::BarSubdivision::One,
            reharm::BarSubdivision::Two,
            reharm::BarSubdivision::Four
        };
        for (int s = 0; s < 3; ++s)
        {
            if (subBounds[i][s].contains (pos))
            {
                model->setSubdivision (i, subs[s]);
                return;
            }
        }

        // Chord cells
        const int n = model->getBar (i).numSlots();
        for (int s = 0; s < n; ++s)
        {
            if (! cellBounds[i][s].contains (pos))
                continue;

            auto existing = model->getChord (i, s);
            if (! existing.has_value())
            {
                // Place default I Major (tonic of current key)
                const auto& key = model->getKey();
                model->setChord (i, s, reharm::Chord { key.tonicPitchClass,
                                                       reharm::ChordQuality::Major, -1 });
            }

            if (onCellSelected)
                onCellSelected (i, s);
            return;
        }

        return;
    }
}

void SequencerView::mouseDown (const juce::MouseEvent& e)
{
    hitTest (e.getPosition());
}
