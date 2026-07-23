#pragma once

#include "ChordModel.h"
#include <optional>

/** Shared UI display / selection state owned by MainComponent. */
struct DisplayState
{
    reharm::Voicing::Style voicingStyle = reharm::Voicing::Style::Close;
    int selectedBar = -1;
    int selectedSlot = -1;
    std::optional<reharm::Chord> chordClipboard;   // Cmd+C internal clipboard

    bool hasSelection() const noexcept { return selectedBar >= 0 && selectedSlot >= 0; }

    void clearSelection() noexcept
    {
        selectedBar = -1;
        selectedSlot = -1;
    }
};
