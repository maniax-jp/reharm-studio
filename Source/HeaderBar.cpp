#include "HeaderBar.h"
#include "Localization.h"

namespace
{

/** PopupMenu custom row for a user preset.
    Left-click selects; right-click overlays a "Delete..." menu on top of the
    still-open parent menu (the parent Select-preset list stays visible behind
    it). Only picking "Delete..." closes the parent, via triggerMenuItem(). */
class UserPresetMenuItem : public juce::PopupMenu::CustomComponent
{
public:
    UserPresetMenuItem (const juce::String& nameIn, juce::String& pendingDelete,
                        juce::LookAndFeel* lookAndFeelIn)
        : juce::PopupMenu::CustomComponent (false),
          presetName (nameIn),
          pendingDeleteName (pendingDelete),
          lnf (lookAndFeelIn)
    {
    }

    void getIdealSize (int& idealWidth, int& idealHeight) override
    {
        idealHeight = 24;
        const auto font = theme::font (14.0f);
        const int textW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, presetName));
        idealWidth = juce::jmax (180, textW + 24);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (2);
        if (isItemHighlighted())
        {
            g.setColour (theme::accent);
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
        }

        g.setColour (theme::text);
        g.setFont (theme::font (14.0f));
        g.drawText (presetName, r.withTrimmedLeft (10).withTrimmedRight (10),
                    juce::Justification::centredLeft, true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            // Overlay a small "Delete..." menu on top of the still-open parent
            // (Select preset) menu, anchored at the cursor. The parent is only
            // closed (via triggerMenuItem) once the user actually picks Delete.
            juce::PopupMenu context;
            if (lnf != nullptr)
                context.setLookAndFeel (lnf);
            context.addItem (1, reharm::loc::tr ("Delete..."));
            context.showMenuAsync (juce::PopupMenu::Options()
                                       .withTargetScreenArea (juce::Rectangle<int> (
                                           e.getScreenPosition().x, e.getScreenPosition().y, 1, 1)),
                                   [this] (int r)
                                   {
                                       if (r == 1)
                                       {
                                           pendingDeleteName = presetName;
                                           triggerMenuItem(); // Closes the parent Select-preset menu.
                                       }
                                   });
            return;
        }

        if (e.mouseWasClicked())
            triggerMenuItem();
    }

private:
    juce::String presetName;
    juce::String& pendingDeleteName;
    juce::LookAndFeel* lnf;
};

} // namespace

HeaderBar::HeaderBar()
{
    static const char* noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    for (int i = 0; i < 12; ++i)
        keyBox.addItem (noteNames[i], i + 1);
    keyBox.setSelectedId (1, juce::dontSendNotification);
    keyBox.onChange = [this] { handleKeyOrScaleChanged(); };
    addAndMakeVisible (keyBox);

    scaleBox.addItem ("Major", 1);
    scaleBox.addItem ("Minor", 2);
    scaleBox.setSelectedId (1, juce::dontSendNotification);
    scaleBox.onChange = [this] { handleKeyOrScaleChanged(); };
    addAndMakeVisible (scaleBox);

    presetButton.showArrow = true;
    presetButton.setButtonText ("Select preset...");
    presetButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton);
    setUserPresets ({});

    saveButton.showArrow = false;
    saveButton.setButtonText (reharm::loc::tr ("Save"));
    saveButton.onClick = [this]
    {
        if (onSaveRequested)
            onSaveRequested();
    };
    addAndMakeVisible (saveButton);
}

void HeaderBar::setKey (int tonicPitchClass, bool isMajor)
{
    keyBox.setSelectedId (juce::jlimit (0, 11, tonicPitchClass) + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (isMajor ? 1 : 2, juce::dontSendNotification);
}

void HeaderBar::setUserPresets (const juce::StringArray& names)
{
    userPresetNames = names;
}

void HeaderBar::handleKeyOrScaleChanged()
{
    if (onKeyChanged)
        onKeyChanged (keyBox.getSelectedId() - 1, scaleBox.getSelectedId() == 1);
}

void HeaderBar::showPresetMenu()
{
    pendingDeleteName.clear();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    const auto& presets = reharm::ProgressionPresets::all();
    for (int i = 0; i < (int) presets.size(); ++i)
        menu.addItem (i + 1, reharm::loc::tr (presets[(size_t) i].name));

    if (! userPresetNames.isEmpty())
    {
        menu.addSeparator();
        menu.addSectionHeader (reharm::loc::tr ("User"));

        for (int i = 0; i < userPresetNames.size(); ++i)
        {
            menu.addCustomItem (1000 + i,
                                std::make_unique<UserPresetMenuItem> (userPresetNames[i], pendingDeleteName,
                                                                      &getLookAndFeel()),
                                nullptr,
                                userPresetNames[i]);
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton),
                        [this] (int resultId) { handlePresetMenuResult (resultId); });
}

void HeaderBar::handlePresetMenuResult (int resultId)
{
    // A pending delete is honoured regardless of resultId: when the delete is
    // triggered from the nested "Delete..." context menu, triggerMenuItem()
    // closes the parent with resultId 0 rather than the custom item's id, so we
    // must key off pendingDeleteName, not the id.
    if (pendingDeleteName.isNotEmpty())
    {
        const juce::String name = pendingDeleteName;
        pendingDeleteName.clear();

        if (onUserPresetDeleteRequested)
            onUserPresetDeleteRequested (name);
        return;
    }

    if (resultId == 0)
        return;

    if (resultId >= 1000)
    {
        const int index = resultId - 1000;
        if (index >= 0 && index < userPresetNames.size())
        {
            if (onUserPresetSelected)
                onUserPresetSelected (userPresetNames[index]);
        }

        pendingDeleteName.clear();
        return;
    }

    const auto& presets = reharm::ProgressionPresets::all();
    const int index = resultId - 1;
    if (index >= 0 && index < (int) presets.size())
    {
        if (onPresetSelected)
            onPresetSelected (presets[(size_t) index]);
    }

    pendingDeleteName.clear();
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::surface);
    g.setColour (theme::border);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // Title: "REHARM " + "STUDIO" (accent)
    g.setFont (theme::font (18.0f, true));
    const int titleY = 0;
    const int titleH = getHeight();
    const juce::String reharm = "REHARM ";
    const juce::String studio = "STUDIO";
    const int reharmW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), reharm));
    const int padX = 16;

    g.setColour (theme::text);
    g.drawText (reharm, padX, titleY, reharmW, titleH, juce::Justification::centredLeft, false);
    g.setColour (theme::accent);
    g.drawText (studio, padX + reharmW, titleY, 120, titleH, juce::Justification::centredLeft, false);
}

void HeaderBar::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    // Leave room for title on the left
    area.removeFromLeft (160);

    const int comboH = 28;
    const int comboW = 72;
    const int gap = 8;

    auto placeCombo = [&] (juce::Component& c, int w)
    {
        c.setBounds (area.removeFromLeft (w).withSizeKeepingCentre (w, comboH));
        area.removeFromLeft (gap);
    };

    placeCombo (keyBox, comboW);
    placeCombo (scaleBox, 90);
    placeCombo (presetButton, 200);

    const int saveButtonW = 64;
    saveButton.setBounds (area.removeFromLeft (saveButtonW).withSizeKeepingCentre (saveButtonW, comboH));
}
