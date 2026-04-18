#pragma once

#include <JuceHeader.h>
#include "ChordProgressionGenerator.h"
#include "AudioEngine.h"
#include "ChordTheory.h"

class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow (juce::String name, juce::AudioProcessorEditor* editor)
        : juce::DocumentWindow (name,
                                juce::Colours::darkgrey,
                                juce::DocumentWindow::closeButton)
    {
        setContentNonOwned (editor, true);
        setResizable (false, false); // Default to non-resizable
        centreWithSize (editor->getWidth(), editor->getHeight());
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }
};

class MainComponent  : public juce::AudioAppComponent,
                       public juce::ChangeListener,
                       public juce::Button::Listener,
                       public juce::ComboBox::Listener,
                       public juce::Slider::Listener,
                       public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBox) override;
    void sliderValueChanged (juce::Slider* slider) override;

    void timerCallback() override;
    void markPluginAsReady();


private:
    void loadPlugin();
    void openPluginEditor();
    void playChordProgression();
    void stopPlayback();

    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<AudioEngine> audioEngine;

    juce::TextButton loadButton { "Load VST3" };
    juce::TextButton pluginNameButton { "No Plugin Loaded" };
    juce::TextButton playButton { "Play Progression" };
    juce::TextButton stopButton { "Stop" };
    juce::ComboBox keyComboBox;
    juce::ComboBox scaleComboBox;
    juce::TextEditor progressionDisplay;

    juce::Slider volumeSlider;
    juce::Slider bpmSlider;
    juce::TextEditor volumeLabel;
    juce::TextEditor bpmLabel;

    ChordProgressionGenerator generator;

    std::unique_ptr<PluginEditorWindow> editorWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
