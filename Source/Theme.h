#pragma once

#include <JuceHeader.h>
#include "ChordModel.h"

namespace theme
{

inline const juce::Colour bg          { 0xff0e1014 };
inline const juce::Colour surface     { 0xff161a22 };
inline const juce::Colour surfaceHi   { 0xff1e2430 };
inline const juce::Colour border      { 0x14ffffff };
inline const juce::Colour text        { 0xffe8eaf0 };
inline const juce::Colour textDim     { 0xff8a93a6 };
inline const juce::Colour accent      { 0xff7c6cff };
inline const juce::Colour accentSoft  { 0x337c6cff };
inline const juce::Colour playing     { 0xff41e0b8 };
inline const juce::Colour nonDiatonic { 0xffffb454 };
inline const juce::Colour patternChip { 0xff4a90e2 };
inline const juce::Colour danger      { 0xffff5c7a };

inline juce::Font font (float height, bool bold = false)
{
    auto opts = juce::FontOptions (height);
    if (bold)
        opts = opts.withStyle ("Bold");
    return juce::Font (opts);
}

} // namespace theme

/** Shared UI display / selection state owned by MainComponent. */
struct DisplayState
{
    reharm::Voicing::Style voicingStyle
 = reharm::Voicing::Style::Close;
    int selectedBar = -1;
    int selectedSlot = -1;

    bool hasSelection() const noexcept { return selectedBar >= 0 && selectedSlot >= 0; }

    void clearSelection() noexcept
    {
        selectedBar = -1;
        selectedSlot = -1;
    }
};

/**
 * Custom LookAndFeel for Studio Noir: rounded ComboBox / TextButton / Slider / PopupMenu.
 */
class ReharmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReharmLookAndFeel();
    ~ReharmLookAndFeel() override = default;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g,
                       int width, int height,
                       bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator,
                            bool isActive,
                            bool isHighlighted,
                            bool isTicked,
                            bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReharmLookAndFeel)
};

/**
 * Two-segment pill toggle: left | right.
 * Active segment is filled with accent.
 */
class SegmentedToggle : public juce::Component
{
public:
    SegmentedToggle (juce::String leftLabel, juce::String rightLabel);

    void setLeftActive (bool leftActive);
    bool isLeftActive() const noexcept { return leftIsActive; }

    std::function<void (bool leftActive)> onChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::String leftText;
    juce::String rightText;
    bool leftIsActive = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedToggle)
};
