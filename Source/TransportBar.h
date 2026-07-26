#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "PlaybackSettings.h"
#include "PlayModeSelection.h"
#include "RecentPlugins.h"

/**
 * Circular play/stop toggle painted custom.
 */
class PlayStopButton : public juce::Component
{
public:
    PlayStopButton();

    void setPlaying (bool playing);
    bool isPlaying() const noexcept { return playingState; }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    bool playingState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayStopButton)
};

/**
 * Draggable "MIDI" badge: dragging it out of the app exports the current
 * progression as a .mid file (drag & drop into a DAW).
 */
class MidiDragBadge : public juce::Component
{
public:
    MidiDragBadge();

    /** Returns the .mid file to drag, or an invalid/nonexistent File to cancel. */
    std::function<juce::File()> onPrepareDragFile;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    bool dragStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiDragBadge)
};

/**
 * TextButton subclass that reports right-clicks separately from left-clicks,
 * so callers can show a context menu without disturbing the normal left-click
 * action. Drawn ComboBox-style via ComboButton (no dropdown arrow: the plain
 * click is an action, not a menu).
 */
class RightClickableButton : public ComboButton
{
public:
    using ComboButton::ComboButton;
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        ComboButton::mouseDown (e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        ComboButton::mouseUp (e);
    }
};

/**
 * Transport: play/stop, BPM, volume, play-style (voicing/arpeggio) picker,
 * VST3 load + plugin name.
 */
class TransportBar : public juce::Component
{
public:
    TransportBar();
    ~TransportBar() override = default;

    void setPlaying (bool playing);
    void setBpm (int bpm);
    void setVolume (float volume01); // 0..1
    /** Empty name = no plugin (the button falls back to "Load VST3"). */
    void setPlugin (const juce::String& name, const juce::String& path);
    /** Previously loaded plugins, most recent first, for the button's menu. */
    void setRecentPlugins (const reharm::RecentPluginList& plugins);
    void setVoicing (reharm::Voicing::Style voicing);
    void setPlayStyle (reharm::ArpPattern pattern);
    void setArpRate (reharm::ArpRate rate);

    std::function<void()> onPlayStopClicked;
    std::function<void (int bpm)> onBpmChanged;
    std::function<void (float volume01)> onVolumeChanged;
    std::function<void()> onLoadPlugin;
    std::function<void()> onOpenPluginEditor;
    std::function<juce::File()> onPrepareMidiDragFile;
    std::function<void()> onUnloadPlugin;
    std::function<void (const juce::String& path)> onRecentPluginSelected;
    std::function<void (reharm::Voicing::Style)> onVoicingChanged;
    std::function<void (reharm::ArpPattern)> onPlayStyleChanged;
    std::function<void (reharm::ArpRate)> onArpRateChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    PlayStopButton playStop;
    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    ComboButton playModeButton;
    /** Load VST3 when empty, the plugin's name (opens its editor) once loaded. */
    RightClickableButton pluginButton { "Load VST3" };
    MidiDragBadge midiBadge;
    bool pluginLoaded = false;
    juce::String activePluginPath;
    reharm::RecentPluginList recentPlugins;

    reharm::PlayModeSelection selection;

    void showPlayModeMenu();
    void showPluginMenu();
    void handlePlayModeMenuResult (int resultId);
    void updatePlayModeButtonLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};
