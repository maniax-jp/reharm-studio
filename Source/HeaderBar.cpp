#include "HeaderBar.h"
#include "Localization.h"

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

    presetBox.onChange = [this] { handlePresetChanged(); };
    addAndMakeVisible (presetBox);
    setUserPresets ({});

    saveButton.setButtonText (reharm::loc::tr ("Save"));
    saveButton.onClick = [this]
    {
        if (onSaveRequested)
            onSaveRequested();
    };
    addAndMakeVisible (saveButton);

    voicingToggle.setLeftActive (true); // default: Close

    voicingToggle.onChanged = [this] (bool leftActive)
    {
        if (onVoicingChanged)
            onVoicingChanged (leftActive); // Close = left
    };
    addAndMakeVisible (voicingToggle);
}

void HeaderBar::setKey (int tonicPitchClass, bool isMajor)
{
    keyBox.setSelectedId (juce::jlimit (0, 11, tonicPitchClass) + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (isMajor ? 1 : 2, juce::dontSendNotification);
}

void HeaderBar::setVoicingClose (bool close)

{
    voicingToggle.setLeftActive (close);
}

void HeaderBar::setUserPresets (const juce::StringArray& names)
{
    userPresetNames = names;

    presetBox.clear (juce::dontSendNotification);

    const auto& presets = reharm::ProgressionPresets::all();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (reharm::loc::tr (presets[(size_t) i].name), i + 1);

    if (! userPresetNames.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading (reharm::loc::tr ("User"));

        for (int i = 0; i < userPresetNames.size(); ++i)
            presetBox.addItem (userPresetNames[i], 1000 + i);
    }

    presetBox.setTextWhenNothingSelected ("Select preset...");
    // Command-style combo box: always rests in the deselected state.
    presetBox.setSelectedId (0, juce::dontSendNotification);
}

void HeaderBar::handleKeyOrScaleChanged()
{
    if (onKeyChanged)
        onKeyChanged (keyBox.getSelectedId() - 1, scaleBox.getSelectedId() == 1);
}

void HeaderBar::handlePresetChanged()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0)
        return;

    if (id >= 1000)
    {
        const int index = id - 1000;
        if (index >= 0 && index < userPresetNames.size())
        {
            if (onUserPresetSelected)
                onUserPresetSelected (userPresetNames[index]);
        }
    }
    else
    {
        const auto& presets = reharm::ProgressionPresets::all();
        const int index = id - 1;
        if (index >= 0 && index < (int) presets.size())
        {
            if (onPresetSelected)
                onPresetSelected (presets[(size_t) index]);
        }
    }

    // Command-style: clear selection immediately
    presetBox.setSelectedId (0, juce::dontSendNotification);
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
    placeCombo (presetBox, 200);

    const int saveButtonW = 64;
    saveButton.setBounds (area.removeFromLeft (saveButtonW).withSizeKeepingCentre (saveButtonW, comboH));
    area.removeFromLeft (gap);

    area.removeFromLeft (12);

    const int toggleW = 100;
    voicingToggle.setBounds (area.removeFromLeft (toggleW).withSizeKeepingCentre (toggleW, comboH));

}
