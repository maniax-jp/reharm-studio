#include "Theme.h"

//==============================================================================
ReharmLookAndFeel::ReharmLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::bg);
    setColour (juce::ComboBox::backgroundColourId, theme::surfaceHi);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);
    setColour (juce::ComboBox::focusedOutlineColourId, theme::accent);

    // Transparent so the popup window itself is created non-opaque; the rounded
    // panel is painted in drawPopupMenuBackground, leaving the corners see-through.
    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::text);

    setColour (juce::TextButton::buttonColourId, theme::surfaceHi);
    setColour (juce::TextButton::buttonOnColourId, theme::accent);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::text);

    setColour (juce::Slider::backgroundColourId, theme::surfaceHi);
    setColour (juce::Slider::trackColourId, theme::accent);
    setColour (juce::Slider::thumbColourId, theme::text);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour (juce::ScrollBar::thumbColourId, theme::surfaceHi);
    setColour (juce::ScrollBar::backgroundColourId, theme::bg);
}

void ReharmLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                              juce::Button& button,
                                              const juce::Colour& /*backgroundColour*/,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = 8.0f;

    juce::Colour fill = theme::surfaceHi;
    if (button.getToggleState())
        fill = theme::accent;
    else if (shouldDrawButtonAsDown)
        fill = theme::surfaceHi.brighter (0.12f);
    else if (shouldDrawButtonAsHighlighted)
        fill = theme::surfaceHi.brighter (0.06f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (theme::border);
    g.drawRoundedRectangle (bounds, radius, 1.0f);
}

void ReharmLookAndFeel::drawButtonText (juce::Graphics& g,
                                        juce::TextButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.findColour (button.getToggleState()
                                        ? juce::TextButton::textColourOnId
                                        : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));

    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().reduced (4, 2),
                      juce::Justification::centred, 1);
}

void ReharmLookAndFeel::drawComboBox (juce::Graphics& g,
                                      int width, int height,
                                      bool /*isButtonDown*/,
                                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float radius = 8.0f;

    g.setColour (theme::surfaceHi);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (box.hasKeyboardFocus (true) ? theme::accent : theme::border);
    g.drawRoundedRectangle (bounds, radius, 1.0f);

    // Arrow
    const float arrowSize = 6.0f;
    const float ax = (float) width - 16.0f;
    const float ay = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - arrowSize * 0.5f, ay - 2.0f,
                       ax + arrowSize * 0.5f, ay - 2.0f,
                       ax, ay + 3.0f);
    g.setColour (theme::textDim);
    g.fillPath (arrow);
}

void ReharmLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, juce::jmax (1, box.getWidth() - 28), box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, theme::text);
}

void ReharmLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    // Clear the whole area first so the region outside the rounded rectangle
    // (the four corners) stays fully transparent.
    g.fillAll (juce::Colours::transparentBlack);
    g.setColour (theme::surfaceHi);

    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 8.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 8.0f, 1.0f);
}

void ReharmLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                           const juce::Rectangle<int>& area,
                                           bool isSeparator,
                                           bool isActive,
                                           bool isHighlighted,
                                           bool isTicked,
                                           bool hasSubMenu,
                                           const juce::String& text,
                                           const juce::String& shortcutKeyText,
                                           const juce::Drawable* /*icon*/,
                                           const juce::Colour* textColourToUse)
{
    if (isSeparator)
    {
        auto r = area.reduced (8, 0);
        g.setColour (theme::border);
        g.fillRect (r.removeFromTop (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (2);
    if (isHighlighted && isActive)
    {
        g.setColour (theme::accent);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }

    auto textColour = (textColourToUse != nullptr ? *textColourToUse : theme::text)
                          .withMultipliedAlpha (isActive ? 1.0f : 0.5f);
    if (isHighlighted && isActive)
        textColour = theme::text;

    g.setColour (textColour);
    g.setFont (getPopupMenuFont());

    auto textArea = r.reduced (10, 0);
    if (isTicked)
    {
        g.drawText (">", textArea.removeFromLeft (14), juce::Justification::centredLeft, false);
    }

    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (textColour.withMultipliedAlpha (0.6f));
        g.drawText (shortcutKeyText, r.reduced (10, 0), juce::Justification::centredRight, false);
    }

    if (hasSubMenu)
    {
        // Simple chevron
        auto arrowZone = r.removeFromRight (16).toFloat();
        juce::Path p;
        p.addTriangle (arrowZone.getCentreX() - 2.0f, arrowZone.getCentreY() - 4.0f,
                       arrowZone.getCentreX() - 2.0f, arrowZone.getCentreY() + 4.0f,
                       arrowZone.getCentreX() + 3.0f, arrowZone.getCentreY());
        g.setColour (textColour);
        g.fillPath (p);
    }
}

void ReharmLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float /*minSliderPos*/,
                                          float /*maxSliderPos*/,
                                          const juce::Slider::SliderStyle style,
                                          juce::Slider& slider)
{
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float radius = juce::jmin (bounds.getHeight(), bounds.getWidth()) * 0.5f;

        g.setColour (theme::surfaceHi);
        g.fillRoundedRectangle (bounds, radius);

        auto filled = bounds;
        if (style == juce::Slider::LinearBar)
            filled.setWidth (juce::jmax (radius * 2.0f, sliderPos - (float) x));
        else
        {
            const float h = juce::jmax (radius * 2.0f, (float) (y + height) - sliderPos);
            filled = bounds.withTop (sliderPos).withHeight (h);
        }

        g.setColour (theme::accent);
        g.fillRoundedRectangle (filled, radius);

        g.setColour (theme::border);
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
        return;
    }

    // Fallback: horizontal linear track with thumb
    auto track = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - 3.0f,
                                         (float) width, 6.0f);
    g.setColour (theme::surfaceHi);
    g.fillRoundedRectangle (track, 3.0f);

    auto filled = track.withWidth (juce::jmax (0.0f, sliderPos - (float) x));
    g.setColour (theme::accent);
    g.fillRoundedRectangle (filled, 3.0f);

    g.setColour (theme::text);
    g.fillEllipse (sliderPos - 6.0f, track.getCentreY() - 6.0f, 12.0f, 12.0f);
}

juce::Font ReharmLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return theme::font (13.0f);
}

juce::Font ReharmLookAndFeel::getTextButtonFont (juce::TextButton&, int /*buttonHeight*/)
{
    return theme::font (13.0f);
}

juce::Font ReharmLookAndFeel::getPopupMenuFont()
{
    return theme::font (13.0f);
}

//==============================================================================
SegmentedToggle::SegmentedToggle (juce::String leftLabel, juce::String rightLabel)
    : leftText (std::move (leftLabel)), rightText (std::move (rightLabel))
{
}

void SegmentedToggle::setLeftActive (bool leftActive)
{
    if (leftIsActive != leftActive)
    {
        leftIsActive = leftActive;
        repaint();
    }
}

void SegmentedToggle::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float radius = bounds.getHeight() * 0.5f;

    g.setColour (theme::surfaceHi);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (theme::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    auto left = bounds.removeFromLeft (bounds.getWidth() * 0.5f);
    auto right = bounds;

    auto paintSeg = [&] (juce::Rectangle<float> r, const juce::String& label, bool active)
    {
        if (active)
        {
            // Fully-rounded pill inside this segment's own half (no bleed into the
            // neighbour segment, so the inactive label is never overdrawn).
            auto pill = r.reduced (2.0f);
            g.setColour (theme::accent);
            g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        }

        g.setColour (active ? theme::text : theme::textDim);
        g.setFont (theme::font (11.0f, true));
        g.drawText (label, r.toNearestInt(), juce::Justification::centred, false);
    };

    paintSeg (left, leftText, leftIsActive);
    paintSeg (right, rightText, ! leftIsActive);
}

void SegmentedToggle::mouseDown (const juce::MouseEvent& e)
{
    const bool wantLeft = e.x < getWidth() / 2;
    if (wantLeft != leftIsActive)
    {
        leftIsActive = wantLeft;
        repaint();
        if (onChanged)
            onChanged (leftIsActive);
    }
}
