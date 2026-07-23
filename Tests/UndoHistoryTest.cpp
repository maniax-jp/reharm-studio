#include <JuceHeader.h>
#include "../Source/ProgressionModel.h"
#include "../Source/UndoHistory.h"

using namespace reharm;

class UndoHistoryTest : public juce::UnitTest
{
public:
    UndoHistoryTest()
        : juce::UnitTest ("UndoHistoryTest")
    {
    }

    void runTest() override
    {
        beginTest ("captureState/restoreState round-trip");
        {
            ProgressionModel model;
            model.setNumBars (5);
            model.setSubdivision (0, BarSubdivision::Two);
            model.setSubdivision (1, BarSubdivision::Four);
            model.setSubdivision (2, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });
            model.setChord (0, 1, Chord { 2, ChordQuality::Minor7, -1 });
            model.setChord (1, 0, Chord { 5, ChordQuality::Major7, -1 });
            model.setChord (1, 2, Chord { 7, ChordQuality::Dominant7, -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor, -1 });
            model.setKey (KeyContext { 4, false });

            const auto original = model.captureState();

            model.setNumBars (8);
            model.clear();
            model.setKey (KeyContext { 0, true });
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 11, ChordQuality::Diminished, -1 });

            model.restoreState (original);
            expect (model.captureState() == original);
            expectEquals (model.getNumBars(), 5);
            expect (model.getKey() == KeyContext { 4, false });
            expect (model.getChord (0, 0) == Chord { 0, ChordQuality::Major, -1 });
            expect (model.getChord (1, 2) == Chord { 7, ChordQuality::Dominant7, -1 });
        }

        beginTest ("restoreState notifies onChanged once");
        {
            ProgressionModel model;
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });
            const auto s = model.captureState();

            int notifyCount = 0;
            model.onChanged = [&] { ++notifyCount; };

            model.restoreState (s);
            expectEquals (notifyCount, 1);
        }

        beginTest ("commit/undo/redo basic sequence");
        {
            ProgressionModel model;
            auto s0 = model.captureState();

            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });
            auto s1 = model.captureState();

            model.setChord (1, 0, Chord { 5, ChordQuality::Major, -1 });
            auto s2 = model.captureState();

            UndoHistory history;
            history.reset (s0);
            history.commit (s1);
            history.commit (s2);

            auto u1 = history.undo();
            expect (u1.has_value());
            expect (*u1 == s1);

            auto u0 = history.undo();
            expect (u0.has_value());
            expect (*u0 == s0);
            expect (! history.canUndo());

            auto r1 = history.redo();
            expect (r1.has_value());
            expect (*r1 == s1);

            auto r2 = history.redo();
            expect (r2.has_value());
            expect (*r2 == s2);
            expect (! history.canRedo());
        }

        beginTest ("commit of identical state is ignored");
        {
            ProgressionModel model;
            auto s0 = model.captureState();

            UndoHistory history;
            history.reset (s0);
            history.commit (s0);
            expect (! history.canUndo());
        }

        beginTest ("commit clears redo stack");
        {
            ProgressionModel model;
            auto s0 = model.captureState();

            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });
            auto s1 = model.captureState();

            model.setChord (0, 0, Chord { 2, ChordQuality::Minor, -1 });
            auto s2 = model.captureState();

            UndoHistory history;
            history.reset (s0);
            history.commit (s1);
            expect (history.undo().has_value());
            expect (history.canRedo());

            history.commit (s2);
            expect (! history.canRedo());
        }

        beginTest ("maxDepth caps undo stack at 100");
        {
            ProgressionModel model;
            UndoHistory history;
            history.reset (model.captureState());

            for (int i = 0; i < 110; ++i)
            {
                model.setChord (0, 0, Chord { i % 12, ChordQuality::Major, -1 });
                // Also flip numBars to ensure distinct states when root wraps.
                model.setNumBars (1 + (i % 8));
                history.commit (model.captureState());
            }

            int undoCount = 0;
            while (history.canUndo())
            {
                expect (history.undo().has_value());
                ++undoCount;
            }
            expectEquals (undoCount, UndoHistory::maxDepth);
            expect (! history.undo().has_value());
        }

        beginTest ("MainComponent-style integration: commit on change, skip while restoring");
        {
            ProgressionModel model;
            UndoHistory history;
            bool restoring = false;

            model.onChanged = [&]
            {
                if (! restoring)
                    history.commit (model.captureState());
            };

            history.reset (model.captureState());

            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });
            model.setChord (1, 0, Chord { 5, ChordQuality::Major, -1 });
            model.setChord (2, 0, Chord { 7, ChordQuality::Dominant7, -1 });

            expect (history.canUndo());

            auto apply = [&] (const ProgressionState& s)
            {
                restoring = true;
                model.restoreState (s);
                restoring = false;
            };

            auto step1 = history.undo();
            expect (step1.has_value());
            apply (*step1);
            expect (! model.getChord (2, 0).has_value());
            expect (model.getChord (1, 0) == Chord { 5, ChordQuality::Major, -1 });

            auto step0 = history.undo();
            expect (step0.has_value());
            apply (*step0);
            expect (! model.getChord (1, 0).has_value());
            expect (model.getChord (0, 0) == Chord { 0, ChordQuality::Major, -1 });

            auto stepEmpty = history.undo();
            expect (stepEmpty.has_value());
            apply (*stepEmpty);
            expect (! model.getChord (0, 0).has_value());

            auto redo1 = history.redo();
            expect (redo1.has_value());
            apply (*redo1);
            expect (model.getChord (0, 0) == Chord { 0, ChordQuality::Major, -1 });

            auto redo2 = history.redo();
            expect (redo2.has_value());
            apply (*redo2);
            expect (model.getChord (1, 0) == Chord { 5, ChordQuality::Major, -1 });

            auto redo3 = history.redo();
            expect (redo3.has_value());
            apply (*redo3);
            expect (model.getChord (2, 0) == Chord { 7, ChordQuality::Dominant7, -1 });
            expect (! history.canRedo());
        }
    }
};

static UndoHistoryTest undoHistoryTest;
