#include "ChordEditorPanel.h"
#include "Localization.h"

namespace
{
const char* rootNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
} // namespace

ChordEditorPanel::ChordEditorPanel()
{
    clearButton.setColour (juce::TextButton::textColourOffId, theme::danger);
    clearButton.setColour (juce::TextButton::buttonColourId, theme::surfaceHi);
    clearButton.onClick = [this] { clearSlot(); };
    addAndMakeVisible (clearButton);
}

void ChordEditorPanel::setModel (reharm::ProgressionModel* m)
{
    model = m;
    refreshFromSelection();
}

void ChordEditorPanel::setDisplayState (DisplayState* state)
{
    display = state;
    refreshFromSelection();
}

std::optional<reharm::Chord> ChordEditorPanel::currentChord() const
{
    if (model == nullptr || display == nullptr || ! display->hasSelection())
        return std::nullopt;
    return model->getChord (display->selectedBar, display->selectedSlot);
}

void ChordEditorPanel::applyChord (const reharm::Chord& c)
{
    if (model == nullptr || display == nullptr || ! display->hasSelection())
        return;
    model->setChord (display->selectedBar, display->selectedSlot, c);
    rebuildSubChips();
    // Re-layout the freshly rebuilt chips: rebuildSubChips() creates them with
    // empty bounds, and without this call paint() would skip drawing them until
    // the next external resized() (e.g. reselecting the cell).
    resized();
    repaint();
}

void ChordEditorPanel::clearSlot()
{
    if (model == nullptr || display == nullptr || ! display->hasSelection())
        return;
    model->setChord (display->selectedBar, display->selectedSlot, std::nullopt);
    rebuildSubChips();
    resized();
    repaint();
}


void ChordEditorPanel::rebuildSubChips()
{
    subChips.clear();
    auto chord = currentChord();
    if (! chord.has_value() || model == nullptr)
        return;

    auto candidates = reharm::HarmonyAnalyzer::substitutions (*chord, model->getKey());
    // Layout done in resized / paint pass; store candidates, bounds set in resized
    for (auto& c : candidates)
        subChips.push_back ({ std::move (c), {} });
}

void ChordEditorPanel::refreshFromSelection()
{
    rebuildSubChips();
    resized();
    repaint();
}

void ChordEditorPanel::paintPill (juce::Graphics& g, juce::Rectangle<int> r,
                                  const juce::String& label, bool active)
{
    auto rf = r.toFloat();
    g.setColour (active ? theme::accent : theme::surfaceHi);
    g.fillRoundedRectangle (rf, 8.0f);
    if (! active)
    {
        g.setColour (theme::border);
        g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);
    }
    g.setColour (active ? theme::text : theme::textDim);
    g.setFont (theme::font (12.0f, true));
    g.drawText (label, r, juce::Justification::centred, false);
}

void ChordEditorPanel::paintSubChip (juce::Graphics& g, const SubChip& chip)
{
    auto r = chip.bounds.toFloat();
    g.setColour (theme::patternChip.withAlpha (0.18f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (theme::patternChip);
    g.setFont (theme::font (11.0f, true));
    g.drawText (reharm::loc::trLabel (chip.candidate.label), chip.bounds, juce::Justification::centred, false);
}

void ChordEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::surface);
    g.setColour (theme::border);
    g.fillRect (0, 0, getWidth(), 1);

    if (model == nullptr || display == nullptr || ! display->hasSelection())
        return;

    auto chord = currentChord();
    const int bar = display->selectedBar;
    const int slot = display->selectedSlot;

    // Header line
    juce::String header = "Bar " + juce::String (bar + 1)
                          + " / Slot " + juce::String (slot + 1);
    if (chord.has_value())
        header += "  -  " + reharm::ChordModel::chordName (*chord);

    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, true));
    g.drawText (header, 16, 8, getWidth() - 120, 22, juce::Justification::centredLeft, false);

    // Section labels: vertically centred against their pill rows.
    // Row Y / heights must match the layout constants in resized():
    // rootY=44, bassY=74, qualY=104 (row height 18), subY=192 (chip height 22),
    // pillH=24, label height=16.
    auto drawLabel = [&] (const juce::String& t, int x, int y)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f));
        g.drawText (t, x, y, 80, 16, juce::Justification::centredLeft, false);
    };

    const int labelH = 16;
    drawLabel ("ROOT", 16, 44 + (24 - labelH) / 2);      // = 48
    drawLabel ("BASS", 16, 74 + (24 - labelH) / 2);      // = 78
    drawLabel ("QUALITY", 16, 104 + (18 - labelH) / 2);  // = 105, first quality row
    drawLabel ("SUBS", 16, 192 + (22 - labelH) / 2);     // = 195


    const int activeRoot = chord.has_value() ? chord->rootPitchClass : -1;
    const int activeBass = chord.has_value()
                               ? (chord->hasBass() ? chord->bassPitchClass : -1)
                               : -2; // -2 = no chord; -1 = no bass
    const auto activeQ = chord.has_value() ? chord->quality : reharm::ChordQuality::NumQualities;

    for (int i = 0; i < kNumRoots; ++i)
        paintPill (g, rootPills[i], rootNames[i], i == activeRoot);

    for (int i = 0; i < kNumBass; ++i)
    {
        const juce::String label = (i == 0) ? "--" : rootNames[i - 1];
        const bool active = (i == 0) ? (activeBass == -1 && chord.has_value())
                                     : (activeBass == i - 1);
        paintPill (g, bassPills[i], label, active);
    }

    for (int i = 0; i < kNumQualities; ++i)
    {
        const auto q = static_cast<reharm::ChordQuality> (i);
        paintPill (g, qualityPills[i],
                   reharm::ChordModel::qualityDisplayName (q),
                   q == activeQ);
    }

    for (const auto& chip : subChips)
        if (! chip.bounds.isEmpty())
            paintSubChip (g, chip);
}

void ChordEditorPanel::resized()
{
    clearButton.setBounds (getWidth() - 90, 8, 74, 24);

    const int pillH = 24;
    const int rootY = 44;
    const int bassY = 74;
    const int qualY = 104;
    const int subY = 192;

    const int startX = 70;
    const int gap = 4;
    const int avail = juce::jmax (100, getWidth() - startX - 16);

    // Root: 12 equal pills
    {
        const int pw = (avail - gap * 11) / 12;
        for (int i = 0; i < kNumRoots; ++i)
            rootPills[i] = { startX + i * (pw + gap), rootY, pw, pillH };
    }

    // Bass: 13 pills
    {
        const int pw = (avail - gap * 12) / 13;
        for (int i = 0; i < kNumBass; ++i)
            bassPills[i] = { startX + i * (pw + gap), bassY, pw, pillH };
    }

    // Quality: 7 columns x ~4 rows
    {
        const int cols = kQualityCols;
        const int pw = (avail - gap * (cols - 1)) / cols;
        const int ph = 18;
        for (int i = 0; i < kNumQualities; ++i)
        {
            const int row = i / cols;
            const int col = i % cols;
            qualityPills[i] = { startX + col * (pw + gap), qualY + row * (ph + 2), pw, ph };
        }
    }

    // Substitution chips
    {
        int x = startX;
        const int h = 22;
        juce::Font f = theme::font (11.0f, true);
        for (auto& chip : subChips)
        {
            const int tw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (f, reharm::loc::trLabel (chip.candidate.label))) + 16;
            if (x + tw > getWidth() - 16)
                break;
            chip.bounds = { x, subY, tw, h };
            x += tw + 6;
        }
    }
}

void ChordEditorPanel::mouseDown (const juce::MouseEvent& e)
{
    if (model == nullptr || display == nullptr || ! display->hasSelection())
        return;

    auto chord = currentChord();
    // If empty slot somehow selected, seed a default before editing
    if (! chord.has_value())
    {
        const auto& key = model->getKey();
        chord = reharm::Chord { key.tonicPitchClass, reharm::ChordQuality::Major, -1 };
    }

    auto c = *chord;

    for (int i = 0; i < kNumRoots; ++i)
    {
        if (rootPills[i].contains (e.getPosition()))
        {
            c.rootPitchClass = i;
            applyChord (c);
            return;
        }
    }

    for (int i = 0; i < kNumBass; ++i)
    {
        if (bassPills[i].contains (e.getPosition()))
        {
            c.bassPitchClass = (i == 0) ? -1 : (i - 1);
            applyChord (c);
            return;
        }
    }

    for (int i = 0; i < kNumQualities; ++i)
    {
        if (qualityPills[i].contains (e.getPosition()))
        {
            c.quality = static_cast<reharm::ChordQuality> (i);
            applyChord (c);
            return;
        }
    }

    for (const auto& chip : subChips)
    {
        if (chip.bounds.contains (e.getPosition()))
        {
            applyChord (chip.candidate.chord);
            return;
        }
    }
}
