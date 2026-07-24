#include "HeaderBar.h"
#include "Localization.h"

namespace
{

/** PopupMenu custom row for a user preset.
    Left-click selects; right-click closes the parent menu with pendingDeleteName set
    so HeaderBar can show a standalone Delete... menu at the cursor. */
class UserPresetMenuItem : public juce::PopupMenu::CustomComponent
{
public:
    UserPresetMenuItem (const juce::String& nameIn, juce::String& pendingDelete)
        : juce::PopupMenu::CustomComponent (false),
          presetName (nameIn),
          pendingDeleteName (pendingDelete)
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
        // Right-click: close parent menu only. Do not open a nested PopupMenu here;
        // two independent menu stacks race and cancel each other on macOS.
        if (e.mods.isPopupMenu())
        {
            pendingDeleteName = presetName;
            triggerMenuItem();
            return;
        }

        if (e.mouseWasClicked())
            triggerMenuItem();
    }

private:
    juce::String presetName;
    juce::String& pendingDeleteName;
};

/** Encodes play-mode popup item IDs. */
enum PlayModeIds
{
    kCloseVoicing = 1,
    kOpenVoicing  = 2,
    // 10 + rate (0..2)  = Arp Up
    // 20 + rate         = Arp Down
    // 30 + rate         = Arp Up-Down
    kArpUpBase    = 10,
    kArpDownBase  = 20,
    kArpUpDownBase = 30
};

int arpItemId (int base, reharm::ArpRate r)
{
    return base + (int) r;
}

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

    presetButton.setButtonText ("Select preset...");
    presetButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton);
    setUserPresets ({});

    saveButton.setButtonText (reharm::loc::tr ("Save"));
    saveButton.onClick = [this]
    {
        if (onSaveRequested)
            onSaveRequested();
    };
    addAndMakeVisible (saveButton);

    playModeButton.onClick = [this] { showPlayModeMenu(); };
    addAndMakeVisible (playModeButton);
    updatePlayModeButtonLabel();
}

void HeaderBar::setKey (int tonicPitchClass, bool isMajor)
{
    keyBox.setSelectedId (juce::jlimit (0, 11, tonicPitchClass) + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (isMajor ? 1 : 2, juce::dontSendNotification);
}

void HeaderBar::setVoicingClose (bool close)
{
    voicingClose = close;
    updatePlayModeButtonLabel();
}

void HeaderBar::setPlayStyle (reharm::ArpPattern p)
{
    pattern = p;
    updatePlayModeButtonLabel();
}

void HeaderBar::setArpRate (reharm::ArpRate r)
{
    rate = r;
    updatePlayModeButtonLabel();
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

juce::String HeaderBar::rateLabel (reharm::ArpRate r)
{
    switch (r)
    {
        case reharm::ArpRate::Quarter:   return "1/4";
        case reharm::ArpRate::Sixteenth: return "1/16";
        case reharm::ArpRate::Eighth:
        default:                         return "1/8";
    }
}

void HeaderBar::updatePlayModeButtonLabel()
{
    if (pattern == reharm::ArpPattern::Off)
    {
        playModeButton.setButtonText (voicingClose
                                          ? reharm::loc::tr ("Block Chord (Close)")
                                          : reharm::loc::tr ("Block Chord (Open)"));
        return;
    }

    juce::String styleName;
    switch (pattern)
    {
        case reharm::ArpPattern::Up:     styleName = reharm::loc::tr ("Arp Up"); break;
        case reharm::ArpPattern::Down:   styleName = reharm::loc::tr ("Arp Down"); break;
        case reharm::ArpPattern::UpDown: styleName = reharm::loc::tr ("Arp Up-Down"); break;
        case reharm::ArpPattern::Off:
        default:                         styleName = reharm::loc::tr ("Block Chord"); break;
    }

    playModeButton.setButtonText (styleName + " " + rateLabel (rate));
}

void HeaderBar::showPlayModeMenu()
{
    juce::PopupMenu menu;

    {
        juce::PopupMenu block;
        block.addItem (kCloseVoicing, reharm::loc::tr ("Close Voicing"),
                       true, pattern == reharm::ArpPattern::Off && voicingClose);
        block.addItem (kOpenVoicing, reharm::loc::tr ("Open Voicing"),
                       true, pattern == reharm::ArpPattern::Off && ! voicingClose);
        menu.addSubMenu (reharm::loc::tr ("Block Chord"), block);
    }

    auto addArpSub = [&] (const juce::String& title, int base, reharm::ArpPattern p)
    {
        juce::PopupMenu sub;
        for (auto r : { reharm::ArpRate::Quarter, reharm::ArpRate::Eighth, reharm::ArpRate::Sixteenth })
            sub.addItem (arpItemId (base, r), rateLabel (r), true, pattern == p && rate == r);
        menu.addSubMenu (title, sub);
    };

    addArpSub (reharm::loc::tr ("Arp Up"),      kArpUpBase,     reharm::ArpPattern::Up);
    addArpSub (reharm::loc::tr ("Arp Down"),    kArpDownBase,   reharm::ArpPattern::Down);
    addArpSub (reharm::loc::tr ("Arp Up-Down"), kArpUpDownBase, reharm::ArpPattern::UpDown);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&playModeButton),
                        [this] (int resultId) { handlePlayModeMenuResult (resultId); });
}

void HeaderBar::handlePlayModeMenuResult (int resultId)
{
    if (resultId == 0)
        return;

    if (resultId == kCloseVoicing || resultId == kOpenVoicing)
    {
        const bool wasArp = (pattern != reharm::ArpPattern::Off);
        voicingClose = (resultId == kCloseVoicing);
        pattern = reharm::ArpPattern::Off;

        if (onVoicingChanged)
            onVoicingChanged (voicingClose);
        if (wasArp && onPlayStyleChanged)
            onPlayStyleChanged (reharm::ArpPattern::Off);

        updatePlayModeButtonLabel();
        return;
    }

    reharm::ArpPattern newPattern = reharm::ArpPattern::Off;
    int base = 0;

    if (resultId >= kArpUpDownBase)
    {
        newPattern = reharm::ArpPattern::UpDown;
        base = kArpUpDownBase;
    }
    else if (resultId >= kArpDownBase)
    {
        newPattern = reharm::ArpPattern::Down;
        base = kArpDownBase;
    }
    else if (resultId >= kArpUpBase)
    {
        newPattern = reharm::ArpPattern::Up;
        base = kArpUpBase;
    }
    else
    {
        return;
    }

    const int rateIndex = resultId - base;
    if (rateIndex < 0 || rateIndex > 2)
        return;

    pattern = newPattern;
    rate = static_cast<reharm::ArpRate> (rateIndex);

    if (onPlayStyleChanged)
        onPlayStyleChanged (pattern);
    if (onArpRateChanged)
        onArpRateChanged (rate);

    updatePlayModeButtonLabel();
}

void HeaderBar::showPresetMenu()
{
    pendingDeleteName.clear();

    juce::PopupMenu menu;

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
                                std::make_unique<UserPresetMenuItem> (userPresetNames[i], pendingDeleteName),
                                nullptr,
                                userPresetNames[i]);
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton),
                        [this] (int resultId) { handlePresetMenuResult (resultId); });
}

void HeaderBar::handlePresetMenuResult (int resultId)
{
    if (resultId == 0)
    {
        pendingDeleteName.clear();
        return;
    }

    if (resultId >= 1000)
    {
        if (pendingDeleteName.isNotEmpty())
        {
            // Parent Select-preset menu is already closed. Show Delete... at cursor
            // as a standalone menu so it does not race the parent stack.
            const juce::String name = pendingDeleteName;
            pendingDeleteName.clear();

            const auto mouse = juce::Desktop::getInstance().getMainMouseSource()
                                   .getScreenPosition().roundToInt();
            juce::PopupMenu context;
            context.addItem (1, reharm::loc::tr ("Delete..."));
            context.showMenuAsync (juce::PopupMenu::Options()
                                       .withTargetScreenArea (juce::Rectangle<int> (mouse.x, mouse.y, 1, 1)),
                                   [this, name] (int r)
                                   {
                                       if (r == 1 && onUserPresetDeleteRequested)
                                           onUserPresetDeleteRequested (name);
                                   });
            return;
        }

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
    area.removeFromLeft (gap);

    area.removeFromLeft (12);

    const int playModeW = 210;
    playModeButton.setBounds (area.removeFromLeft (playModeW).withSizeKeepingCentre (playModeW, comboH));
}
