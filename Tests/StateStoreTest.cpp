#include <JuceHeader.h>
#include "../Source/StateStore.h"
#include "../Source/ProgressionModel.h"

using namespace reharm;

class StateStoreTest : public juce::UnitTest
{
public:
    StateStoreTest() : juce::UnitTest ("StateStore") {}

    void runTest() override
    {
        beginTest ("qualityId round-trip for all qualities; Major is \"maj\"");
        {
            for (int i = 0; i < (int) ChordQuality::NumQualities; ++i)
            {
                const auto q = static_cast<ChordQuality> (i);
                const auto id = ChordModel::qualityId (q);
                const auto back = ChordModel::qualityFromId (id);
                expect (back.has_value());
                expect (*back == q);
            }

            expect (ChordModel::qualityId (ChordQuality::Major) == "maj");
        }

        beginTest ("serialize/deserialize round-trip: full session");
        {
            ProgressionModel model;
            model.setKey ({ 9, false }); // A minor

            {
                ProgressionModel::ScopedBulkEdit bulk (model);
                model.setNumBars (3);

                model.setSubdivision (0, BarSubdivision::One);
                model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C major

                model.setSubdivision (1, BarSubdivision::Two);
                Chord g7 { 7, ChordQuality::Dominant7, 11 }; // G7/B
                g7.addMask = AddNine;
                ChordModel::sanitizeOptions (g7);
                model.setChord (1, 0, g7);
                model.setChord (1, 1, std::nullopt);

                model.setSubdivision (2, BarSubdivision::Four);
                model.setChord (2, 0, Chord { 5, ChordQuality::Minor7, -1 });
                model.setChord (2, 2, Chord { 2, ChordQuality::Major7, -1 });
            }

            StateStore::SessionData s;
            s.bpm = 97;
            s.volume = 0.33f;
            s.voicingClose = false;
            s.pluginPath = "/Library/Audio/Plug-Ins/VST3/Test.vst3";
            s.pluginName = "Test Plugin";
            s.pluginStateB64 = "QUJDRA=="; // "ABCD"

            const auto v = StateStore::serialize (model, s);

            ProgressionModel model2;
            StateStore::SessionData s2;
            const bool ok = StateStore::deserialize (v, model2, s2);
            expect (ok);

            expectEquals (model2.getNumBars(), model.getNumBars());
            expect (model2.getKey().tonicPitchClass == model.getKey().tonicPitchClass);
            expect (model2.getKey().isMajor == model.getKey().isMajor);

            for (int b = 0; b < model.getNumBars(); ++b)
            {
                expect (model2.getSubdivision (b) == model.getSubdivision (b));
                const auto numSlots = model.getBar (b).numSlots();
                for (int slot = 0; slot < numSlots; ++slot)
                {
                    const auto a = model.getChord (b, slot);
                    const auto bb = model2.getChord (b, slot);
                    expect (a.has_value() == bb.has_value());
                    if (a.has_value() && bb.has_value())
                        expect (*a == *bb);
                }
            }

            expectEquals (s2.bpm, s.bpm);
            expectWithinAbsoluteError (s2.volume, s.volume, 0.0001f);
            expect (s2.voicingClose == s.voicingClose);
            expect (s2.pluginPath == s.pluginPath);
            expect (s2.pluginName == s.pluginName);
            expect (s2.pluginStateB64 == s.pluginStateB64);
        }

        beginTest ("musicalOnly: no plugin/volume fields; deserialize keeps default volume");
        {
            ProgressionModel model;
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });

            StateStore::SessionData s;
            s.bpm = 140;
            s.volume = 0.55f;
            s.pluginPath = "/some/plugin.vst3";
            s.pluginName = "Some Plugin";
            s.pluginStateB64 = "AAAA";

            const auto v = StateStore::serialize (model, s, true);

            auto* obj = v.getDynamicObject();
            expect (obj != nullptr);
            expect (! obj->hasProperty ("plugin"));
            expect (! obj->hasProperty ("volume"));

            ProgressionModel model2;
            StateStore::SessionData s2;
            s2.volume = 0.8f; // default sentinel to check against
            const bool ok = StateStore::deserialize (v, model2, s2);
            expect (ok);
            expectWithinAbsoluteError (s2.volume, 0.8f, 0.0001f);
            expect (s2.pluginPath.isEmpty());
        }

        beginTest ("Invalid input: null var / missing bars leaves model untouched");
        {
            ProgressionModel model;
            model.setChord (0, 0, Chord { 3, ChordQuality::Minor7, -1 });

            StateStore::SessionData s;
            bool ok = StateStore::deserialize (juce::var(), model, s);
            expect (! ok);
            auto c = model.getChord (0, 0);
            expect (c.has_value());
            expect (*c == Chord ({ 3, ChordQuality::Minor7, -1 }));

            auto* obj = new juce::DynamicObject();
            obj->setProperty ("schema", 1);
            obj->setProperty ("bpm", 120);
            juce::var noBars (obj);

            ok = StateStore::deserialize (noBars, model, s);
            expect (! ok);
            c = model.getChord (0, 0);
            expect (c.has_value());
            expect (*c == Chord ({ 3, ChordQuality::Minor7, -1 }));
        }

        beginTest ("Defensive parsing: bad root -> empty slot; bad div -> 1; bpm clamp");
        {
            auto* barObj = new juce::DynamicObject();
            barObj->setProperty ("div", 3); // invalid -> should become 1

            juce::Array<juce::var> slots;
            auto* badChord = new juce::DynamicObject();
            badChord->setProperty ("root", 99); // invalid -> empty slot
            badChord->setProperty ("q", "maj");
            slots.add (juce::var (badChord));
            barObj->setProperty ("slots", juce::var (slots));

            juce::Array<juce::var> bars;
            bars.add (juce::var (barObj));

            auto* root = new juce::DynamicObject();
            root->setProperty ("schema", 1);
            root->setProperty ("bpm", 9999); // should clamp to 240
            root->setProperty ("bars", juce::var (bars));

            ProgressionModel model;
            StateStore::SessionData s;
            const bool ok = StateStore::deserialize (juce::var (root), model, s);
            expect (ok);

            expect (model.getSubdivision (0) == BarSubdivision::One);
            expect (! model.getChord (0, 0).has_value());
            expectEquals (s.bpm, 240);
        }

        beginTest ("File round-trip: session + user preset");
        {
            auto tempBase = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("reharm-statestore-test");
            tempBase.deleteRecursively();

            StateStore store (tempBase);

            ProgressionModel model;
            model.setKey ({ 4, true });
            model.setChord (0, 0, Chord { 4, ChordQuality::Major7, -1 });

            StateStore::SessionData s;
            s.bpm = 110;
            s.volume = 0.6f;
            s.voicingClose = true;

            expect (store.saveSession (model, s));
            expect (store.sessionFile().existsAsFile());

            ProgressionModel loadedModel;
            StateStore::SessionData loadedSession;
            expect (store.loadSession (loadedModel, loadedSession));
            expectEquals (loadedSession.bpm, 110);
            auto c = loadedModel.getChord (0, 0);
            expect (c.has_value());
            expect (*c == Chord ({ 4, ChordQuality::Major7, -1 }));

            expect (! store.userPresetExists ("Test Name"));
            expect (store.saveUserPreset ("Test Name", model, s));
            expect (store.userPresetExists ("Test Name"));

            const auto names = store.listUserPresetNames();
            expect (names.contains ("Test Name"));

            ProgressionModel loadedPreset;
            StateStore::SessionData loadedPresetSession;
            expect (store.loadUserPreset ("Test Name", loadedPreset, loadedPresetSession));
            auto pc = loadedPreset.getChord (0, 0);
            expect (pc.has_value());
            expect (*pc == Chord ({ 4, ChordQuality::Major7, -1 }));

            tempBase.deleteRecursively();
        }

        beginTest ("sanitizePresetName: strips path separators; blank -> empty");
        {
            const auto sanitized = StateStore::sanitizePresetName ("a/b:c");
            expect (! sanitized.contains ("/"));
            expect (! sanitized.contains (":"));

            expect (StateStore::sanitizePresetName ("   ").isEmpty());
        }
    }
};

static StateStoreTest stateStoreTest;
