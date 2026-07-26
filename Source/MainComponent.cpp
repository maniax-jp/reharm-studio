#include "MainComponent.h"
#include "Localization.h"
#include "MidiExporter.h"
#include "BinaryData.h"
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

namespace
{

/** Content of the custom About dialog: app icon, name, and version. */
class AboutContent : public juce::Component
{
public:
    AboutContent()
    {
        appIcon = juce::ImageCache::getFromMemory (BinaryData::AppIcon_1024_png,
                                                    BinaryData::AppIcon_1024_pngSize);

        nameLabel.setText ("Reharm Studio", juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setFont (theme::font (18.0f, true));
        nameLabel.setColour (juce::Label::textColourId, theme::text);
        addAndMakeVisible (nameLabel);

        const juce::String versionText = reharm::loc::tr ("Version") + " "
                                            + juce::String (ProjectInfo::versionString);
        versionLabel.setText (versionText, juce::dontSendNotification);
        versionLabel.setJustificationType (juce::Justification::centred);
        versionLabel.setFont (theme::font (13.0f));
        versionLabel.setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (versionLabel);

        // Button text comes from the member initialiser ("OK" — shown in English).
        okButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };
        addAndMakeVisible (okButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::surface);

        if (appIcon.isValid())
        {
            const int iconSize = 128;
            auto iconArea = juce::Rectangle<int> (0, 20, getWidth(), iconSize);
            g.drawImage (appIcon, iconArea.withSizeKeepingCentre (iconSize, iconSize).toFloat(),
                        juce::RectanglePlacement::centred);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (156); // icon area
        nameLabel.setBounds (area.removeFromTop (26));
        versionLabel.setBounds (area.removeFromTop (20));
        area.removeFromTop (12);
        const int buttonW = 80;
        okButton.setBounds (area.removeFromTop (28).withSizeKeepingCentre (buttonW, 28));
    }

private:
    juce::Image appIcon;
    juce::Label nameLabel;
    juce::Label versionLabel;
    juce::TextButton okButton { "OK" };
};

} // namespace

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
    transportBar.setVoicing (display.voicingStyle);


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

    headerBar.onSaveRequested = [this] { promptSaveUserPreset(); };
    headerBar.onSettingsRequested = [this] { showSettingsDialog(); };
    headerBar.onUserPresetSelected = [this] (const juce::String& name) { loadUserPreset (name); };
    headerBar.onUserPresetDeleteRequested = [this] (const juce::String& name)
    {
        juce::NativeMessageBox::showOkCancelBox (
            juce::MessageBoxIconType::QuestionIcon,
            reharm::loc::tr ("Delete Preset"),
            reharm::loc::deleteConfirmMessage (name),
            this,
            juce::ModalCallbackFunction::create ([this, name] (int ok)
            {
                if (ok != 1)
                    return;
                if (stateStore.deleteUserPreset (name))
                {
                    if (lastUserPresetName == name)
                        lastUserPresetName.clear();
                    headerBar.setUserPresets (stateStore.listUserPresetNames());
                }
            }));
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
    transportBar.onUnloadPlugin = [this] { unloadPlugin(); };
    transportBar.onVoicingChanged = [this] (reharm::Voicing::Style v)
    {
        display.voicingStyle = v;
        sessionData.voicing = v;
        if (isPlaying)
            pushChordDataToEngine();
        markSessionDirty();
    };
    transportBar.onPlayStyleChanged = [this] (reharm::ArpPattern p)
    {
        display.arpPattern = p;
        sessionData.arpPattern = p;          // package A autosave picks this up
        if (isPlaying)
            pushChordDataToEngine();
        markSessionDirty();
    };
    transportBar.onArpRateChanged = [this] (reharm::ArpRate r)
    {
        display.arpRate = r;
        sessionData.arpRate = r;
        if (isPlaying)
            pushChordDataToEngine();
        markSessionDirty();
    };
    transportBar.onPrepareMidiDragFile = [this]() -> juce::File
    {
        const auto settings = currentPlaybackSettings();
        auto data = reharm::PlaybackBuilder::build (model, settings);
        if (data == nullptr || data->totalChords == 0)
            return {};

        auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("Reharm Progression.mid");
        if (! reharm::MidiExporter::writeMidiFile (*data, sessionData.bpm, dest))
            return {};
        return dest;
    };

    model.onChanged = [this] { handleModelChanged(); };

    autosaveTimer.onFire = [this] { saveSessionNow(); };

    const bool restored = stateStore.loadSession (model, sessionData);
    if (restored)
    {
        headerBar.setKey (model.getKey().tonicPitchClass, model.getKey().isMajor);
        display.voicingStyle = sessionData.voicing;
        transportBar.setVoicing (sessionData.voicing);
        display.arpPattern = sessionData.arpPattern;
        display.arpRate = sessionData.arpRate;
        transportBar.setPlayStyle (sessionData.arpPattern);
        transportBar.setArpRate (sessionData.arpRate);
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

    // After setAudioChannels so prepareToPlay has given the collector its sample
    // rate: messages arriving before that would be dropped.
    if (restored)
        reopenSavedMidiInput();

    commandManager.registerAllCommandsForTarget (this);
    setApplicationCommandManagerToWatch (&commandManager);

   #if JUCE_MAC
    {
        // Localise the JUCE-generated Application menu (Services / Hide / Hide Others /
        // Show All / Quit). juce_MainMenu_mac.mm calls TRANS() with these exact
        // single-word keys, so registering mappings before setMacMainMenu() is enough;
        // existing loc::tr()-based UI is unaffected since it never goes through TRANS().
        //
        // LocalisedStrings only exposes translation-file text (or a File) as a public
        // constructor, so building that text is the intended route. Keys and values are
        // quoted per line; quoteEntry escapes any embedded quote so a translation
        // containing one cannot break the parse.
        static constexpr const char* menuKeys[] = { "Services", "Hide", "Hide Others",
                                                    "Show All", "Quit" };

        auto quoteEntry = [] (const juce::String& key, const juce::String& value)
        {
            return "\"" + key.replace ("\"", "\\\"") + "\" = \""
                        + value.replace ("\"", "\\\"") + "\"\n";
        };

        juce::String translationText;
        for (auto* key : menuKeys)
            translationText << quoteEntry (key, reharm::loc::tr (key));

        juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (translationText, false));
    }

    extraAppleMenu.addItem (reharm::loc::tr ("About Reharm Studio"), [this] { showAboutDialog(); });
    extraAppleMenu.addSeparator();
    extraAppleMenu.addItem (reharm::loc::tr ("Settings"), [this] { showSettingsDialog(); });
    juce::MenuBarModel::setMacMainMenu (this, &extraAppleMenu);
   #endif

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
    if (mods.isCommandDown() && lower == 'x')
    {
        performCut();
        return true;
    }
    if (mods.isCommandDown() && lower == 'c')
    {
        performCopy();
        return true;
    }
    if (mods.isCommandDown() && lower == 'v')
    {
        performPaste();
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        performDeleteSelection();
        return true;
    }

    return false;
}

void MainComponent::performCopy()
{
    if (display.hasSelection())
        if (auto c = model.getChord (display.selectedBar, display.selectedSlot))
        {
            display.chordClipboard = c;
            commandManager.commandStatusChanged();
        }
}

void MainComponent::performPaste()
{
    if (display.hasSelection() && display.chordClipboard.has_value())
        model.setChord (display.selectedBar, display.selectedSlot, display.chordClipboard);
}

void MainComponent::performCut()
{
    const bool hadChord = display.hasSelection()
        && model.getChord (display.selectedBar, display.selectedSlot).has_value();
    if (! hadChord)
        return;

    performCopy();
    performDeleteSelection();
}

void MainComponent::performDeleteSelection()
{
    if (display.hasSelection())
    {
        model.setChord (display.selectedBar, display.selectedSlot, std::nullopt);
        display.clearSelection();
        updateEditorVisibility();
        sequencerView.repaint();
    }
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
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif

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
    midiInput.prepare (sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::MidiBuffer midiBuffer;

    // Live input first; the engine then adds its sequenced chord events to the
    // same buffer, so played notes and the progression share the instrument.
    midiInput.fillNextBlock (midiBuffer, bufferToFill.numSamples);

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
    commandManager.commandStatusChanged();
}

void MainComponent::handleModelChanged()
{
    if (! restoringHistory)
        undoHistory.commit (model.captureState());

    sequencerView.markLayoutDirty();
    sequencerView.repaint();
    refreshAnalysis();
    chordEditor.refreshFromSelection();

    if (isPlaying && ! applyingPreset)
        pushChordDataToEngine();

    markSessionDirty();
    commandManager.commandStatusChanged();
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

    // A preset carries the progression and the play settings together, but the model
    // is applied inside loadUserPreset and fires onChanged before the settings below
    // are in place. Without this gate that notification rebuilt the engine from the
    // outgoing settings and nothing pushed again, so a preset's arpeggio showed up on
    // the button while playback stayed on the previous style until the next edit.
    applyingPreset = true;
    const bool loadedOk = stateStore.loadUserPreset (name, model, loaded);
    applyingPreset = false;

    if (loadedOk)
    {
        // Only the musical fields are applied (plugin/volume are not in the preset).
        sessionData.bpm = loaded.bpm;
        sessionData.voicing = loaded.voicing;
        sessionData.arpPattern = loaded.arpPattern;
        sessionData.arpRate = loaded.arpRate;
        display.voicingStyle = loaded.voicing;
        display.arpPattern = loaded.arpPattern;
        display.arpRate = loaded.arpRate;
        headerBar.setKey (model.getKey().tonicPitchClass, model.getKey().isMajor);
        transportBar.setVoicing (loaded.voicing);
        transportBar.setPlayStyle (loaded.arpPattern);
        transportBar.setArpRate (loaded.arpRate);
        transportBar.setBpm (loaded.bpm);
        audioEngine->setBpm (loaded.bpm);
        lastUserPresetName = name;
        display.clearSelection();
        updateEditorVisibility();

        // Now that the settings are in place, rebuild playback once so the engine and
        // the play-mode button agree.
        if (isPlaying)
            pushChordDataToEngine();

        markSessionDirty();
    }
}

reharm::PlaybackSettings MainComponent::currentPlaybackSettings() const
{
    return { display.voicingStyle, display.arpPattern, display.arpRate };
}

void MainComponent::pushChordDataToEngine()
{
    const auto settings = currentPlaybackSettings();
    auto data = reharm::PlaybackBuilder::build (model, settings);
    currentDisplayMap = data->displayIndices;
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
        int flatIndex = -1;
        if (slot >= 0)
        {
            if (slot < (int) currentDisplayMap.size())
                flatIndex = currentDisplayMap[(size_t) slot];
            // Out of map range (engine briefly still playing stale data) -> -1 = no highlight.
        }
        sequencerView.setPlayingFlatIndex (flatIndex);
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

void MainComponent::unloadPlugin()
{
    stopPlayback();
    editorWindow.reset();
    audioEngine->unloadPlugin();

    sessionData.pluginPath = {};
    sessionData.pluginName = {};
    sessionData.pluginStateB64 = {};

    transportBar.setPluginName ({});
    markSessionDirty();
}

//==============================================================================
// juce::MenuBarModel

juce::StringArray MainComponent::getMenuBarNames()
{
    return { reharm::loc::tr ("Edit"), reharm::loc::tr ("Help") };
}

juce::PopupMenu MainComponent::getMenuForIndex (int topLevelMenuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    if (topLevelMenuIndex == 0)
    {
        menu.addCommandItem (&commandManager, CommandIDs::undo);
        menu.addCommandItem (&commandManager, CommandIDs::redo);
        menu.addSeparator();
        menu.addCommandItem (&commandManager, CommandIDs::cut);
        menu.addCommandItem (&commandManager, CommandIDs::copy);
        menu.addCommandItem (&commandManager, CommandIDs::paste);
        menu.addCommandItem (&commandManager, CommandIDs::deleteChord);
    }
    else if (topLevelMenuIndex == 1)
    {
        menu.addItem (reharm::loc::tr ("Version Info"), [this] { showAboutDialog(); });
    }

    return menu;
}

void MainComponent::menuItemSelected (int, int)
{
}

//==============================================================================
// juce::ApplicationCommandTarget

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    commands.addArray ({ CommandIDs::undo, CommandIDs::redo,
                         CommandIDs::cut, CommandIDs::copy,
                         CommandIDs::paste, CommandIDs::deleteChord });
}

void MainComponent::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    switch (commandID)
    {
        case CommandIDs::undo:
            result.setInfo (reharm::loc::tr ("Undo"), reharm::loc::tr ("Undo"), "Edit", 0);
            result.addDefaultKeypress ('z', juce::ModifierKeys::commandModifier);
            result.setActive (undoHistory.canUndo());
            break;
        case CommandIDs::redo:
            result.setInfo (reharm::loc::tr ("Redo"), reharm::loc::tr ("Redo"), "Edit", 0);
            result.addDefaultKeypress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
            result.setActive (undoHistory.canRedo());
            break;
        case CommandIDs::cut:
            result.setInfo (reharm::loc::tr ("Cut"), reharm::loc::tr ("Cut"), "Edit", 0);
            result.addDefaultKeypress ('x', juce::ModifierKeys::commandModifier);
            result.setActive (display.hasSelection()
                              && model.getChord (display.selectedBar, display.selectedSlot).has_value());
            break;
        case CommandIDs::copy:
            result.setInfo (reharm::loc::tr ("Copy"), reharm::loc::tr ("Copy"), "Edit", 0);
            result.addDefaultKeypress ('c', juce::ModifierKeys::commandModifier);
            result.setActive (display.hasSelection()
                              && model.getChord (display.selectedBar, display.selectedSlot).has_value());
            break;
        case CommandIDs::paste:
            result.setInfo (reharm::loc::tr ("Paste"), reharm::loc::tr ("Paste"), "Edit", 0);
            result.addDefaultKeypress ('v', juce::ModifierKeys::commandModifier);
            result.setActive (display.hasSelection() && display.chordClipboard.has_value());
            break;
        case CommandIDs::deleteChord:
            result.setInfo (reharm::loc::tr ("Delete"), reharm::loc::tr ("Delete"), "Edit", 0);
            result.addDefaultKeypress (juce::KeyPress::backspaceKey, juce::ModifierKeys::noModifiers);
            result.setActive (display.hasSelection());
            break;
        default:
            break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::undo:        performUndo(); return true;
        case CommandIDs::redo:        performRedo(); return true;
        case CommandIDs::cut:         performCut(); return true;
        case CommandIDs::copy:        performCopy(); return true;
        case CommandIDs::paste:       performPaste(); return true;
        case CommandIDs::deleteChord: performDeleteSelection(); return true;
        default: break;
    }
    return false;
}

void MainComponent::applyMidiInputDevice (const juce::String& identifier, const juce::String& name)
{
    midiInput.setDevice (identifier);

    // What the user picked is stored even if the device failed to open, so the
    // settings screen can keep showing that choice (greyed out) instead of
    // silently falling back to "no MIDI input".
    sessionData.midiInputId = identifier;
    sessionData.midiInputName = name;
    markSessionDirty();
}

void MainComponent::reopenSavedMidiInput()
{
    if (sessionData.midiInputId.isEmpty() && sessionData.midiInputName.isEmpty())
        return;

    if (! midiInput.restoreDevice (sessionData.midiInputId, sessionData.midiInputName))
        return;   // still unplugged: keep the stored choice so it can be shown

    // A reconnected device may come back under a new identifier.
    sessionData.midiInputId = midiInput.getDeviceIdentifier();
    sessionData.midiInputName = midiInput.getDeviceName();
    markSessionDirty();
}

void MainComponent::showSettingsDialog()
{
    if (settingsWindow != nullptr)
    {
        // Two ways in (gear button and the application menu), one window.
        settingsWindow->toFront (true);
        return;
    }

    auto* content = new SettingsPanel();
    content->setSize (380, 100);
    content->setLookAndFeel (&getLookAndFeel());

    // The dialog is not modal and outlives this call, so both the poll timer and
    // the selection callback must survive this component going away.
    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::Component::SafePointer<SettingsPanel> safePanel (content);

    auto refresh = [safeThis, safePanel]
    {
        if (safeThis == nullptr || safePanel == nullptr)
            return;

        // Opening the screen, and every poll after it, is also when a device
        // that was unplugged gets picked up again, so what the list shows and
        // what is actually open agree.
        safeThis->reopenSavedMidiInput();

        safePanel->setMidiInputs (reharm::MidiInputRouter::availableDevices(),
                                  safeThis->midiInput.getDeviceIdentifier(),
                                  safeThis->sessionData.midiInputName);
    };

    content->onRefreshRequested = refresh;
    content->onMidiInputSelected = [safeThis, refresh] (const juce::String& identifier,
                                                        const juce::String& name)
    {
        if (safeThis == nullptr)
            return;
        safeThis->applyMidiInputDevice (identifier, name);
        refresh();
    };

    refresh();

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = reharm::loc::tr ("Settings");
    options.dialogBackgroundColour = theme::surface;
    options.content.setOwned (content);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    settingsWindow = options.launchAsync();
}

void MainComponent::showAboutDialog()
{
    auto* content = new AboutContent();
    content->setSize (320, 240);
    if (auto* lnf = &getLookAndFeel())
        content->setLookAndFeel (lnf);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Reharm Studio";
    options.dialogBackgroundColour = theme::surface;
    options.content.setOwned (content);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.launchAsync();
}
