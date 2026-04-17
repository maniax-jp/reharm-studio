#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

/**
 * ChordTheory provides utility functions for converting musical chord symbols
 * into MIDI note numbers based on a given root note and scale.
 */
class ChordTheory
{
public:
    /**
     * Converts a chord symbol (e.g., "I", "ii", "V") into a set of MIDI notes.
     * @param chordSymbol The musical symbol of the chord.
     * @param root The root note (0 = C, 1 = C#, etc.).
     * @param isMajor Whether the underlying scale is Major or Minor.
     * @return A vector of MIDI note numbers.
     */
    static std::vector<int> getChordNotes(const juce::String& chordSymbol, int root, bool isMajor);

private:
    // Helper to get the MIDI note for a specific scale degree
    static int getNoteFromDegree(int degree, int root, bool isMajor);
};
