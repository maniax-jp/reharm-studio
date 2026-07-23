#include "MidiExporter.h"

namespace reharm
{

juce::MidiFile MidiExporter::buildMidiFile (const ChordData& data, int bpm)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ticksPerQuarterNote);

    // Defensive: recompute cumulative beats from `beats` if the invariant
    // (cumBeats.size() == totalChords + 1) does not hold.
    std::vector<double> cumBeats = data.cumBeats;
    if (cumBeats.size() != (size_t) data.totalChords + 1)
    {
        cumBeats.clear();
        cumBeats.reserve ((size_t) data.totalChords + 1);
        cumBeats.push_back (0.0);
        for (int i = 0; i < data.totalChords; ++i)
        {
            const double b = (i < (int) data.beats.size()) ? data.beats[(size_t) i] : 0.0;
            cumBeats.push_back (cumBeats.back() + b);
        }
    }

    juce::MidiMessageSequence seq;
    const double microsecondsPerQuarter = 60000000.0 / (double) juce::jlimit (1, 999, bpm);
    {
        auto tempo = juce::MidiMessage::tempoMetaEvent ((int) microsecondsPerQuarter);
        tempo.setTimeStamp (0.0);
        seq.addEvent (tempo);
        auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
        ts.setTimeStamp (0.0);
        seq.addEvent (ts);
    }
    for (int i = 0; i < data.totalChords; ++i)
    {
        const double onTick  = cumBeats[(size_t) i]     * ticksPerQuarterNote;
        const double offTick = cumBeats[(size_t) i + 1] * ticksPerQuarterNote;
        for (int note : data.notes[(size_t) i])
        {
            auto on = juce::MidiMessage::noteOn (midiChannel, note, noteVelocity);
            on.setTimeStamp (onTick);
            seq.addEvent (on);
            auto off = juce::MidiMessage::noteOff (midiChannel, note);
            off.setTimeStamp (offTick);
            seq.addEvent (off);
        }
    }
    {
        auto eot = juce::MidiMessage::endOfTrack();
        eot.setTimeStamp (data.totalBeats * ticksPerQuarterNote);
        seq.addEvent (eot);
    }
    seq.updateMatchedPairs();
    seq.sort();
    mf.addTrack (seq);
    return mf;
}

bool MidiExporter::writeMidiFile (const ChordData& data, int bpm, const juce::File& dest)
{
    dest.deleteFile();
    juce::FileOutputStream fos (dest);
    if (fos.openedOk())
        return buildMidiFile (data, bpm).writeTo (fos);
    return false;
}

} // namespace reharm
