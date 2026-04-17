#pragma once

#include <juce_core/juce_core.h>

class ChordProgressionGenerator
{
public:
    ChordProgressionGenerator();
    ~ChordProgressionGenerator();

    juce::String generateProgression (int rootNote, const juce::String& scale);

private:
    juce::Array<juce::String> majorChords;
    juce::Array<juce::String> minorChords;

    juce::String applySubstitution (const juce::String& progression);
};