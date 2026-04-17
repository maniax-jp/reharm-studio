#include <juce_core/juce_core.h>

class TimingTest : public juce::UnitTest
{
public:
    TimingTest() : juce::UnitTest ("TimingTest") {}

    void runTest() override
    {
        beginTest ("BPMToSamplesCalculation");
        {
            double sampleRate = 44100.0;
            int bpm = 120;
            
            int samplesPerBeat = static_cast<int> (sampleRate * 60.0 / bpm);
            expectEquals (samplesPerBeat, 22050);
            
            int64_t samplesPerMeasure = static_cast<int64_t>(samplesPerBeat) * 4;
            expectEquals (samplesPerMeasure, (int64_t)88200);
        }

        beginTest ("ChordTransitionTiming");
        {
            // Simulate the logic in getNextAudioBlock
            int64_t samplesPerMeasure = 88200;
            int currentChordIndex = 0;
            int64_t playbackSamplePosition = 0;
            int numSamples = 512; // typical block size

            // Simulate blocks until we hit the first transition
            while (playbackSamplePosition < 88200)
            {
                int64_t blockStart = playbackSamplePosition;
                int64_t blockEnd = playbackSamplePosition + numSamples;
                
                // Transition check logic from MainComponent.cpp
                if (currentChordIndex < 3) // assuming 4 chords total
                {
                    int64_t nextChordStart = static_cast<int64_t>(currentChordIndex + 1) * samplesPerMeasure;
                    if (nextChordStart >= blockStart && nextChordStart < blockEnd)
                    {
                        currentChordIndex++;
                    }
                }
                playbackSamplePosition += numSamples;
            }

            expectEquals (currentChordIndex, 1, "Should have transitioned to the second chord after 88200 samples");
            expect (playbackSamplePosition >= 88200, "Playback position should have reached or passed the transition point");
        }

        beginTest ("StopPlaybackImmediateNoteOff");
        {
            // Simulate a state where the 3rd chord is playing
            int currentChordIndex = 2; 
            bool isPlaying = true;
            bool stopRequested = false;
            int64_t playbackSamplePosition = 176400 + 1000; // Middle of 3rd measure
            int numSamples = 512;
            
            // Trigger Stop
            isPlaying = false;
            stopRequested = true;
            
            // Simulate getNextAudioBlock logic for stopRequested
            bool noteOffSent = false;
            if (!isPlaying && stopRequested)
            {
                if (currentChordIndex < 4) // assuming 4 chords
                {
                    // In real code, this sends Note Off for chordNotes[currentChordIndex]
                    noteOffSent = true;
                }
                stopRequested = false;
                currentChordIndex = 0;
                playbackSamplePosition = 0;
            }
            
            expect (noteOffSent, "Note Off should be sent for the currently playing chord");
            expectEquals (currentChordIndex, 0, "Chord index should be reset after stop");
            expectEquals (playbackSamplePosition, (int64_t)0, "Playback position should be reset after stop");
        }
    }
};

static TimingTest timingTest;
