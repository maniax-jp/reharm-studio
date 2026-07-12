#include "AnalysisStrip.h"
#include "Localization.h"

namespace
{
class ChipPainter : public juce::Component
{
public:
    std::vector<std::pair<juce::String, bool>> items; // text, emphasize

    void paint (juce::Graphics& g) override
    {
        int x = 12;
        const int y = 8;
        const int h = 24;
        const int gap = 8;

        if (items.empty())
        {
            g.setColour (theme::textDim);
            g.setFont (theme::font (12.0f));
            g.drawText (reharm::loc::tr ("No pattern detected"), 12, 0, getWidth() - 24, getHeight(),
                        juce::Justification::centredLeft, false);
            return;
        }

        for (const auto& item : items)
        {
            g.setFont (theme::font (11.0f, true));
            const int tw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), item.first)) + 20;
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) tw, (float) h);

            if (item.second)
            {
                g.setColour (theme::accent);
                g.fillRoundedRectangle (r, 10.0f);
                g.setColour (theme::text);
            }
            else
            {
                g.setColour (theme::patternChip.withAlpha (0.18f));
                g.fillRoundedRectangle (r, 10.0f);
                g.setColour (theme::patternChip);
            }

            g.drawText (item.first, r.toNearestInt(), juce::Justification::centred, false);
            x += tw + gap;
        }
    }
};
} // namespace

AnalysisStrip::AnalysisStrip()
{
    auto* painter = new ChipPainter();
    content.setInterceptsMouseClicks (false, false);
    viewport.setViewedComponent (painter, true);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
}

void AnalysisStrip::setPatterns (const std::vector<reharm::DetectedPattern>& patterns,
                                 const reharm::ProgressionModel& model)
{
    chips.clear();

    const auto flat = model.flatten();

    auto barOfFlat = [&] (int flatIndex) -> int
    {
        if (flatIndex < 0 || flatIndex >= (int) flat.size())
            return 1;
        return flat[(size_t) flatIndex].barIndex + 1; // 1-based for display
    };

    // Collect preset names for emphasis detection
    std::vector<juce::String> presetNames;
    for (const auto& p : reharm::ProgressionPresets::all())
        presetNames.push_back (p.name);

    std::vector<Chip> normal;
    std::vector<Chip> emphasized;

    for (const auto& pat : patterns)
    {
        Chip c;
        const int barStart = barOfFlat (pat.startIndex);
        const int barEnd = barOfFlat (pat.endIndex);

        if (barStart == barEnd)
            c.text = reharm::loc::tr (pat.name) + " (bar " + juce::String (barStart) + ")";
        else
            c.text = reharm::loc::tr (pat.name) + " (bars " + juce::String (barStart) + "-" + juce::String (barEnd) + ")";

        c.emphasize = std::find (presetNames.begin(), presetNames.end(), pat.name) != presetNames.end();

        if (c.emphasize)
            emphasized.push_back (std::move (c));
        else
            normal.push_back (std::move (c));
    }

    // Emphasized (full preset match) first
    chips = std::move (emphasized);
    chips.insert (chips.end(), normal.begin(), normal.end());

    layoutChips();
    repaint();
}

void AnalysisStrip::layoutChips()
{
    auto* painter = dynamic_cast<ChipPainter*> (viewport.getViewedComponent());
    if (painter == nullptr)
        return;

    painter->items.clear();
    int totalW = 24;

    juce::Font f = theme::font (11.0f, true);
    for (const auto& c : chips)
    {
        painter->items.push_back ({ c.text, c.emphasize });
        totalW += juce::roundToInt (juce::GlyphArrangement::getStringWidth (f, c.text)) + 20 + 8;
    }

    if (chips.empty())
        totalW = juce::jmax (200, getWidth());

    const int h = juce::jmax (40, getHeight());
    painter->setSize (juce::jmax (totalW, viewport.getWidth()), h);
    painter->repaint();
}

void AnalysisStrip::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    g.setColour (theme::border);
    g.fillRect (0, 0, getWidth(), 1);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void AnalysisStrip::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutChips();
}
