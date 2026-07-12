#include "TransportBar.h"

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

    loadButton.onClick = [this]
    {
        if (onLoadPlugin)
            onLoadPlugin();
    };
    addAndMakeVisible (loadButton);

    pluginNameButton.setColour (juce::TextButton::textColourOffId, theme::textDim);
    pluginNameButton.onClick = [this]
    {
        if (onOpenPluginEditor)
            onOpenPluginEditor();
    };
    addAndMakeVisible (pluginNameButton);
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

    auto right = getLocalBounds().reduced (16, 10);
    pluginNameButton.setBounds (right.removeFromRight (160).withSizeKeepingCentre (160, 32));
    right.removeFromRight (8);
    loadButton.setBounds (right.removeFromRight (100).withSizeKeepingCentre (100, 32));
}
