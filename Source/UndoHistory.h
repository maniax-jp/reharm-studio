#pragma once

#include "ProgressionModel.h"
#include <optional>
#include <vector>

namespace reharm
{

/** Snapshot-based undo/redo history for ProgressionState. Message thread only. */
class UndoHistory
{
public:
    static constexpr int maxDepth = 100;

    void reset (const ProgressionState& initial)
    {
        undoStack.clear();
        redoStack.clear();
        current = initial;
        hasCurrent = true;
    }

    void commit (const ProgressionState& newState)
    {
        if (! hasCurrent)
            return;

        if (newState == current)
            return;

        undoStack.push_back (current);

        if (static_cast<int> (undoStack.size()) > maxDepth)
            undoStack.erase (undoStack.begin());

        current = newState;
        redoStack.clear();
    }

    bool canUndo() const noexcept
    {
        return hasCurrent && ! undoStack.empty();
    }

    bool canRedo() const noexcept
    {
        return hasCurrent && ! redoStack.empty();
    }

    std::optional<ProgressionState> undo()
    {
        if (! canUndo())
            return std::nullopt;

        redoStack.push_back (current);
        current = undoStack.back();
        undoStack.pop_back();
        return current;
    }

    std::optional<ProgressionState> redo()
    {
        if (! canRedo())
            return std::nullopt;

        undoStack.push_back (current);
        current = redoStack.back();
        redoStack.pop_back();
        return current;
    }

private:
    std::vector<ProgressionState> undoStack, redoStack;
    ProgressionState current;
    bool hasCurrent = false;
};

} // namespace reharm
