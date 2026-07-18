#pragma once

#include "ChordModel.h"

/** Shared UI display / selection state owned by MainComponent. */
struct DisplayState
{
    reharm::Voicing::Style voicingStyle = reharm::Voicing::Style::Close;
    int selectedBar = -1;
    int selectedSlot = -1;

    bool hasSelection() const noexcept { return selectedBar >= 0 && selectedSlot >= 0; }

    void clearSelection() noexcept
    {
        selectedBar = -1;
        selectedSlot = -1;
    }
};
