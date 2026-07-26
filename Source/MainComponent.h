#pragma once

#include <JuceHeader.h>
#include "AudioEngine.h"
#include "ProgressionModel.h"
#include "HarmonyAnalyzer.h"
#include "Theme.h"
#include "DisplayState.h"
#include "HeaderBar.h"
#include "SequencerView.h"
#include "AnalysisStrip.h"
#include "ChordEditorPanel.h"
#include "TransportBar.h"
#include "StateStore.h"
#include "UndoHistory.h"
#include "PlaybackBuilder.h"
#include "MidiInputRouter.h"
#include "SettingsPanel.h"

class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow (juce::String name, juce::AudioProcessorEditor* editor)
        : juce::DocumentWindow (name,
                                juce::Colours::darkgrey,
                                juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor, true);
        setResizable (false, false);
        centreWithSize (editor->getWidth(), editor->getHeight());
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }
};

namespace CommandIDs
{
    enum
    {
        undo = 1,
        redo,
        cut,
        copy,
        paste,
        deleteChord
    };
}

class MainComponent : public juce::AudioAppComponent,
                      public juce::Timer,
                      public juce::KeyListener,
                      public juce::MenuBarModel,
                      public juce::ApplicationCommandTarget
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override;
    void visibilityChanged() override;

    void timerCallback() override;

    // juce::MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    // juce::ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
    bool perform (const InvocationInfo& info) override;

private:
    void loadPlugin();
    /** silentOnError=true suppresses failure dialogs, logging via debugLog only
        (used for startup restore). */
    void loadPluginFromFile (const juce::File& file,
                             std::unique_ptr<juce::MemoryBlock> stateToApply,
                             bool silentOnError);
    void openPluginEditor();
    void startPlayback();
    void stopPlayback();
    void togglePlayback();
    void pushChordDataToEngine();
    void handleModelChanged();
    void refreshAnalysis();
    void updateEditorVisibility();

    reharm::PlaybackSettings currentPlaybackSettings() const;

    void markSessionDirty();
    void saveSessionNow();
    void refreshPluginStateCache();
    void promptSaveUserPreset();
    void doSaveUserPreset (const juce::String& name);
    void loadUserPreset (const juce::String& name);

    void applyHistoryState (const reharm::ProgressionState& s);
    void performUndo();
    void performRedo();
    void validateSelection();

    void performCopy();
    void performPaste();
    void performCut();
    void performDeleteSelection();
    void showAboutDialog();
    void showSettingsDialog();
    /** Opens (or, with an empty identifier, closes) the live MIDI input and
        remembers the choice in the session. */
    void applyMidiInputDevice (const juce::String& identifier, const juce::String& name);
    /** Opens the MIDI input stored in the session, if it is connected. */
    void reopenSavedMidiInput();
    void unloadPlugin();

    std::unique_ptr<ReharmLookAndFeel> lookAndFeel;
    std::unique_ptr<AudioEngine> audioEngine;
    reharm::MidiInputRouter midiInput;

    juce::ApplicationCommandManager commandManager;
   #if JUCE_MAC
    juce::PopupMenu extraAppleMenu;
   #endif

    reharm::ProgressionModel model;
    DisplayState display;
    reharm::UndoHistory undoHistory;
    bool restoringHistory = false;
    /** Set while a user preset is being applied: the model lands before the preset's
        play settings do, so the engine must not be rebuilt from the outgoing ones. */
    bool applyingPreset = false;
    bool isPlaying = false;
    std::vector<int> currentDisplayMap;

    reharm::StateStore stateStore { reharm::StateStore::createDefault() };
    reharm::StateStore::SessionData sessionData;
    juce::String lastUserPresetName;

    struct DebounceTimer : juce::Timer
    {
        std::function<void()> onFire;
        void timerCallback() override { stopTimer(); if (onFire) onFire(); }
    };
    DebounceTimer autosaveTimer;

    HeaderBar headerBar;
    SequencerView sequencerView;
    AnalysisStrip analysisStrip;
    ChordEditorPanel chordEditor;
    TransportBar transportBar;

    std::unique_ptr<PluginEditorWindow> editorWindow;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;

    static constexpr int kHeaderH = 56;
    static constexpr int kAnalysisH = 40;
    static constexpr int kEditorH = 224;
    static constexpr int kTransportH = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
