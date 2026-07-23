#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/MidiExporter.h"

using namespace reharm;

class MidiExporterTest : public juce::UnitTest
{
public:
    MidiExporterTest() : juce::UnitTest ("MidiExporter") {}

    void runTest() override
    {
        beginTest ("Header: ticksPerQuarterNote 960, single track");
        {
            ChordData data;
            data.notes = { { 60 } };
            data.beats = { 4.0 };
            data.totalChords = 1;
            data.totalBeats = 4.0;
            data.cumBeats = { 0.0, 4.0 };

            auto mf = MidiExporter::buildMidiFile (data, 120);

            juce::MemoryOutputStream mos;
            expect (mf.writeTo (mos));

            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            juce::MidiFile readBack;
            expect (readBack.readFrom (mis, false));

            expectEquals ((int) readBack.getTimeFormat(), 960);
            expectEquals (readBack.getNumTracks(), 1);
        }

        beginTest ("Tempo and time signature: bpm=120 -> 0.5 sec/quarter, 4/4");
        {
            ChordData data;
            data.notes = { { 60 } };
            data.beats = { 4.0 };
            data.totalChords = 1;
            data.totalBeats = 4.0;
            data.cumBeats = { 0.0, 4.0 };

            auto mf = MidiExporter::buildMidiFile (data, 120);

            juce::MemoryOutputStream mos;
            expect (mf.writeTo (mos));

            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            juce::MidiFile readBack;
            expect (readBack.readFrom (mis, false));

            const auto* seq = readBack.getTrack (0);
            expect (seq != nullptr);

            bool foundTempo = false;
            bool foundTimeSig = false;
            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                const auto& msg = seq->getEventPointer (i)->message;
                if (msg.isTempoMetaEvent())
                {
                    foundTempo = true;
                    expectWithinAbsoluteError (msg.getTempoSecondsPerQuarterNote(), 0.5, 1.0e-6);
                }
                if (msg.isTimeSignatureMetaEvent())
                {
                    int numerator = 0, denominator = 0;
                    msg.getTimeSignatureInfo (numerator, denominator);
                    foundTimeSig = true;
                    expectEquals (numerator, 4);
                    expectEquals (denominator, 4);
                }
            }
            expect (foundTempo);
            expect (foundTimeSig);
        }

        beginTest ("Note placement: chord, rest, chord slots produce correct on/off ticks");
        {
            std::vector<std::vector<int>> notes = { { 60, 64, 67 }, {}, { 55 } };
            std::vector<double> beats = { 4.0, 2.0, 2.0 };
            ChordData data (notes, beats);

            auto mf = MidiExporter::buildMidiFile (data, 120);

            juce::MemoryOutputStream mos;
            expect (mf.writeTo (mos));

            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            juce::MidiFile readBack;
            expect (readBack.readFrom (mis, false));

            const auto* seq = readBack.getTrack (0);
            expect (seq != nullptr);

            bool foundNoteOn60 = false, foundNoteOff60 = false;
            bool foundNoteOn55 = false, foundNoteOff55 = false;
            bool anyNoteInRestGap = false;

            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                const auto& msg = seq->getEventPointer (i)->message;

                if (msg.isNoteOn())
                {
                    expectEquals ((int) msg.getVelocity(), (int) MidiExporter::noteVelocity);
                    expectEquals (msg.getChannel(), MidiExporter::midiChannel);

                    if (msg.getNoteNumber() == 60)
                    {
                        foundNoteOn60 = true;
                        expectWithinAbsoluteError (msg.getTimeStamp(), 0.0, 1.0e-6);
                    }
                    else if (msg.getNoteNumber() == 55)
                    {
                        foundNoteOn55 = true;
                        expectWithinAbsoluteError (msg.getTimeStamp(), 5760.0, 1.0e-6);
                    }

                    // Strictly inside the rest gap (3840, 5760), excluding both endpoints.
                    if (msg.getTimeStamp() > 3840.0 + 1.0e-6 && msg.getTimeStamp() < 5760.0 - 1.0e-6)
                        anyNoteInRestGap = true;
                }
                else if (msg.isNoteOff())
                {
                    expectEquals (msg.getChannel(), MidiExporter::midiChannel);

                    if (msg.getNoteNumber() == 60)
                    {
                        foundNoteOff60 = true;
                        expectWithinAbsoluteError (msg.getTimeStamp(), 3840.0, 1.0e-6);
                    }
                    else if (msg.getNoteNumber() == 55)
                    {
                        foundNoteOff55 = true;
                        expectWithinAbsoluteError (msg.getTimeStamp(), 7680.0, 1.0e-6);
                    }
                }
            }

            expect (foundNoteOn60);
            expect (foundNoteOff60);
            expect (foundNoteOn55);
            expect (foundNoteOff55);
            expect (! anyNoteInRestGap);
        }

        beginTest ("writeMidiFile: writes a non-empty file that reads back with 1 track");
        {
            ChordData data;
            data.notes = { { 60 } };
            data.beats = { 4.0 };
            data.totalChords = 1;
            data.totalBeats = 4.0;
            data.cumBeats = { 0.0, 4.0 };

            auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("reharm-midiexporter-test.mid");
            dest.deleteFile();

            expect (MidiExporter::writeMidiFile (data, 120, dest));
            expect (dest.existsAsFile());
            expect (dest.getSize() > 0);

            juce::FileInputStream fis (dest);
            expect (fis.openedOk());
            juce::MidiFile readBack;
            expect (readBack.readFrom (fis, false));
            expectEquals (readBack.getNumTracks(), 1);

            dest.deleteFile();
        }

        beginTest ("Empty data: totalChords==0 produces tempo/timeSig/EOT only, no crash");
        {
            ChordData data;
            data.notes = {};
            data.beats = {};
            data.totalChords = 0;
            data.totalBeats = 0.0;
            data.cumBeats = { 0.0 };

            auto mf = MidiExporter::buildMidiFile (data, 120);

            juce::MemoryOutputStream mos;
            expect (mf.writeTo (mos));

            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            juce::MidiFile readBack;
            expect (readBack.readFrom (mis, false));

            expectEquals (readBack.getNumTracks(), 1);
            const auto* seq = readBack.getTrack (0);
            expect (seq != nullptr);

            bool foundTempo = false, foundTimeSig = false;
            int numNoteEvents = 0;
            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                const auto& msg = seq->getEventPointer (i)->message;
                if (msg.isTempoMetaEvent())
                    foundTempo = true;
                if (msg.isTimeSignatureMetaEvent())
                    foundTimeSig = true;
                if (msg.isNoteOnOrOff())
                    ++numNoteEvents;
            }
            expect (foundTempo);
            expect (foundTimeSig);
            expectEquals (numNoteEvents, 0);
        }
    }
};

static MidiExporterTest midiExporterTest;
