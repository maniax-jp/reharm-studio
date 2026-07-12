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

    const auto& presets = reharm::ProgressionPresets::all();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (reharm::loc::tr (presets[(size_t) i].name), i + 1);
    presetBox.setTextWhenNothingSelected ("Select preset...");
    presetBox.setSelectedId (0, juce::dontSendNotification);
    presetBox.onChange = [this] { handlePresetChanged(); };
    addAndMakeVisible (presetBox);

    degreeToggle.setLeftActive (false); // default: Name (right)
    degreeToggle.onChanged = [this] (bool leftActive)
    {
        if (onShowDegreeChanged)
            onShowDegreeChanged (leftActive); // Deg = left = showDegree
    };
    addAndMakeVisible (degreeToggle);

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

void HeaderBar::setShowDegree (bool showDegree)
{
    degreeToggle.setLeftActive (showDegree);
}

void HeaderBar::setVoicingClose (bool close)
{
    voicingToggle.setLeftActive (close);
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

    const auto& presets = reharm::ProgressionPresets::all();
    const int index = id - 1;
    if (index >= 0 && index < (int) presets.size())
    {
        if (onPresetSelected)
            onPresetSelected (presets[(size_t) index]);
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

    area.removeFromLeft (12);

    const int toggleW = 100;
    degreeToggle.setBounds (area.removeFromLeft (toggleW).withSizeKeepingCentre (toggleW, comboH));
    area.removeFromLeft (gap);
    voicingToggle.setBounds (area.removeFromLeft (toggleW).withSizeKeepingCentre (toggleW, comboH));
}
