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

    loadButton.showArrow = false;
    loadButton.onClick = [this]
    {
        if (onLoadPlugin)
            onLoadPlugin();
    };
    loadButton.onRightClick = [this]
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        menu.addItem ("Unload VST3", pluginLoaded, false,
                      [this]
                      {
                          if (onUnloadPlugin)
                              onUnloadPlugin();
                      });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadButton));
    };
    addAndMakeVisible (loadButton);

    pluginNameButton.setColour (juce::TextButton::textColourOffId, theme::textDim);
    pluginNameButton.onClick = [this]
    {
        if (onOpenPluginEditor)
            onOpenPluginEditor();
    };
    addAndMakeVisible (pluginNameButton);

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

void TransportBar::setPluginName (const juce::String& name)
{
    pluginLoaded = name.isNotEmpty();

    if (name.isEmpty())
    {
        pluginNameButton.setButtonText ("No Plugin");
        pluginNameButton.setColour (juce::TextButton::textColourOffId, theme::textDim);
    }
    else
    {
        pluginNameButton.setButtonText (name);
        pluginNameButton.setColour (juce::TextButton::textColourOffId, theme::text);
    }
}

void TransportBar::setVoicingClose (bool close)
{
    voicingClose = close;
    updatePlayModeButtonLabel();
}

void TransportBar::setPlayStyle (reharm::ArpPattern p)
{
    pattern = p;
    updatePlayModeButtonLabel();
}

void TransportBar::setArpRate (reharm::ArpRate r)
{
    rate = r;
    updatePlayModeButtonLabel();
}

juce::String TransportBar::rateLabel (reharm::ArpRate r)
{
    switch (r)
    {
        case reharm::ArpRate::Quarter:   return "1/4";
        case reharm::ArpRate::Sixteenth: return "1/16";
        case reharm::ArpRate::Eighth:
        default:                         return "1/8";
    }
}

void TransportBar::updatePlayModeButtonLabel()
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

    const juce::String voicingName = voicingClose ? reharm::loc::tr ("Close")
                                                  : reharm::loc::tr ("Open");
    playModeButton.setButtonText (styleName + " " + voicingName + " " + rateLabel (rate));
}

namespace
{
/** Play-mode popup item IDs.
    Block: 1 = Close, 2 = Open.
    Arp:   100 + patternIdx*20 + voicingIdx*10 + rateIdx
           patternIdx Up=0 Down=1 UpDown=2 ; voicingIdx Close=0 Open=1 ; rateIdx 0..2. */
constexpr int kBlockClose = 1;
constexpr int kBlockOpen  = 2;
constexpr int kArpBase    = 100;

int arpItemId (int patternIdx, int voicingIdx, reharm::ArpRate r)
{
    return kArpBase + patternIdx * 20 + voicingIdx * 10 + (int) r;
}

int patternIdxOf (reharm::ArpPattern p)
{
    switch (p)
    {
        case reharm::ArpPattern::Up:     return 0;
        case reharm::ArpPattern::Down:   return 1;
        case reharm::ArpPattern::UpDown: return 2;
        default:                         return -1;
    }
}
} // namespace

void TransportBar::showPlayModeMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    {
        juce::PopupMenu block;
        block.addItem (kBlockClose, reharm::loc::tr ("Close Voicing"),
                       true, pattern == reharm::ArpPattern::Off && voicingClose);
        block.addItem (kBlockOpen, reharm::loc::tr ("Open Voicing"),
                       true, pattern == reharm::ArpPattern::Off && ! voicingClose);
        menu.addSubMenu (reharm::loc::tr ("Block Chord"), block);
    }

    const int curPatternIdx = patternIdxOf (pattern);

    // Each arp pattern gets a Close/Open submenu, each with the three rates.
    auto addArpSub = [&] (const juce::String& title, reharm::ArpPattern p, int patternIdx)
    {
        juce::PopupMenu patternMenu;

        auto addVoicingSub = [&] (const juce::String& vTitle, int voicingIdx, bool wantClose)
        {
            juce::PopupMenu rates;
            for (auto r : { reharm::ArpRate::Quarter, reharm::ArpRate::Eighth, reharm::ArpRate::Sixteenth })
            {
                const bool ticked = (curPatternIdx == patternIdx)
                                    && (voicingClose == wantClose)
                                    && (rate == r);
                rates.addItem (arpItemId (patternIdx, voicingIdx, r), rateLabel (r), true, ticked);
            }
            patternMenu.addSubMenu (vTitle, rates);
        };

        addVoicingSub (reharm::loc::tr ("Close Voicing"), 0, true);
        addVoicingSub (reharm::loc::tr ("Open Voicing"),  1, false);

        menu.addSubMenu (title, patternMenu);
    };

    addArpSub (reharm::loc::tr ("Arp Up"),      reharm::ArpPattern::Up,     0);
    addArpSub (reharm::loc::tr ("Arp Down"),    reharm::ArpPattern::Down,   1);
    addArpSub (reharm::loc::tr ("Arp Up-Down"), reharm::ArpPattern::UpDown, 2);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&playModeButton),
                        [this] (int resultId) { handlePlayModeMenuResult (resultId); });
}

void TransportBar::handlePlayModeMenuResult (int resultId)
{
    if (resultId == 0)
        return;

    if (resultId == kBlockClose || resultId == kBlockOpen)
    {
        const bool wasArp = (pattern != reharm::ArpPattern::Off);
        voicingClose = (resultId == kBlockClose);
        pattern = reharm::ArpPattern::Off;

        if (onVoicingChanged)
            onVoicingChanged (voicingClose);
        if (wasArp && onPlayStyleChanged)
            onPlayStyleChanged (reharm::ArpPattern::Off);

        updatePlayModeButtonLabel();
        return;
    }

    if (resultId < kArpBase)
        return;

    const int enc = resultId - kArpBase;
    const int patternIdx = enc / 20;
    const int voicingIdx = (enc % 20) / 10;
    const int rateIdx    = enc % 10;

    static const reharm::ArpPattern patterns[3] = { reharm::ArpPattern::Up,
                                                    reharm::ArpPattern::Down,
                                                    reharm::ArpPattern::UpDown };
    if (patternIdx < 0 || patternIdx > 2 || rateIdx < 0 || rateIdx > 2)
        return;

    const bool newVoicingClose = (voicingIdx == 0);
    const bool voicingChanged = (newVoicingClose != voicingClose);

    voicingClose = newVoicingClose;
    pattern = patterns[patternIdx];
    rate = static_cast<reharm::ArpRate> (rateIdx);

    // Voicing affects the arpeggio note content, so notify voicing first.
    if (voicingChanged && onVoicingChanged)
        onVoicingChanged (voicingClose);
    if (onPlayStyleChanged)
        onPlayStyleChanged (pattern);
    if (onArpRateChanged)
        onArpRateChanged (rate);

    updatePlayModeButtonLabel();
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
    pluginNameButton.setBounds (right.removeFromRight (160).withSizeKeepingCentre (160, comboH));
    right.removeFromRight (8);
    loadButton.setBounds (right.removeFromRight (100).withSizeKeepingCentre (100, comboH));
    right.removeFromRight (8);
    midiBadge.setBounds (right.removeFromRight (76).withSizeKeepingCentre (76, comboH));
}
