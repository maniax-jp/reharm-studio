#include <JuceHeader.h>
#include "../Source/ChordProgressionGenerator.h"

class ChordProgressionGeneratorTest : public juce::UnitTest
{
public:
    ChordProgressionGeneratorTest() : juce::UnitTest ("ChordProgressionGenerator") {}

    void runTest() override
    {
        ChordProgressionGenerator generator;
        
        beginTest ("GenerateProgressionLength");
        {
            juce::String progression = generator.generateProgression (0, "Major");
            juce::StringArray chords = juce::StringArray::fromTokens (progression, " - ", "");
            expect (chords.size() == 4, "Progression should have 4 chords");
        }

        beginTest ("MajorScaleProgression");
        {
            juce::String progression = generator.generateProgression (0, "Major");
            expect (!progression.isEmpty(), "Progression should not be empty");
        }
    }
};

static ChordProgressionGeneratorTest chordProgressionGeneratorTest;
