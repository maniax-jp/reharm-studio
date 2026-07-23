#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "AudioEngine.h"

namespace reharm
{

/** Converts a built ChordData sequence into a Standard MIDI File. */
class MidiExporter
{
public:
    static constexpr int ticksPerQuarterNote = 960;
    static constexpr int midiChannel = 1;
    static constexpr juce::uint8 noteVelocity = 96;

    /** Single-track SMF: tempo + 4/4 time signature + note events.
        Slot i: noteOn at cumBeats[i]*960 ticks, noteOff at cumBeats[i+1]*960 ticks
        for each note in notes[i]. Rest slots (empty notes) produce no events. */
    static juce::MidiFile buildMidiFile (const ChordData& data, int bpm);

    /** buildMidiFile + write to dest (overwrite). Returns success. */
    static bool writeMidiFile (const ChordData& data, int bpm, const juce::File& dest);
};

} // namespace reharm
