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

class MainComponent : public juce::AudioAppComponent,
                      public juce::Timer,
                      public juce::KeyListener
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

    void markSessionDirty();
    void saveSessionNow();
    void refreshPluginStateCache();
    void promptSaveUserPreset();
    void doSaveUserPreset (const juce::String& name);
    void loadUserPreset (const juce::String& name);

    std::unique_ptr<ReharmLookAndFeel> lookAndFeel;
    std::unique_ptr<AudioEngine> audioEngine;

    reharm::ProgressionModel model;
    DisplayState display;
    bool isPlaying = false;

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

    static constexpr int kHeaderH = 56;
    static constexpr int kAnalysisH = 40;
    static constexpr int kEditorH = 224;
    static constexpr int kTransportH = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
