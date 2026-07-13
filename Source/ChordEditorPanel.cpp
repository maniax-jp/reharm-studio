#include "ChordEditorPanel.h"
#include "Localization.h"

namespace
{
const char* rootNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

void paintOptionPill (juce::Graphics& g, juce::Rectangle<int> r,
                      const juce::String& label, bool active)
{
    auto rf = r.toFloat();
    g.setColour (active ? theme::accent : theme::surfaceHi);
    g.fillRoundedRectangle (rf, 8.0f);
    if (!active)
    {
        g.setColour (theme::border);
        g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);
    }
    g.setColour (active ? theme::text : theme::textDim);
    g.setFont (theme::font (12.0f, true));
    g.drawText (label, r, juce::Justification::centred, false);
}

class OptionsPanel : public juce::Component
{
public:
    explicit OptionsPanel (juce::Component::SafePointer<ChordEditorPanel> owner)
        : owner (owner)
    {
        layoutPills();
        setSize (424, 112);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::surfaceHi);

        auto chordOpt = owner ? owner->currentChordForOptions() : std::nullopt;
        if (!chordOpt.has_value())
        {
            drawLabels (g);
            return;
        }

        const auto& chord = *chordOpt;
        const auto avail = reharm::ChordModel::optionAvailability (chord);

        drawLabels (g);

        for (int i = 0; i < 5; ++i)
        {
            const auto& pill = omitAlterPills[i];
            const bool active = isActive (chord, i, false);
            const bool available = isAvailable (avail, i, false);
            const bool disabled = !active && !available;

            g.beginTransparencyLayer (disabled ? 0.3f : 1.0f);
            paintOptionPill (g, pill.bounds, pill.label, active);
            g.endTransparencyLayer();
        }

        for (int i = 0; i < 7; ++i)
        {
            const auto& pill = addPills[i];
            const bool active = isActive (chord, i, true);
            const bool available = isAvailable (avail, i, true);
            const bool disabled = !active && !available;

            g.beginTransparencyLayer (disabled ? 0.3f : 1.0f);
            paintOptionPill (g, pill.bounds, pill.label, active);
            g.endTransparencyLayer();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (!owner) return;

        auto chordOpt = owner->currentChordForOptions();
        if (!chordOpt.has_value()) return;

        auto c = *chordOpt;
        const auto avail = reharm::ChordModel::optionAvailability (c);

        for (int i = 0; i < 5; ++i)
        {
            if (omitAlterPills[i].bounds.contains (e.getPosition()))
            {
                const bool wasActive = isActive (c, i, false);
                const bool available = isAvailable (avail, i, false);

                if (!wasActive && !available) return;

                toggleOption (c, i, false);
                reharm::ChordModel::sanitizeOptions (c);
                owner->applyChordFromOptions (c);
                repaint();
                return;
            }
        }

        for (int i = 0; i < 7; ++i)
        {
            if (addPills[i].bounds.contains (e.getPosition()))
            {
                const bool wasActive = isActive (c, i, true);
                const bool available = isAvailable (avail, i, true);

                if (!wasActive && !available) return;

                toggleOption (c, i, true);
                reharm::ChordModel::sanitizeOptions (c);
                owner->applyChordFromOptions (c);
                repaint();
                return;
            }
        }
    }

private:
    juce::Component::SafePointer<ChordEditorPanel> owner;

    struct OptionPill
    {
        juce::String label;
        juce::Rectangle<int> bounds;
    };

    OptionPill omitAlterPills[5];
    OptionPill addPills[7];

    void layoutPills()
    {
        const int padX = 16;
        const int pillW = 52;
        const int pillH = 22;
        const int pillGap = 4;

        const char* omitLabels[] = { "omit3", "omit5", "omit7", "b5", "#5" };
        const int row1Y = 26;
        for (int i = 0; i < 5; ++i)
            omitAlterPills[i] = { omitLabels[i],
                { padX + i * (pillW + pillGap), row1Y, pillW, pillH } };

        const char* addLabels[] = { "b9", "9", "#9", "11", "#11", "b13", "13" };
        const int row2Y = 76;
        for (int i = 0; i < 7; ++i)
            addPills[i] = { addLabels[i],
                { padX + i * (pillW + pillGap), row2Y, pillW, pillH } };
    }

    void drawLabels (juce::Graphics& g)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f));
        g.drawText ("OMIT / ALTER", 16, 8, getWidth() - 32, 16,
                     juce::Justification::centredLeft, false);
        g.drawText ("ADD", 16, 58, getWidth() - 32, 16,
                     juce::Justification::centredLeft, false);
    }

    static bool isActive (const reharm::Chord& chord, int index, bool isAdd)
    {
        if (isAdd)
            return (chord.addMask & (1 << index)) != 0;

        switch (index)
        {
            case 0: return (chord.omitMask & reharm::Omit3) != 0;
            case 1: return (chord.omitMask & reharm::Omit5) != 0;
            case 2: return (chord.omitMask & reharm::Omit7) != 0;
            case 3: return chord.fifthAlt == reharm::FifthAlt::Flat;
            case 4: return chord.fifthAlt == reharm::FifthAlt::Sharp;
            default: return false;
        }
    }

    static bool isAvailable (const reharm::ChordModel::OptionAvailability& avail,
                              int index, bool isAdd)
    {
        if (isAdd)
            return avail.add[index];

        switch (index)
        {
            case 0: return avail.omit3;
            case 1: return avail.omit5;
            case 2: return avail.omit7;
            case 3: return avail.flat5;
            case 4: return avail.sharp5;
            default: return false;
        }
    }

    static void toggleOption (reharm::Chord& chord, int index, bool isAdd)
    {
        if (isAdd)
        {
            chord.addMask ^= (1 << index);
            return;
        }

        switch (index)
        {
            case 0: chord.omitMask ^= reharm::Omit3; break;
            case 1: chord.omitMask ^= reharm::Omit5; break;
            case 2: chord.omitMask ^= reharm::Omit7; break;
            case 3:
                chord.fifthAlt = (chord.fifthAlt == reharm::FifthAlt::Flat)
                                  ? reharm::FifthAlt::None
                                  : reharm::FifthAlt::Flat;
                break;
            case 4:
                chord.fifthAlt = (chord.fifthAlt == reharm::FifthAlt::Sharp)
                                  ? reharm::FifthAlt::None
                                  : reharm::FifthAlt::Sharp;
                break;
            default: break;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionsPanel)
};
} // namespace

ChordEditorPanel::ChordEditorPanel()
{
    clearButton.setColour (juce::TextButton::textColourOffId, theme::danger);
    clearButton.setColour (juce::TextButton::buttonColourId, theme::surfaceHi);
    clearButton.onClick = [this] { clearSlot(); };
    addAndMakeVisible (clearButton);

    optionsButton.setColour (juce::TextButton::textColourOffId, theme::text);
    optionsButton.setColour (juce::TextButton::buttonColourId, theme::surfaceHi);
    optionsButton.onClick = [this]
    {
        auto content = std::make_unique<OptionsPanel> (
            juce::Component::SafePointer<ChordEditorPanel> { this });
        juce::CallOutBox::launchAsynchronously (std::move (content),
            optionsButton.getScreenBounds(), nullptr);
    };
    addAndMakeVisible (optionsButton);
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

std::optional<reharm::Chord> ChordEditorPanel::currentChordForOptions() const
{
    return currentChord();
}

void ChordEditorPanel::applyChord (const reharm::Chord& c)
{
    if (model == nullptr || display == nullptr || ! display->hasSelection())
        return;
    model->setChord (display->selectedBar, display->selectedSlot, c);
    rebuildSubChips();
    updateOptionsButton();
    // Re-layout the freshly rebuilt chips: rebuildSubChips() creates them with
    // empty bounds, and without this call paint() would skip drawing them until
    // the next external resized() (e.g. reselecting the cell).
    resized();
    repaint();
}

void ChordEditorPanel::applyChordFromOptions (const reharm::Chord& c)
{
    applyChord (c);
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
    for (auto& c : candidates)
        subChips.push_back ({ std::move (c), {} });
}

void ChordEditorPanel::refreshFromSelection()
{
    rebuildSubChips();
    updateOptionsButton();
    resized();
    repaint();
}

void ChordEditorPanel::updateOptionsButton()
{
    auto chord = currentChord();
    optionsButton.setEnabled (chord.has_value());
    optionsButton.setColour (juce::TextButton::textColourOffId,
                             chord.has_value() && chord->hasOptions() ? theme::accent
                                                                      : theme::text);
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

    juce::String header = "Bar " + juce::String (bar + 1)
                          + " / Slot " + juce::String (slot + 1);
    if (chord.has_value())
        header += "  -  " + reharm::ChordModel::chordName (*chord);

    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, true));
    g.drawText (header, 16, 8, getWidth() - 120, 22, juce::Justification::centredLeft, false);

    // Section labels for ROOT, BASS, SUBS
    auto drawLabel = [&] (const juce::String& t, int x, int y)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f));
        g.drawText (t, x, y, 80, 16, juce::Justification::centredLeft, false);
    };

    const int labelH = 16;
    drawLabel ("ROOT", 16, 44 + (24 - labelH) / 2);
    drawLabel ("BASS", 16, 74 + (24 - labelH) / 2);
    drawLabel ("SUBS", 16, 192 + (22 - labelH) / 2);

    // Quality group labels (DYAD, TRIAD, 7TH)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f));
        const auto& groups = reharm::ChordModel::qualityGroups();
        int row = 0;
        for (const auto& group : groups)
        {
            const int y = 104 + row * 20;
            g.drawText (group.label, 16, y + (18 - labelH) / 2, 50, labelH,
                        juce::Justification::centredLeft, false);
            ++row;
        }
    }

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

    for (const auto& [q, r] : qualityPills)
        paintPill (g, r, reharm::ChordModel::qualityDisplayName (q), q == activeQ);

    for (const auto& chip : subChips)
        if (! chip.bounds.isEmpty())
            paintSubChip (g, chip);
}

void ChordEditorPanel::resized()
{
    optionsButton.setBounds (getWidth() - 170, 8, 74, 24);
    clearButton.setBounds (getWidth() - 90, 8, 74, 24);

    const int pillH = 24;
    const int rootY = 44;
    const int bassY = 74;
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

    // Quality: grouped rows (DYAD / TRIAD / 7TH) with unified pill width
    {
        const int qualStartY = 104;
        const int rowGap = 20;
        const int ph = 18;
        const int maxInRow = 13;
        const int pw = (avail - gap * (maxInRow - 1)) / maxInRow;

        qualityPills.clear();
        const auto& groups = reharm::ChordModel::qualityGroups();
        int row = 0;
        for (const auto& group : groups)
        {
            const int y = qualStartY + row * rowGap;
            int col = 0;
            for (const auto& q : group.qualities)
            {
                const int x = startX + col * (pw + gap);
                qualityPills.push_back ({ q, { x, y, pw, ph } });
                ++col;
            }
            ++row;
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

    for (const auto& [q, r] : qualityPills)
    {
        if (r.contains (e.getPosition()))
        {
            c.quality = q;
            reharm::ChordModel::sanitizeOptions (c);
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
