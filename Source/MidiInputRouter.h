#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

namespace reharm
{

/**
 * Live MIDI input. Opens the selected hardware/virtual input and hands its
 * messages to the audio callback, which merges them into the same MidiBuffer
 * the sequenced chords use — so incoming notes are voiced by the loaded VST3,
 * or by the fallback synth when no plugin is loaded.
 *
 * Threading: setDevice()/restoreDevice()/prepare() run on the message thread,
 * handleIncomingMidiMessage() on the MIDI thread, fillNextBlock() on the audio
 * thread. juce::MidiMessageCollector does the hand-off between them.
 */
class MidiInputRouter : private juce::MidiInputCallback
{
public:
    MidiInputRouter() = default;
    ~MidiInputRouter() override;

    /** Input devices currently connected to the system. */
    static juce::Array<juce::MidiDeviceInfo> availableDevices();

    /** Picks which device a stored (identifier, name) pair refers to: the
        identifier wins, with the name as a fallback because the OS may hand out
        a different identifier once a device is reconnected. Returns an empty
        string when neither matches anything that is plugged in. */
    static juce::String resolveDeviceIdentifier (const juce::Array<juce::MidiDeviceInfo>& devices,
                                                 const juce::String& storedIdentifier,
                                                 const juce::String& storedName);

    /** Must be called (from prepareToPlay) before any message can be collected:
        the collector needs the sample rate to time-stamp incoming events. */
    void prepare (double sampleRate);

    /** Audio thread: appends everything received since the previous block.
        Messages already in destination (sequenced chords) are kept. */
    void fillNextBlock (juce::MidiBuffer& destination, int numSamples);

    /** Opens the device with this identifier; an empty identifier closes the
        current one. Returns true when a device is open afterwards. */
    bool setDevice (const juce::String& identifier);

    /** Reopens the device saved in a previous session (see resolveDeviceIdentifier). */
    bool restoreDevice (const juce::String& storedIdentifier, const juce::String& storedName);

    /** Identifier of the open device, or empty when MIDI input is off. */
    juce::String getDeviceIdentifier() const { return openIdentifier; }
    juce::String getDeviceName() const       { return openName; }
    bool hasDevice() const                   { return input != nullptr; }

private:
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    void closeDevice();

    std::unique_ptr<juce::MidiInput> input;
    juce::String openIdentifier;
    juce::String openName;

    juce::MidiMessageCollector collector;
    // The collector asserts (and would mis-time events) until reset() has run,
    // so every access is gated on this flag rather than on the device state.
    std::atomic<bool> collectorReady { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputRouter)
};

} // namespace reharm
