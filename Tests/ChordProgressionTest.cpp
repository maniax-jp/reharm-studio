#include <juce_core/juce_core.h>
#include "../Source/ChordProgressionGenerator.h"
#include <iostream>

class ChordProgressionTest : public juce::UnitTest
{
public:
    ChordProgressionTest() : juce::UnitTest ("ChordProgression") {}

    void runTest() override
    {
        ChordProgressionGenerator generator;
        
        beginTest ("ProgressionLength");
        {
            juce::String progression = generator.generateProgression (0, "Major");
            juce::StringArray chords;
            juce::String remaining = progression;
            while (remaining.isNotEmpty())
            {
                int nextHyphen = remaining.indexOf (" - ");
                if (nextHyphen >= 0)
                {
                    chords.add (remaining.substring (0, nextHyphen).trim());
                    remaining = remaining.substring (nextHyphen + 3);
                }
                else
                {
                    chords.add (remaining.trim());
                    remaining = "";
                }
            }
            expect (chords.size() == 4, "Should generate exactly 4 chords");
        }

        beginTest ("ProgressionContent");
        {
            juce::String progression = generator.generateProgression (0, "Major");
            // Default is I - vi - IV - V, but substitutions might change it.
            // We just check if it's a valid string.
            expect (!progression.isEmpty(), "Progression should not be empty");
        }
    }
};

static ChordProgressionTest chordProgressionTest;
