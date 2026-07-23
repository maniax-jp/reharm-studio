#include "StateStore.h"

namespace reharm
{

namespace
{
constexpr int kSchemaVersion = 1;

juce::var chordToVar (const Chord& c)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("root", c.rootPitchClass);
    obj->setProperty ("q", ChordModel::qualityId (c.quality));

    if (c.hasBass())
        obj->setProperty ("bass", c.bassPitchClass);
    if (c.omitMask != 0)
        obj->setProperty ("omit", c.omitMask);
    if (c.addMask != 0)
        obj->setProperty ("add", c.addMask);
    if (c.fifthAlt == FifthAlt::Flat)
        obj->setProperty ("fifth", "b5");
    else if (c.fifthAlt == FifthAlt::Sharp)
        obj->setProperty ("fifth", "#5");

    return juce::var (obj);
}

/** Parses a chord object. Returns nullopt when root/quality is invalid (empty slot). */
std::optional<Chord> chordFromVar (const juce::var& v)
{
    if (! v.isObject())
        return std::nullopt;

    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    const int root = static_cast<int> (v.getProperty ("root", -1));
    if (root < 0 || root > 11)
        return std::nullopt;

    const auto qId = v.getProperty ("q", juce::var()).toString();
    const auto quality = ChordModel::qualityFromId (qId);
    if (! quality.has_value())
        return std::nullopt;

    Chord c;
    c.rootPitchClass = root;
    c.quality = *quality;

    const int bass = static_cast<int> (v.getProperty ("bass", -1));
    c.bassPitchClass = juce::jlimit (-1, 11, bass);

    const int omit = static_cast<int> (v.getProperty ("omit", 0));
    c.omitMask = omit & 7;

    const int add = static_cast<int> (v.getProperty ("add", 0));
    c.addMask = add & 127;

    const auto fifth = v.getProperty ("fifth", juce::var()).toString();
    if (fifth == "b5")
        c.fifthAlt = FifthAlt::Flat;
    else if (fifth == "#5")
        c.fifthAlt = FifthAlt::Sharp;
    else
        c.fifthAlt = FifthAlt::None;

    ChordModel::sanitizeOptions (c);
    return c;
}

int subdivisionToInt (BarSubdivision s) noexcept
{
    return static_cast<int> (s);
}

BarSubdivision subdivisionFromInt (int i) noexcept
{
    if (i == 2)
        return BarSubdivision::Two;
    if (i == 4)
        return BarSubdivision::Four;
    return BarSubdivision::One;
}
} // namespace

StateStore::StateStore (const juce::File& baseDirectory)
    : base (baseDirectory)
{
}

StateStore StateStore::createDefault()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("Application Support")
                    .getChildFile ("Reharm Studio");
    return StateStore (base);
}

juce::var StateStore::serialize (const ProgressionModel& model, const SessionData& s, bool musicalOnly)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", kSchemaVersion);
    root->setProperty ("bpm", s.bpm);

    if (! musicalOnly)
        root->setProperty ("volume", s.volume);

    root->setProperty ("voicing", s.voicingClose ? "close" : "open");

    auto* keyObj = new juce::DynamicObject();
    keyObj->setProperty ("tonic", model.getKey().tonicPitchClass);
    keyObj->setProperty ("major", model.getKey().isMajor);
    root->setProperty ("key", juce::var (keyObj));

    juce::Array<juce::var> bars;
    const int numBars = model.getNumBars();
    for (int b = 0; b < numBars; ++b)
    {
        const auto subdivision = model.getSubdivision (b);
        auto* barObj = new juce::DynamicObject();
        barObj->setProperty ("div", subdivisionToInt (subdivision));

        juce::Array<juce::var> slots;
        const int numSlots = static_cast<int> (subdivision);
        for (int slot = 0; slot < numSlots; ++slot)
        {
            const auto chord = model.getChord (b, slot);
            slots.add (chord.has_value() ? chordToVar (*chord) : juce::var());
        }
        barObj->setProperty ("slots", juce::var (slots));

        bars.add (juce::var (barObj));
    }
    root->setProperty ("bars", juce::var (bars));

    if (! musicalOnly)
    {
        auto* pluginObj = new juce::DynamicObject();
        pluginObj->setProperty ("path", s.pluginPath);
        pluginObj->setProperty ("name", s.pluginName);
        pluginObj->setProperty ("state", s.pluginStateB64);
        root->setProperty ("plugin", juce::var (pluginObj));
    }

    return juce::var (root);
}

bool StateStore::deserialize (const juce::var& v, ProgressionModel& model, SessionData& s)
{
    if (! v.isObject())
        return false;

    const auto barsVar = v.getProperty ("bars", juce::var());
    if (! barsVar.isArray())
        return false;

    auto* barsArray = barsVar.getArray();
    if (barsArray == nullptr || barsArray->isEmpty() || barsArray->size() > ProgressionModel::maxBars)
        return false;

    // Everything below only reads from v/copies into locals; the model is
    // touched only once we know the input is structurally valid.
    const int numBars = barsArray->size();

    struct ParsedBar
    {
        BarSubdivision subdivision = BarSubdivision::One;
        std::array<std::optional<Chord>, 4> slots;
    };
    std::vector<ParsedBar> parsedBars (static_cast<size_t> (numBars));

    for (int b = 0; b < numBars; ++b)
    {
        const auto& barVar = (*barsArray)[b];
        ParsedBar parsed;
        const int div = static_cast<int> (barVar.getProperty ("div", 1));
        parsed.subdivision = subdivisionFromInt (div);

        const auto slotsVar = barVar.getProperty ("slots", juce::var());
        if (auto* slotsArray = slotsVar.getArray())
        {
            const int numSlots = static_cast<int> (parsed.subdivision);
            for (int slot = 0; slot < numSlots && slot < slotsArray->size(); ++slot)
                parsed.slots [static_cast<size_t> (slot)] = chordFromVar ((*slotsArray)[slot]);
        }

        parsedBars [static_cast<size_t> (b)] = std::move (parsed);
    }

    int bpm = static_cast<int> (v.getProperty ("bpm", 120));
    bpm = juce::jlimit (40, 240, bpm);

    double volume = static_cast<double> (v.getProperty ("volume", 0.8));
    volume = juce::jlimit (0.0, 1.0, volume);

    const auto voicingStr = v.getProperty ("voicing", juce::var()).toString();
    const bool voicingClose = (voicingStr != "open");

    const auto keyVar = v.getProperty ("key", juce::var());
    int tonic = static_cast<int> (keyVar.getProperty ("tonic", 0));
    if (tonic < 0 || tonic > 11)
        tonic = 0;

    const auto majorVar = keyVar.getProperty ("major", juce::var (true));
    const bool isMajor = majorVar.isBool() ? static_cast<bool> (majorVar) : true;

    // All parsed; now apply to the model in a single bulk edit.
    {
        ProgressionModel::ScopedBulkEdit bulk (model);
        model.setNumBars (numBars);
        model.setKey ({ tonic, isMajor });

        for (int b = 0; b < numBars; ++b)
        {
            const auto& parsed = parsedBars [static_cast<size_t> (b)];
            model.setSubdivision (b, parsed.subdivision);

            const int numSlots = static_cast<int> (parsed.subdivision);
            for (int slot = 0; slot < numSlots; ++slot)
                model.setChord (b, slot, parsed.slots [static_cast<size_t> (slot)]);
        }
    }

    s.bpm = bpm;
    s.volume = static_cast<float> (volume);
    s.voicingClose = voicingClose;

    const auto pluginVar = v.getProperty ("plugin", juce::var());
    if (pluginVar.isObject())
    {
        s.pluginPath = pluginVar.getProperty ("path", juce::var()).toString();
        s.pluginName = pluginVar.getProperty ("name", juce::var()).toString();
        s.pluginStateB64 = pluginVar.getProperty ("state", juce::var()).toString();
    }
    else
    {
        s.pluginPath = juce::String();
        s.pluginName = juce::String();
        s.pluginStateB64 = juce::String();
    }

    return true;
}

bool StateStore::saveSession (const ProgressionModel& model, const SessionData& s) const
{
    if (! base.createDirectory())
        return false;

    const auto json = juce::JSON::toString (serialize (model, s, false));
    return sessionFile().replaceWithText (json);
}

bool StateStore::loadSession (ProgressionModel& model, SessionData& s) const
{
    const auto file = sessionFile();
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file);
    return deserialize (parsed, model, s);
}

juce::StringArray StateStore::listUserPresetNames() const
{
    juce::StringArray names;
    const auto dir = presetsDir();
    if (! dir.isDirectory())
        return names;

    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*.json", juce::File::findFiles))
        names.add (entry.getFile().getFileNameWithoutExtension());

    names.sort (false);
    return names;
}

bool StateStore::userPresetExists (const juce::String& name) const
{
    return presetsDir().getChildFile (name + ".json").existsAsFile();
}

bool StateStore::saveUserPreset (const juce::String& name, const ProgressionModel& model, const SessionData& s) const
{
    const auto sanitized = sanitizePresetName (name);
    if (sanitized.isEmpty())
        return false;

    if (! presetsDir().createDirectory())
        return false;

    const auto json = juce::JSON::toString (serialize (model, s, true));
    return presetsDir().getChildFile (sanitized + ".json").replaceWithText (json);
}

bool StateStore::loadUserPreset (const juce::String& name, ProgressionModel& model, SessionData& s) const
{
    const auto file = presetsDir().getChildFile (name + ".json");
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file);
    return deserialize (parsed, model, s);
}

juce::String StateStore::sanitizePresetName (const juce::String& raw)
{
    return juce::File::createLegalFileName (raw).trim();
}

juce::File StateStore::baseDir() const
{
    return base;
}

juce::File StateStore::sessionFile() const
{
    return base.getChildFile ("session.json");
}

juce::File StateStore::presetsDir() const
{
    return base.getChildFile ("Presets");
}

} // namespace reharm
