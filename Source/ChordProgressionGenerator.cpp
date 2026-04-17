#include "ChordProgressionGenerator.h"

ChordProgressionGenerator::ChordProgressionGenerator()
{
    majorChords = { "I", "ii", "iii", "IV", "V", "vi", "vii°" };
    minorChords = { "i", "ii°", "III", "iv", "v", "VI", "VII" };
}

ChordProgressionGenerator::~ChordProgressionGenerator()
{
}

juce::String ChordProgressionGenerator::generateProgression (int rootNote, const juce::String& scale)
{
    juce::Array<juce::String> chords = (scale == "Major") ? majorChords : minorChords;

    // Generate more complex progression: I - vi - IV - V
    juce::StringArray progression;
    progression.add (chords[0]); // I
    progression.add (chords[5]); // vi
    progression.add (chords[3]); // IV
    progression.add (chords[4]); // V

    juce::String result = progression.joinIntoString (" - ");

    return result;
}

juce::String ChordProgressionGenerator::applySubstitution (const juce::String& progression)
{
    juce::String result = progression;

    // Example substitutions
    // Tritone substitution: V -> vii°
    result = result.replace ("V", "vii°");

    // Backdoor progression: IV -> bVII
    result = result.replace ("IV", "bVII");

    // Add more substitutions as needed

    return result;
}