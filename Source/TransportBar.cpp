#include "TransportBar.h"
#include "Localization.h"

//==============================================================================
PlayStopButton::PlayStopButton()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PlayStopButton::setPlaying (bool playing)
{
    if (playingState != playing)
    {
        playingState = playing;
        repaint();
    }
}

void PlayStopButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float d = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto circle = bounds.withSizeKeepingCentre (d, d);

    g.setColour (playingState ? theme::playing : theme::accent);
    g.fillEllipse (circle);

    g.setColour (theme::text);
    if (! playingState)
    {
        juce::Path tri;
        const float cx = circle.getCentreX() + d * 0.04f;
        const float cy = circle.getCentreY();
        const float s = d * 0.22f;
        tri.addTriangle (cx - s * 0.6f, cy - s,
                         cx - s * 0.6f, cy + s,
                         cx + s * 0.9f, cy);
        g.fillPath (tri);
    }
    else
    {
        const float s = d * 0.28f;
        g.fillRoundedRectangle (circle.getCentreX() - s * 0.5f,
                                circle.getCentreY() - s * 0.5f,
                                s, s, 2.0f);
    }
}

void PlayStopButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

//==============================================================================
MidiDragBadge::MidiDragBadge()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void MidiDragBadge::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (theme::surfaceHi);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    // Icon (eighth-note glyph) + "MIDI" text are treated as one block and
    // centred together, so the left/right margins around the whole badge
    // content are equal.
    const float glyphH = bounds.getHeight() * 0.55f;
    const float noteD = glyphH * 0.34f;
    const float iconW = noteD + glyphH * 0.3f; // notehead footprint + flag width
    const float gapW = 6.0f;

    const auto textFont = theme::font (12.0f, true);
    const juce::String text ("MIDI");
    const float textW = juce::GlyphArrangement::getStringWidth (textFont, text);

    const float contentW = iconW + gapW + textW;
    const float startX = bounds.getX() + (bounds.getWidth() - contentW) * 0.5f;

    const float stemX = startX + noteD;
    const float stemTop = bounds.getCentreY() - glyphH * 0.5f;
    const float stemBottom = bounds.getCentreY() + glyphH * 0.5f;

    g.setColour (theme::accent);
    g.fillRect (juce::Rectangle<float> (stemX - 1.0f, stemTop, 2.0f, glyphH));
    g.fillEllipse (stemX - noteD, stemBottom - noteD * 0.5f, noteD, noteD);

    juce::Path flag;
    flag.addTriangle (stemX, stemTop,
                      stemX, stemTop + glyphH * 0.4f,
                      stemX + glyphH * 0.3f, stemTop + glyphH * 0.15f);
    g.fillPath (flag);

    g.setColour (theme::text);
    g.setFont (textFont);
    auto textArea = juce::Rectangle<float> (startX + iconW + gapW, bounds.getY(), textW, bounds.getHeight());
    g.drawText (text, textArea, juce::Justification::centredLeft);
}

void MidiDragBadge::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
}

void MidiDragBadge::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || e.getDistanceFromDragStart() < 6)
        return;
    dragStarted = true;
    if (onPrepareDragFile == nullptr)
        return;
    const juce::File f = onPrepareDragFile();
    if (! f.existsAsFile())
        return;
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        juce::StringArray { f.getFullPathName() }, /*canMoveFiles*/ false, this, nullptr);
}

void MidiDragBadge::mouseUp (const juce::MouseEvent&)
{
    dragStarted = false;
}

//==============================================================================
TransportBar::TransportBar()
{
    playStop.onClick = [this]
    {
        if (onPlayStopClicked)
            onPlayStopClicked();
    };
    addAndMakeVisible (playStop);

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.onValueChange = [this]
    {
        const int bpm = (int) bpmSlider.getValue();
        bpmLabel.setText ("BPM  " + juce::String (bpm), juce::dontSendNotification);
        if (onBpmChanged)
            onBpmChanged (bpm);
    };
    addAndMakeVisible (bpmSlider);

    bpmLabel.setText ("BPM  120", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centredLeft);
    bpmLabel.setColour (juce::Label::textColourId, theme::text);
    bpmLabel.setFont (theme::font (13.0f));
    addAndMakeVisible (bpmLabel);

    volumeSlider.setSliderStyle (juce::Slider::LinearBar);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    volumeSlider.setRange (0.0, 100.0, 1.0);
    volumeSlider.setValue (80.0, juce::dontSendNotification);
    volumeSlider.onValueChange = [this]
    {
        const int pct = (int) volumeSlider.getValue();
        volumeLabel.setText ("Vol  " + juce::String (pct) + "%", juce::dontSendNotification);
        if (onVolumeChanged)
            onVolumeChanged ((float) pct / 100.0f);
    };
    addAndMakeVisible (volumeSlider);

    volumeLabel.setText ("Vol  80%", juce::dontSendNotification);
    volumeLabel.setJustificationType (juce::Justification::centredLeft);
    volumeLabel.setColour (juce::Label::textColourId, theme::text);
    volumeLabel.setFont (theme::font (13.0f));
    addAndMakeVisible (volumeLabel);

    playModeButton.showArrow = true;
    playModeButton.onClick = [this] { showPlayModeMenu(); };
    addAndMakeVisible (playModeButton);
    updatePlayModeButtonLabel();

    pluginButton.showArrow = false;
    pluginButton.onClick = [this]
    {
        // One button, two jobs: pick a plugin while there is none, open the
        // loaded plugin's editor afterwards.
        if (pluginLoaded)
        {
            if (onOpenPluginEditor)
                onOpenPluginEditor();
        }
        else if (onLoadPlugin)
        {
            onLoadPlugin();
        }
    };
    pluginButton.onRightClick = [this] { showPluginMenu(); };
    addAndMakeVisible (pluginButton);

    midiBadge.onPrepareDragFile = [this] { return onPrepareMidiDragFile != nullptr ? onPrepareMidiDragFile() : juce::File(); };
    addAndMakeVisible (midiBadge);
}

void TransportBar::setPlaying (bool playing)
{
    playStop.setPlaying (playing);
}

void TransportBar::setBpm (int bpm)
{
    bpmSlider.setValue ((double) bpm, juce::dontSendNotification);
    bpmLabel.setText ("BPM  " + juce::String (bpm), juce::dontSendNotification);
}

void TransportBar::setVolume (float volume01)
{
    const int pct = juce::roundToInt (volume01 * 100.0f);
    volumeSlider.setValue ((double) pct, juce::dontSendNotification);
    volumeLabel.setText ("Vol  " + juce::String (pct) + "%", juce::dontSendNotification);
}

void TransportBar::setPlugin (const juce::String& name, const juce::String& path)
{
    pluginLoaded = name.isNotEmpty();
    activePluginPath = pluginLoaded ? path : juce::String();
    pluginButton.setButtonText (pluginLoaded ? name : juce::String ("Load VST3"));
}

void TransportBar::setRecentPlugins (const reharm::RecentPluginList& plugins)
{
    recentPlugins = plugins;
}

void TransportBar::showPluginMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem (reharm::loc::tr ("Load plugin..."),
                  [this]
                  {
                      if (onLoadPlugin)
                          onLoadPlugin();
                  });

    // Unloading the plugin is exactly "play through the built-in synth", so the
    // entry is ticked (and inert) when that is already the case.
    menu.addItem (reharm::loc::tr ("Built-in synth"), pluginLoaded, ! pluginLoaded,
                  [this]
                  {
                      if (onUnloadPlugin)
                          onUnloadPlugin();
                  });

    if (! recentPlugins.empty())
    {
        // Most recent first; the one already playing is ticked and inert, like
        // the built-in synth entry above.
        menu.addSeparator();

        for (const auto& recent : recentPlugins)
        {
            const bool isActive = pluginLoaded && recent.path == activePluginPath;
            menu.addItem (recent.name, ! isActive, isActive,
                          [this, path = recent.path]
                          {
                              if (onRecentPluginSelected)
                                  onRecentPluginSelected (path);
                          });
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pluginButton));
}

void TransportBar::setVoicing (reharm::Voicing::Style voicing)
{
    selection.voicing = voicing;
    selection = reharm::normalize (selection);
    updatePlayModeButtonLabel();
}

void TransportBar::setPlayStyle (reharm::ArpPattern p)
{
    selection.pattern = p;
    selection = reharm::normalize (selection);
    updatePlayModeButtonLabel();
}

void TransportBar::setArpRate (reharm::ArpRate r)
{
    selection.rate = r;
    selection = reharm::normalize (selection);
    updatePlayModeButtonLabel();
}

void TransportBar::updatePlayModeButtonLabel()
{
    // The button label stays in English (short forms), even though the popup menu
    // that sets it is localised: a compact "Arp Up-Down Open 1/8" reads better in
    // the transport bar than the equivalent Japanese, which is long and clumsy.
    playModeButton.setButtonText (reharm::buttonLabel (selection));
}

void TransportBar::showPlayModeMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    {
        juce::PopupMenu block;
        block.addItem (reharm::toMenuId ({ reharm::ArpPattern::Off, reharm::Voicing::Style::Close, selection.rate }),
                       reharm::loc::tr ("Close Voicing"), true,
                       selection.pattern == reharm::ArpPattern::Off && selection.voicing == reharm::Voicing::Style::Close);
        block.addItem (reharm::toMenuId ({ reharm::ArpPattern::Off, reharm::Voicing::Style::Open, selection.rate }),
                       reharm::loc::tr ("Open Voicing"), true,
                       selection.pattern == reharm::ArpPattern::Off && selection.voicing == reharm::Voicing::Style::Open);
        block.addItem (reharm::toMenuId ({ reharm::ArpPattern::Off, reharm::Voicing::Style::Auto, selection.rate }),
                       reharm::loc::tr ("Auto Voicing"), true,
                       selection.pattern == reharm::ArpPattern::Off && selection.voicing == reharm::Voicing::Style::Auto);
        menu.addSubMenu (reharm::loc::tr ("Block Chord"), block);
    }

    // Each arp pattern gets a Close/Open submenu, each with the three rates. Auto is
    // deliberately not offered here: it is a block-chord-only feature, so leaving it
    // out of this submenu makes "Auto while arpeggiating" unreachable from the UI,
    // matching what PlaybackBuilder actually does (it folds Auto to Close for arps).
    auto addArpSub = [&] (const juce::String& title, reharm::ArpPattern p)
    {
        juce::PopupMenu patternMenu;

        auto addVoicingSub = [&] (const juce::String& vTitle, reharm::Voicing::Style v)
        {
            juce::PopupMenu rates;
            for (auto r : { reharm::ArpRate::Quarter, reharm::ArpRate::Eighth, reharm::ArpRate::Sixteenth })
            {
                const bool ticked = (selection.pattern == p) && (selection.voicing == v) && (selection.rate == r);
                rates.addItem (reharm::toMenuId ({ p, v, r }), reharm::rateLabel (r), true, ticked);
            }
            patternMenu.addSubMenu (vTitle, rates);
        };

        addVoicingSub (reharm::loc::tr ("Close Voicing"), reharm::Voicing::Style::Close);
        addVoicingSub (reharm::loc::tr ("Open Voicing"),  reharm::Voicing::Style::Open);

        menu.addSubMenu (title, patternMenu);
    };

    addArpSub (reharm::loc::tr ("Arp Up"),      reharm::ArpPattern::Up);
    addArpSub (reharm::loc::tr ("Arp Down"),    reharm::ArpPattern::Down);
    addArpSub (reharm::loc::tr ("Arp Up-Down"), reharm::ArpPattern::UpDown);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&playModeButton),
                        [this] (int resultId) { handlePlayModeMenuResult (resultId); });
}

void TransportBar::handlePlayModeMenuResult (int resultId)
{
    reharm::PlayModeSelection next;
    if (! reharm::applyMenuId (resultId, selection, next))
        return;

    const auto prev = selection;
    selection = next;
    updatePlayModeButtonLabel();

    if (selection.voicing != prev.voicing && onVoicingChanged)
        onVoicingChanged (selection.voicing);
    if (selection.pattern != prev.pattern && onPlayStyleChanged)
        onPlayStyleChanged (selection.pattern);
    if (selection.rate != prev.rate && onArpRateChanged)
        onArpRateChanged (selection.rate);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::surface);
    g.setColour (theme::border);
    g.fillRect (0, 0, getWidth(), 1);
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced (16, 10);

    playStop.setBounds (area.removeFromLeft (44).withSizeKeepingCentre (44, 44));
    area.removeFromLeft (20);

    {
        auto block = area.removeFromLeft (230);
        bpmLabel.setBounds (block.removeFromLeft (70).withSizeKeepingCentre (70, 24));
        bpmSlider.setBounds (block.withSizeKeepingCentre (block.getWidth(), 20));
    }

    area.removeFromLeft (16);

    {
        auto block = area.removeFromLeft (200);
        volumeLabel.setBounds (block.removeFromLeft (70).withSizeKeepingCentre (70, 24));
        volumeSlider.setBounds (block.withSizeKeepingCentre (block.getWidth(), 20));
    }

    area.removeFromLeft (16);

    const int comboH = 28;
    const int playModeW = 190;
    playModeButton.setBounds (area.removeFromLeft (playModeW).withSizeKeepingCentre (playModeW, comboH));

    auto right = getLocalBounds().reduced (16, 10);
    const int pluginW = 200;
    pluginButton.setBounds (right.removeFromRight (pluginW).withSizeKeepingCentre (pluginW, comboH));
    right.removeFromRight (8);
    midiBadge.setBounds (right.removeFromRight (76).withSizeKeepingCentre (76, comboH));
}
