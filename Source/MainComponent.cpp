#include "MainComponent.h"
#include <dlfcn.h>
#include <fstream>

static void debugLog (const juce::String& msg)
{
    auto path = juce::File ("/tmp/reharm_debug.log");
    std::ofstream f (path.getFullPathName().toStdString(), std::ios::app);
    if (f.is_open()) {
        f << msg << std::endl;
        f.close();
    }
    juce::Logger::writeToLog (msg);
}

// CoreAudio types (minimal definitions to avoid header conflicts)
typedef uint32_t UInt32;
typedef uint64_t AudioObjectID;
typedef int32_t AudioObjectPropertyStatus;
typedef struct AudioObjectPropertyAddress {
    UInt32 mSelector;
    UInt32 mScope;
    UInt32 mElement;
} AudioObjectPropertyAddress;
typedef AudioObjectID AudioDeviceID;

MainComponent::MainComponent()
{
    setSize (800, 600);

    audioEngine = std::make_unique<AudioEngine>();

    addAndMakeVisible (loadButton);
    loadButton.addListener (this);

    addAndMakeVisible (pluginNameButton);
    pluginNameButton.addListener (this);

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
    audioEngine->prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::MidiBuffer midiBuffer;
    audioEngine->processBlock (*bufferToFill.buffer, midiBuffer);

    // Since we are an AudioAppComponent, the buffer passed to us is the output buffer.
    // AudioEngine::processBlock handles filling the buffer and processing MIDI.
    // However, we need to ensure that the MIDI messages generated in AudioEngine
    // are actually handled if this were a plugin. In a standalone app, the
    // AudioEngine already handled the plugin processBlock internally.
}

void MainComponent::releaseResources()
{
    audioEngine->releaseResources();
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
    pluginNameButton.setBounds (topArea.removeFromLeft (200));
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
    else if (button == &pluginNameButton)
    {
        openPluginEditor();
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
        auto progression = generator.generateProgression (keyComboBox.getSelectedId() - 1,
                                                          scaleComboBox.getSelectedId() == 1 ? "Major" : "Minor");
        progressionDisplay.setText (progression);
    }
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &volumeSlider)
    {
        float vol = static_cast<float> (volumeSlider.getValue());
        audioEngine->setVolume (vol);
        volumeLabel.setText ("Volume: " + juce::String (static_cast<int> (vol * 100)) + "%", false);
    }
    else if (slider == &bpmSlider)
    {
        int newBpm = static_cast<int> (bpmSlider.getValue());
        audioEngine->setBpm (newBpm);
        bpmLabel.setText ("BPM: " + juce::String (newBpm), false);
    }
}

void MainComponent::timerCallback()
{
    if (audioEngine->getPluginInstance() != nullptr)
    {
        audioEngine->setPluginReady(true);
        stopTimer();
    }
}

void MainComponent::markPluginAsReady()
{
    audioEngine->setPluginReady(true);
}

void MainComponent::openPluginEditor()
{
    auto* plugin = audioEngine->getPluginInstance();
    if (plugin == nullptr)
        return;

    if (editorWindow != nullptr)
    {
        editorWindow->setVisible (true);
        return;
    }

    auto* editor = plugin->createEditorIfNeeded();
    if (editor != nullptr)
    {
        auto window = std::make_unique<PluginEditorWindow> (plugin->getName(), editor);
        window->setVisible (true);
        editorWindow = std::move (window);
    }
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
                                  debugLog ("loadPlugin: file selected: " + file.getFullPathName());

                                  stopPlayback();
                                  editorWindow.reset();

                                  juce::VST3PluginFormat format;
                                  juce::OwnedArray<juce::PluginDescription> descriptions;

                                  debugLog ("loadPlugin: scanning plugin file...");
                                  format.findAllTypesForFile (descriptions, file.getFullPathName());
                                  debugLog ("loadPlugin: scan complete, descriptions count: " + juce::String (descriptions.size()));

                                  if (!descriptions.isEmpty())
                                  {
                                      auto setup = deviceManager.getAudioDeviceSetup();
                                      double sampleRate = setup.sampleRate;
                                      int blockSize = setup.bufferSize;

                                      debugLog ("loadPlugin: closing audio device (SR=" + juce::String (sampleRate) + ", BS=" + juce::String (blockSize) + ")");
                                      deviceManager.closeAudioDevice();
                                      debugLog ("loadPlugin: audio device closed");

                                      audioEngine->beginPluginLoad();
                                      juce::Thread::sleep (200);

                                      debugLog ("loadPlugin: calling createInstanceFromDescription...");
                                      juce::String error;
                                      auto descCopy = *descriptions[0];
                                      debugLog ("loadPlugin: desc name=" + descCopy.name);
                                      auto pluginInstance = format.createInstanceFromDescription (descCopy, sampleRate, blockSize, error);

                                      audioEngine->endPluginLoad();

                                      if (pluginInstance != nullptr)
                                      {
                                          debugLog ("loadPlugin: createInstanceFromDescription succeeded");

                                          // Reinitialize audio device BEFORE loadPluginInstance (which calls prepareToPlay)
                                          debugLog ("loadPlugin: reinitializing audio device...");
                                          deviceManager.initialise (0, 2, nullptr, true);
                                          debugLog ("loadPlugin: audio device reinitialized");

                                          auto pluginName = pluginInstance->getName();
                                          audioEngine->loadPluginInstance (std::move (pluginInstance), sampleRate, blockSize);
                                          debugLog ("Plugin loaded successfully");

                                          // Set button text AFTER loading to avoid UI thread contention
                                          pluginNameButton.setButtonText (pluginName);
                                          debugLog ("Plugin loaded UI update: " + pluginName);
                                      }
                                      else
                                      {
                                          // Reinitialize audio device even on failure
                                          debugLog ("loadPlugin: reinitializing audio device...");
                                          deviceManager.initialise (0, 2, nullptr, true);
                                          debugLog ("loadPlugin: audio device reinitialized");
                                          debugLog ("loadPlugin: createInstanceFromDescription returned nullptr: " + error);
                                      }
                                  }
                                  else
                                  {
                                      debugLog ("No valid VST3 plugin found");
                                  }
                              }
                          });
}

void MainComponent::stopPlayback()
{
    audioEngine->stopPlayback();
}

void MainComponent::playChordProgression()
{
    // We need the plugin to be loaded first
    // AudioEngine doesn't expose the pluginInstance directly, so we check if it can play
    // In a real app, we'd check if pluginInstance is null in AudioEngine.

    auto progression = generator.generateProgression (keyComboBox.getSelectedId() - 1,
                                                      scaleComboBox.getSelectedId() == 1 ? "Major" : "Minor");

    // Parse progression and generate chord notes
    juce::Array<juce::Array<int>> juceNotes;
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
        auto notes = ChordTheory::getChordNotes (chord, root, isMajor);
        juce::Array<int> juceNotesArray;
        for (int n : notes) juceNotesArray.add (n);
        juceNotes.add (juceNotesArray);
    }

    auto chordData = std::make_shared<ChordData> (juceNotes);
    audioEngine->setChordData (chordData);
    audioEngine->startPlayback();
}
