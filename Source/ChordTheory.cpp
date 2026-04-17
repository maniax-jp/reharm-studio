#include "ChordTheory.h"
#include <map>

std::vector<int> ChordTheory::getChordNotes(const juce::String& chordSymbol, int root, bool isMajor)
{
    // Define intervals for each chord symbol relative to the scale root
    static const std::map<juce::String, std::vector<int>> majorIntervals = {
        {"I",    {0, 4, 7}},
        {"ii",   {2, 5, 9}},
        {"iii",  {4, 7, 11}},
        {"IV",   {5, 9, 12}},
        {"V",    {7, 11, 14}},
        {"vi",   {9, 12, 16}},
        {"vii°", {11, 14, 17}},
        {"bVII", {10, 14, 17}}
    };

    static const std::map<juce::String, std::vector<int>> minorIntervals = {
        {"i",    {0, 3, 7}},
        {"ii°",  {2, 5, 8}},
        {"III",  {3, 7, 11}},
        {"iv",   {5, 8, 12}},
        {"v",    {7, 10, 14}},
        {"VI",   {8, 12, 16}},
        {"VII",  {10, 14, 17}}
    };

    int baseNote = 60 + root;
    const auto& intervalsMap = isMajor ? majorIntervals : minorIntervals;

    auto it = intervalsMap.find(chordSymbol);
    if (it != intervalsMap.end())
    {
        std::vector<int> notes;
        for (int interval : it->second)
        {
            notes.push_back(baseNote + interval);
        }
        return notes;
    }

    // Fallback: try the other map if not found (some symbols might be used interchangeably)
    const auto& fallbackMap = isMajor ? minorIntervals : majorIntervals;
    auto fit = fallbackMap.find(chordSymbol);
    if (fit != fallbackMap.end())
    {
        std::vector<int> notes;
        for (int interval : fit->second)
        {
            notes.push_back(baseNote + interval);
        }
        return notes;
    }

    return {};
}
