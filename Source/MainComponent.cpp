#include "MainComponent.h"
#include "Localization.h"
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
        sessionData.voicingClose = close;
        if (isPlaying)
            pushChordDataToEngine();
        markSessionDirty();
    };

    headerBar.onSaveRequested = [this] { promptSaveUserPreset(); };
    headerBar.onUserPresetSelected = [this] (const juce::String& name) { loadUserPreset (name); };

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
        sessionData.bpm = bpm;
        audioEngine->setBpm (bpm);
        markSessionDirty();
    };
    transportBar.onVolumeChanged = [this] (float vol)
    {
        sessionData.volume = vol;
        audioEngine->setVolume (vol);
        markSessionDirty();
    };
    transportBar.onLoadPlugin = [this] { loadPlugin(); };
    transportBar.onOpenPluginEditor = [this] { openPluginEditor(); };

    model.onChanged = [this] { handleModelChanged(); };

    autosaveTimer.onFire = [this] { saveSessionNow(); };

    const bool restored = stateStore.loadSession (model, sessionData);
    if (restored)
    {
        headerBar.setKey (model.getKey().tonicPitchClass, model.getKey().isMajor);
        display.voicingStyle = sessionData.voicingClose ? reharm::Voicing::Style::Close
                                                        : reharm::Voicing::Style::Open;
        headerBar.setVoicingClose (sessionData.voicingClose);
        transportBar.setBpm (sessionData.bpm);
        transportBar.setVolume (sessionData.volume);
        audioEngine->setBpm (sessionData.bpm);
        audioEngine->setVolume (sessionData.volume);
    }
    else
    {
        // Seed a simple default progression so the UI is not empty on launch
        const auto& presets = reharm::ProgressionPresets::all();
        if (! presets.empty())
            reharm::ProgressionPresets::applyToModel (presets.front(), model);
    }
    headerBar.setUserPresets (stateStore.listUserPresetNames());
    undoHistory.reset (model.captureState());

    refreshAnalysis();
    updateEditorVisibility();

    // Keep keyboard focus on this component so the space bar always toggles
    // playback: child controls must not steal focus when clicked.
    setWantsKeyboardFocus (true);
    std::function<void (juce::Component&)> disableClickFocus =
        [&disableClickFocus] (juce::Component& comp)
    {
        comp.setMouseClickGrabsKeyboardFocus (false);
        for (auto* child : comp.getChildren())
            disableClickFocus (*child);
    };
    disableClickFocus (*this);

    setAudioChannels (0, 2);
    startTimer (50);

    if (restored && sessionData.pluginPath.isNotEmpty())
    {
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe]
        {
            if (safe == nullptr) return;
            juce::File f (safe->sessionData.pluginPath);
            if (! f.exists()) return;
            std::unique_ptr<juce::MemoryBlock> state;
            if (safe->sessionData.pluginStateB64.isNotEmpty())
            {
                state = std::make_unique<juce::MemoryBlock>();
                juce::MemoryOutputStream mo (*state, false);
                juce::Base64::convertFromBase64 (mo, safe->sessionData.pluginStateB64);
            }
            safe->loadPluginFromFile (f, std::move (state), true);
        });
    }
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress (juce::KeyPress::spaceKey))
    {
        togglePlayback();
        return true;
    }

    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();
    const auto lower = (juce::juce_wchar) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code);

    if (mods.isCommandDown() && lower == 'z')
    {
        if (mods.isShiftDown()) performRedo(); else performUndo();
        return true;
    }
    if (mods.isCommandDown() && lower == 'c')
    {
        if (display.hasSelection())
            if (auto c = model.getChord (display.selectedBar, display.selectedSlot))
            {
                display.chordClipboard = c;
                return true;
            }
        return false;
    }
    if (mods.isCommandDown() && lower == 'v')
    {
        if (display.hasSelection() && display.chordClipboard.has_value())
        {
            model.setChord (display.selectedBar, display.selectedSlot, display.chordClipboard);
            return true;
        }
        return false;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        if (display.hasSelection())
        {
            model.setChord (display.selectedBar, display.selectedSlot, std::nullopt);
            display.clearSelection();
            updateEditorVisibility();
            sequencerView.repaint();
            return true;
        }
        return false;
    }

    return false;
}

// KeyListener callback: registered on the top-level window so the space bar
// works even when no component inside the window has keyboard focus.
bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return keyPressed (key);
}

void MainComponent::visibilityChanged()
{
    if (isShowing())
        grabKeyboardFocus();
}

MainComponent::~MainComponent()
{
    stopTimer();
    autosaveTimer.stopTimer();
    refreshPluginStateCache();
    saveSessionNow();
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

    // Honour startSample/numSamples: process only the requested region.
    juce::AudioBuffer<float> subBuffer (bufferToFill.buffer->getArrayOfWritePointers(),
                                        bufferToFill.buffer->getNumChannels(),
                                        bufferToFill.startSample,
                                        bufferToFill.numSamples);
    audioEngine->processBlock (subBuffer, midiBuffer);
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
    if (! restoringHistory)
        undoHistory.commit (model.captureState());

    sequencerView.markLayoutDirty();
    sequencerView.repaint();
    refreshAnalysis();
    chordEditor.refreshFromSelection();

    if (isPlaying)
        pushChordDataToEngine();

    markSessionDirty();
}

void MainComponent::applyHistoryState (const reharm::ProgressionState& s)
{
    restoringHistory = true;
    model.restoreState (s);
    restoringHistory = false;
    headerBar.setKey (model.getKey().tonicPitchClass, model.getKey().isMajor);
    validateSelection();
}

void MainComponent::performUndo()
{
    if (auto s = undoHistory.undo())
        applyHistoryState (*s);
}

void MainComponent::performRedo()
{
    if (auto s = undoHistory.redo())
        applyHistoryState (*s);
}

void MainComponent::validateSelection()
{
    if (! display.hasSelection())
        return;
    if (display.selectedBar >= model.getNumBars()
        || display.selectedSlot >= model.getBar (display.selectedBar).numSlots())
    {
        display.clearSelection();
        updateEditorVisibility();
        sequencerView.repaint();
    }
}

void MainComponent::refreshAnalysis()
{
    const auto analyses = reharm::HarmonyAnalyzer::analyzeAll (model);
    sequencerView.setAnalyses (analyses);

    const auto patterns = reharm::HarmonyAnalyzer::detectPatterns (model);
    analysisStrip.setPatterns (patterns, model);
}

void MainComponent::markSessionDirty()
{
    autosaveTimer.startTimer (1000);
}

void MainComponent::saveSessionNow()
{
    stateStore.saveSession (model, sessionData);
}

void MainComponent::refreshPluginStateCache()
{
    auto* plugin = audioEngine->getPluginInstance();
    if (plugin == nullptr)
        return;

    juce::MemoryBlock mb;
    plugin->getStateInformation (mb);
    sessionData.pluginStateB64 = mb.getSize() > 0
        ? juce::Base64::toBase64 (mb.getData(), mb.getSize())
        : juce::String();
}

void MainComponent::promptSaveUserPreset()
{
    juce::String defaultName = lastUserPresetName;
    if (defaultName.isEmpty())
    {
        const auto existing = stateStore.listUserPresetNames();
        int n = 1;
        auto candidate = [&n] { return reharm::loc::tr ("My Progression") + " " + juce::String (n); };
        while (existing.contains (candidate())) ++n;
        defaultName = candidate();
    }

    auto* aw = new juce::AlertWindow (reharm::loc::tr ("Save Progression"),
                                      reharm::loc::tr ("Progression name"),
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", defaultName);
    aw->addButton (reharm::loc::tr ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton (reharm::loc::tr ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        const auto name = reharm::StateStore::sanitizePresetName (aw->getTextEditorContents ("name"));
        if (result == 1 && name.isNotEmpty())
        {
            if (stateStore.userPresetExists (name))
            {
                juce::NativeMessageBox::showOkCancelBox (
                    juce::MessageBoxIconType::QuestionIcon,
                    reharm::loc::tr ("Save Progression"),
                    reharm::loc::tr ("Overwrite preset?"),
                    this,
                    juce::ModalCallbackFunction::create ([this, name] (int ok)
                    {
                        if (ok == 1)
                            doSaveUserPreset (name);
                    }));
            }
            else
            {
                doSaveUserPreset (name);
            }
        }
    }), true);   // deleteWhenDismissed = true
}

void MainComponent::doSaveUserPreset (const juce::String& name)
{
    if (stateStore.saveUserPreset (name, model, sessionData))
    {
        lastUserPresetName = name;
        headerBar.setUserPresets (stateStore.listUserPresetNames());
    }
}

void MainComponent::loadUserPreset (const juce::String& name)
{
    reharm::StateStore::SessionData loaded = sessionData;   // volume/plugin stay as-is
    if (stateStore.loadUserPreset (name, model, loaded))
    {
        // Only the musical fields are applied (plugin/volume are not in the preset).
        sessionData.bpm = loaded.bpm;
        sessionData.voicingClose = loaded.voicingClose;
        display.voicingStyle = loaded.voicingClose ? reharm::Voicing::Style::Close
                                                   : reharm::Voicing::Style::Open;
        headerBar.setKey (model.getKey().tonicPitchClass, model.getKey().isMajor);
        headerBar.setVoicingClose (loaded.voicingClose);
        transportBar.setBpm (loaded.bpm);
        audioEngine->setBpm (loaded.bpm);
        lastUserPresetName = name;
        display.clearSelection();
        updateEditorVisibility();
        markSessionDirty();
    }
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
                                  loadPluginFromFile (file, nullptr, false);
                          });
}

void MainComponent::loadPluginFromFile (const juce::File& file,
                                        std::unique_ptr<juce::MemoryBlock> stateToApply,
                                        bool silentOnError)
{
    debugLog ("loadPluginFromFile: file selected: " + file.getFullPathName());

    stopPlayback();
    editorWindow.reset();

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;

    debugLog ("loadPluginFromFile: scanning plugin file...");
    format.findAllTypesForFile (descriptions, file.getFullPathName());
    debugLog ("loadPluginFromFile: scan complete, descriptions count: " + juce::String (descriptions.size()));

    if (! descriptions.isEmpty())
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        double sampleRate = setup.sampleRate;
        int blockSize = setup.bufferSize;
        if (sampleRate <= 0.0)
            sampleRate = 44100.0;
        if (blockSize <= 0)
            blockSize = 512;

        debugLog ("loadPluginFromFile: using device setup SR=" + juce::String (sampleRate)
                  + ", BS=" + juce::String (blockSize)
                  + " (device stays open; load gated via beginPluginLoad)");

        // Prefer instrument plugins when multiple descriptions exist.
        const juce::PluginDescription* chosen = nullptr;
        const juce::PluginDescription* fallback = nullptr;
        for (auto* d : descriptions)
            {
                if (d == nullptr)
                    continue;

                if (fallback == nullptr)
                    fallback = d;

                if (d->isInstrument)
                {
                    chosen = d;
                    break;
                }
            }
        if (chosen == nullptr)
            chosen = fallback;

        if (chosen == nullptr)
            {
                debugLog ("No valid plugin description found");
                if (! silentOnError)
                    juce::NativeMessageBox::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Plugin Load Failed",
                        "No valid plugin description was found in the selected file.");
                return;
            }

        audioEngine->beginPluginLoad();

        debugLog ("loadPluginFromFile: calling createInstanceFromDescription...");
        juce::String error;
        auto descCopy = *chosen;
        debugLog ("loadPluginFromFile: desc name=" + descCopy.name);
        auto pluginInstance = format.createInstanceFromDescription (descCopy, sampleRate, blockSize, error);

        audioEngine->endPluginLoad();

        if (pluginInstance != nullptr)
        {
            debugLog ("loadPluginFromFile: createInstanceFromDescription succeeded");

            if (stateToApply != nullptr && stateToApply->getSize() > 0)
                pluginInstance->setStateInformation (stateToApply->getData(), (int) stateToApply->getSize());

            auto pluginName = pluginInstance->getName();
            audioEngine->loadPluginInstance (std::move (pluginInstance), sampleRate, blockSize);
            debugLog ("Plugin loaded successfully");

            transportBar.setPluginName (pluginName);
            debugLog ("Plugin loaded UI update: " + pluginName);

            sessionData.pluginPath = file.getFullPathName();
            sessionData.pluginName = pluginName;
            refreshPluginStateCache();
            markSessionDirty();
        }
        else
        {
            debugLog ("loadPluginFromFile: createInstanceFromDescription returned nullptr: " + error);
            if (! silentOnError)
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Plugin Load Failed",
                    "Failed to create plugin instance:\n" + error);
        }
    }
    else
    {
        debugLog ("No valid VST3 plugin found");
        if (! silentOnError)
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Plugin Load Failed",
                "No valid VST3 plugin was found in the selected file.");
    }
}
