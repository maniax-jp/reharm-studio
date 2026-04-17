#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize (800, 600);

    addAndMakeVisible (loadButton);
    loadButton.addListener (this);

    addAndMakeVisible (playButton);
    playButton.addListener (this);

    addAndMakeVisible (stopButton);
    stopButton.addListener (this);

    addAndMakeVisible (keyComboBox);
    keyComboBox.addListener (this);
    keyComboBox.addItem ("C", 1);
    keyComboBox.addItem ("C#", 2);
    keyComboBox.addItem ("D", 3);
    keyComboBox.addItem ("D#", 4);
    keyComboBox.addItem ("E", 5);
    keyComboBox.addItem ("F", 6);
    keyComboBox.addItem ("F#", 7);
    keyComboBox.addItem ("G", 8);
    keyComboBox.addItem ("G#", 9);
    keyComboBox.addItem ("A", 10);
    keyComboBox.addItem ("A#", 11);
    keyComboBox.addItem ("B", 12);
    keyComboBox.setSelectedId (1);

    addAndMakeVisible (scaleComboBox);
    scaleComboBox.addListener (this);
    scaleComboBox.addItem ("Major", 1);
    scaleComboBox.addItem ("Minor", 2);
    scaleComboBox.setSelectedId (1);

    addAndMakeVisible (progressionDisplay);
    progressionDisplay.setMultiLine (true);
    progressionDisplay.setReadOnly (true);

    // Volume slider
    addAndMakeVisible (volumeSlider);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (0.8);
    volumeSlider.addListener (this);

    addAndMakeVisible (volumeLabel);
    volumeLabel.setText ("Volume: 80%", false);
    volumeLabel.setReadOnly (true);

    // BPM slider
    addAndMakeVisible (bpmSlider);
    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0);
    bpmSlider.addListener (this);

    addAndMakeVisible (bpmLabel);
    bpmLabel.setText ("BPM: 120", false);
    bpmLabel.setReadOnly (true);

    deviceManager.initialise (0, 2, nullptr, true);

    setAudioChannels (0, 2);
}

MainComponent::~MainComponent()
{
    stopPlayback();
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    samplesPerBeat = static_cast<int> (currentSampleRate * 60.0 / bpm);
    if (pluginInstance != nullptr)
    {
        pluginInstance->prepareToPlay (sampleRate, samplesPerBlockExpected);
    }
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (pluginInstance == nullptr)
        return;

    if (isPlaying)
    {
        int numSamples = bufferToFill.numSamples;
        
        int currentBpm = bpm.load();
        double beatsPerSample = currentBpm / (60.0 * currentSampleRate);
        double blockBeats = numSamples * beatsPerSample;
        
        double startBeat = currentBeatPosition;
        currentBeatPosition += blockBeats;
        double endBeat = currentBeatPosition;

        double totalBeats = (double)chordNotes.size() * 4.0;
        if (totalBeats <= 0) return;

        // 1. Handle the very first Note On
        if (startBeat == 0.0 && currentChordIndex < (int)chordNotes.size())
        {
            for (int note : chordNotes[0])
                midiBuffer.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<uint8>(64)), 0);
        }

        // 2. Handle transitions (including loop)
        while (true)
        {
            double nextTransitionBeat = (currentChordIndex + 1) * 4.0;
            if (currentChordIndex == (int)chordNotes.size() - 1)
                nextTransitionBeat = totalBeats;

            if (nextTransitionBeat >= endBeat)
                break;

            if (nextTransitionBeat >= startBeat)
            {
                int relativeOffset = static_cast<int>((nextTransitionBeat - startBeat) / beatsPerSample);
                
                for (int note : chordNotes[currentChordIndex])
                    midiBuffer.addEvent (juce::MidiMessage::noteOff (1, note), relativeOffset);
                
                currentChordIndex++;
                if (currentChordIndex >= (int)chordNotes.size())
                    currentChordIndex = 0;
                
                for (int note : chordNotes[currentChordIndex])
                    midiBuffer.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<uint8>(64)), relativeOffset);
                
                if (nextTransitionBeat == totalBeats)
                {
                    startBeat -= totalBeats;
                    endBeat -= totalBeats;
                }
            }
            else
            {
                currentChordIndex++;
                if (currentChordIndex >= (int)chordNotes.size())
                    currentChordIndex = 0;
            }
        }

        // Wrap currentBeatPosition for the next block
        while (currentBeatPosition >= totalBeats)
            currentBeatPosition -= totalBeats;
    }
    else if (stopRequested)
    {
        // Send Note Off for all notes in the current chord to stop the sound immediately
        if (currentChordIndex < (int)chordNotes.size())
        {
            for (int note : chordNotes[currentChordIndex])
                midiBuffer.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        }
        stopRequested = false;
        
        // Reset playback state after stopping all notes
        currentChordIndex = 0;
        currentBeatPosition = 0.0;
    }

    pluginInstance->processBlock (*bufferToFill.buffer, midiBuffer);
    
    // Apply volume gain to the processed audio
    bufferToFill.buffer->applyGain (volume);
    
    midiBuffer.clear();
}

void MainComponent::releaseResources()
{
    if (pluginInstance != nullptr)
        pluginInstance->releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topArea = area.removeFromTop (40);
    loadButton.setBounds (topArea.removeFromLeft (100));
    playButton.setBounds (topArea.removeFromLeft (150));
    stopButton.setBounds (topArea.removeFromLeft (100));

    auto keyArea = area.removeFromTop (40);
    keyComboBox.setBounds (keyArea.removeFromLeft (100));
    scaleComboBox.setBounds (keyArea.removeFromLeft (100));

    auto controlArea = area.removeFromTop (60);
    volumeSlider.setBounds (controlArea.removeFromLeft (200));
    volumeLabel.setBounds (controlArea.removeFromLeft (150));
    bpmSlider.setBounds (controlArea.removeFromLeft (200));
    bpmLabel.setBounds (controlArea.removeFromLeft (100));

    progressionDisplay.setBounds (area);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
}

void MainComponent::buttonClicked (juce::Button* button)
{
    if (button == &loadButton)
    {
        loadPlugin();
    }
    else if (button == &playButton)
    {
        playChordProgression();
    }
    else if (button == &stopButton)
    {
        stopPlayback();
    }
}

void MainComponent::comboBoxChanged (juce::ComboBox* comboBox)
{
    if (comboBox == &keyComboBox || comboBox == &scaleComboBox)
    {
        // Update progression display
        auto progression = generator.generateProgression (keyComboBox.getSelectedId() - 1,
                                                          scaleComboBox.getSelectedId() == 1 ? "Major" : "Minor");
        progressionDisplay.setText (progression);
    }
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &volumeSlider)
    {
        volume = static_cast<float> (volumeSlider.getValue());
        volumeLabel.setText ("Volume: " + juce::String (static_cast<int> (volume * 100)) + "%", false);
    }
    else if (slider == &bpmSlider)
    {
        int newBpm = static_cast<int> (bpmSlider.getValue());
        bpm = newBpm;
        samplesPerBeat = static_cast<int> (currentSampleRate * 60.0 / newBpm);
        bpmLabel.setText ("BPM: " + juce::String (newBpm), false);
    }
}

void MainComponent::timerCallback()
{
    // Timer is no longer used for MIDI scheduling
}

void MainComponent::loadPlugin()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Select a VST3 plugin file...",
                                                        juce::File ("/Library/Audio/Plug-Ins/VST3"),
                                                        "*.vst3");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, chooser] (const juce::FileChooser& c)
                          {
                              auto file = c.getResult();

                              if (file.exists())
                              {
                                  stopPlayback();

                                  juce::VST3PluginFormat format;
                                  juce::OwnedArray<juce::PluginDescription> descriptions;

                                  format.findAllTypesForFile (descriptions, file.getFullPathName());

                                  if (!descriptions.isEmpty())
                                  {
                                      juce::String error;
                                      pluginInstance = format.createInstanceFromDescription (*descriptions[0], currentSampleRate, 512, error);

                                      if (pluginInstance != nullptr)
                                      {
                                          pluginInstance->prepareToPlay (currentSampleRate, 512);
                                          juce::Logger::writeToLog ("Plugin loaded successfully");
                                      }
                                      else
                                      {
                                          juce::Logger::writeToLog ("Failed to load plugin: " + error);
                                      }
                                  }
                                  else
                                  {
                                      juce::Logger::writeToLog ("No valid VST3 plugin found");
                                  }
                              }
                          });
}

void MainComponent::stopPlayback()
{
    stopRequested = true;
    isPlaying = false;
}

void MainComponent::playChordProgression()
{
    if (pluginInstance == nullptr)
        return;

    stopPlayback();

    auto progression = generator.generateProgression (keyComboBox.getSelectedId() - 1,
                                                      scaleComboBox.getSelectedId() == 1 ? "Major" : "Minor");

    // Parse progression and generate chord notes
    chordNotes.clear();
    juce::StringArray chords;
    juce::String remaining = progression;
    while (remaining.isNotEmpty())
    {
        int nextHyphen = remaining.indexOf (" - ");
        if (nextHyphen >= 0)
        {
            chords.add (remaining.substring (0, nextHyphen).trim());
            remaining = remaining.substring (nextHyphen + 3);
        }
        else
        {
            chords.add (remaining.trim());
            remaining = "";
        }
    }

    int root = keyComboBox.getSelectedId() - 1;
    bool isMajor = scaleComboBox.getSelectedId() == 1;

    for (auto& chord : chords)
    {
        juce::Array<int> notes = getChordNotes (chord, root, isMajor);
        chordNotes.add (notes);
    }

    samplesPerBeat = static_cast<int> (currentSampleRate * 60.0 / bpm);
    currentChordIndex = 0;
    currentBeatPosition = 0.0;
    isPlaying = true;
}

juce::Array<int> MainComponent::getChordNotes (const juce::String& chordSymbol, int root, bool isMajor)
{
    juce::Array<int> notes;

    int baseNote = 60 + root;

    if (chordSymbol == "I" || chordSymbol == "i")
    {
        notes.add (baseNote);
        notes.add (baseNote + 4);
        notes.add (baseNote + 7);
    }
    else if (chordSymbol == "ii" || chordSymbol == "ii°")
    {
        notes.add (baseNote + 2);
        notes.add (baseNote + 5);
        notes.add (baseNote + 9);
    }
    else if (chordSymbol == "iii" || chordSymbol == "III")
    {
        notes.add (baseNote + 4);
        notes.add (baseNote + 7);
        notes.add (baseNote + 11);
    }
    else if (chordSymbol == "IV" || chordSymbol == "iv")
    {
        notes.add (baseNote + 5);
        notes.add (baseNote + 9);
        notes.add (baseNote + 12);
    }
    else if (chordSymbol == "V" || chordSymbol == "v")
    {
        notes.add (baseNote + 7);
        notes.add (baseNote + 11);
        notes.add (baseNote + 14);
    }
    else if (chordSymbol == "vi" || chordSymbol == "VI")
    {
        notes.add (baseNote + 9);
        notes.add (baseNote + 12);
        notes.add (baseNote + 16);
    }
    else if (chordSymbol == "vii°" || chordSymbol == "VII")
    {
        notes.add (baseNote + 11);
        notes.add (baseNote + 14);
        notes.add (baseNote + 17);
    }
    else if (chordSymbol == "bVII")
    {
        notes.add (baseNote + 10);
        notes.add (baseNote + 14);
        notes.add (baseNote + 17);
    }

    return notes;
}