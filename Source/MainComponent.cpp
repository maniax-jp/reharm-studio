#include "MainComponent.h"
#include <dlfcn.h>
#include <fstream>

static void debugLog (const juce::String& msg)
{
#ifdef REHARM_DEBUG_LOG
    auto path = juce::File ("/tmp/reharm_debug.log");
    std::ofstream f (path.getFullPathName().toStdString(), std::ios::app);
    if (f.is_open()) {
        f << msg << std::endl;
        f.close();
    }
    juce::Logger::writeToLog (msg);
#endif
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

//==============================================================================
MainComponent::MainComponent()
{
    setSize (1140, 760);

    lookAndFeel = std::make_unique<ReharmLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    audioEngine = std::make_unique<AudioEngine>();

    // --- Child components ---
    addAndMakeVisible (headerBar);
    addAndMakeVisible (sequencerView);
    addAndMakeVisible (analysisStrip);
    addAndMakeVisible (chordEditor);
    addAndMakeVisible (transportBar);

    sequencerView.setModel (&model);
    sequencerView.setDisplayState (&display);
    chordEditor.setModel (&model);
    chordEditor.setDisplayState (&display);

    headerBar.setKey (model.getKey().tonicPitchClass, model.getKey().isMajor);
    headerBar.setVoicingClose (display.voicingStyle == reharm::Voicing::Style::Close);


    headerBar.onKeyChanged = [this] (int tonic, bool isMajor)
    {
        model.setKey ({ tonic, isMajor });
    };

    headerBar.onPresetSelected = [this] (const reharm::PresetProgression& preset)
    {
        reharm::ProgressionPresets::applyToModel (preset, model);
        display.clearSelection();
        updateEditorVisibility();
    };

    headerBar.onVoicingChanged = [this] (bool close)

    {
        display.voicingStyle = close ? reharm::Voicing::Style::Close
                                     : reharm::Voicing::Style::Open;
        if (isPlaying)
            pushChordDataToEngine();
    };

    sequencerView.onCellSelected = [this] (int bar, int slot)
    {
        display.selectedBar = bar;
        display.selectedSlot = slot;
        chordEditor.refreshFromSelection();
        updateEditorVisibility();
        sequencerView.repaint();
    };

    transportBar.onPlayStopClicked = [this] { togglePlayback(); };
    transportBar.onBpmChanged = [this] (int bpm)
    {
        audioEngine->setBpm (bpm);
    };
    transportBar.onVolumeChanged = [this] (float vol)
    {
        audioEngine->setVolume (vol);
    };
    transportBar.onLoadPlugin = [this] { loadPlugin(); };
    transportBar.onOpenPluginEditor = [this] { openPluginEditor(); };

    model.onChanged = [this] { handleModelChanged(); };

    // Seed a simple default progression so the UI is not empty on launch
    {
        const auto& presets = reharm::ProgressionPresets::all();
        if (! presets.empty())
            reharm::ProgressionPresets::applyToModel (presets.front(), model);
    }

    refreshAnalysis();
    updateEditorVisibility();

    setAudioChannels (0, 2);
    startTimer (50);
}

MainComponent::~MainComponent()
{
    stopTimer();
    stopPlayback();
    editorWindow.reset();
    shutdownAudio();
    setLookAndFeel (nullptr);
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    audioEngine->prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::MidiBuffer midiBuffer;
    audioEngine->processBlock (*bufferToFill.buffer, midiBuffer);
}

void MainComponent::releaseResources()
{
    audioEngine->releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    headerBar.setBounds (area.removeFromTop (kHeaderH));
    transportBar.setBounds (area.removeFromBottom (kTransportH));

    const int editorH = display.hasSelection() ? kEditorH : 0;
    if (editorH > 0)
        chordEditor.setBounds (area.removeFromBottom (editorH));
    else
        chordEditor.setBounds ({});

    analysisStrip.setBounds (area.removeFromBottom (kAnalysisH));
    sequencerView.setBounds (area);
}

void MainComponent::updateEditorVisibility()
{
    chordEditor.setVisible (display.hasSelection());
    resized();
}

void MainComponent::handleModelChanged()
{
    sequencerView.repaint();
    refreshAnalysis();
    chordEditor.refreshFromSelection();

    if (isPlaying)
        pushChordDataToEngine();
}

void MainComponent::refreshAnalysis()
{
    const auto analyses = reharm::HarmonyAnalyzer::analyzeAll (model);
    sequencerView.setAnalyses (analyses);

    const auto patterns = reharm::HarmonyAnalyzer::detectPatterns (model);
    analysisStrip.setPatterns (patterns, model);
}

void MainComponent::pushChordDataToEngine()
{
    auto flat = model.flatten();

    // Trim trailing rests so playback loops right after the last populated slot.
    // Interior rests (followed by a chord later) are preserved.
    while (! flat.empty() && ! flat.back().chord.has_value())
        flat.pop_back();

    std::vector<std::vector<int>> notes;

    std::vector<double> beats;
    notes.reserve (flat.size());
    beats.reserve (flat.size());

    for (const auto& fc : flat)
    {
        beats.push_back (fc.beats);
        if (fc.chord.has_value())
            notes.push_back (reharm::Voicing::midiNotes (*fc.chord, display.voicingStyle));
        else
            notes.emplace_back(); // rest: empty note list
    }

    auto data = std::make_shared<ChordData> (std::move (notes), std::move (beats));
    audioEngine->setChordData (data);
}

void MainComponent::startPlayback()
{
    pushChordDataToEngine();
    audioEngine->startPlayback();
    isPlaying = true;
    transportBar.setPlaying (true);
}

void MainComponent::stopPlayback()
{
    audioEngine->stopPlayback();
    isPlaying = false;
    transportBar.setPlaying (false);
    sequencerView.setPlayingFlatIndex (-1);
}

void MainComponent::togglePlayback()
{
    if (isPlaying)
        stopPlayback();
    else
        startPlayback();
}

void MainComponent::timerCallback()
{
    // Preserve original plugin-ready polling behaviour
    if (audioEngine->getPluginInstance() != nullptr)
    {
        // Mark ready once instance exists (original stopped the timer; we keep it
        // running for playhead polling and only set ready when needed).
        static bool marked = false;
        if (! marked)
        {
            audioEngine->setPluginReady (true);
            marked = true;
        }
    }

    if (isPlaying)
    {
        const int slot = audioEngine->getCurrentPlayingSlot();
        sequencerView.setPlayingFlatIndex (slot);
    }
    else
    {
        sequencerView.setPlayingFlatIndex (-1);
    }
}

void MainComponent::markPluginAsReady()
{
    debugLog ("markPluginAsReady: called");
    audioEngine->setPluginReady (true);
    debugLog ("markPluginAsReady: plugin ready set");
}

void MainComponent::openPluginEditor()
{
    debugLog ("openPluginEditor: ENTER");
    auto* plugin = audioEngine->getPluginInstance();
    if (plugin == nullptr)
    {
        debugLog ("openPluginEditor: no plugin loaded");
        return;
    }

    if (editorWindow != nullptr)
    {
        debugLog ("openPluginEditor: existing window, making visible");
        editorWindow->setVisible (true);
        return;
    }

    debugLog ("openPluginEditor: creating editor (plugin=" + plugin->getName() + ")");
    auto* editor = plugin->createEditorAndMakeActive();
    if (editor != nullptr)
    {
        debugLog ("openPluginEditor: editor created, creating window");
        auto window = std::make_unique<PluginEditorWindow> (plugin->getName(), editor);
        window->setVisible (true);
        editorWindow = std::move (window);
        debugLog ("openPluginEditor: editor window created and shown");
    }
    else
    {
        debugLog ("openPluginEditor: plugin has no editor");
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

                                  if (! descriptions.isEmpty())
                                  {
                                      auto setup = deviceManager.getAudioDeviceSetup();
                                      double sampleRate = setup.sampleRate;
                                      int blockSize = setup.bufferSize;
                                      if (sampleRate <= 0.0)
                                          sampleRate = 44100.0;
                                      if (blockSize <= 0)
                                          blockSize = 512;

                                      debugLog ("loadPlugin: using device setup SR=" + juce::String (sampleRate)
                                                + ", BS=" + juce::String (blockSize)
                                                + " (device stays open; load gated via beginPluginLoad)");

                                      audioEngine->beginPluginLoad();

                                      debugLog ("loadPlugin: calling createInstanceFromDescription...");
                                      juce::String error;
                                      auto descCopy = *descriptions[0];
                                      debugLog ("loadPlugin: desc name=" + descCopy.name);
                                      auto pluginInstance = format.createInstanceFromDescription (descCopy, sampleRate, blockSize, error);

                                      audioEngine->endPluginLoad();

                                      if (pluginInstance != nullptr)
                                      {
                                          debugLog ("loadPlugin: createInstanceFromDescription succeeded");

                                          auto pluginName = pluginInstance->getName();
                                          audioEngine->loadPluginInstance (std::move (pluginInstance), sampleRate, blockSize);
                                          debugLog ("Plugin loaded successfully");

                                          transportBar.setPluginName (pluginName);
                                          debugLog ("Plugin loaded UI update: " + pluginName);
                                      }
                                      else
                                      {
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
