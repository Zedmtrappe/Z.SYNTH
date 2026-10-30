#pragma once

// Factory presets for the delay and reverb: named starting points grouped by
// type, chosen from the PRESETS menu in each effect's advanced popup. They
// set only that effect's parameters (the ON switch and MIX included), so the
// rest of the sound is untouched.

#include "../Parameters/ParameterLayout.h"

namespace zsynth
{

struct FxPreset
{
    const char* group;  // menu folder (the effect type)
    const char* name;
    const char* values; // "id = value; id = value" in preset text form
};

inline const std::vector<FxPreset>& delayPresets()
{
    static const std::vector<FxPreset> list = {
        { "Digital", "Clean Quarter",        "fx.dly.type = Digital; fx.dly.sync = on; fx.dly.div = 1/4; fx.dly.fb = 0.35; fx.dly.tone = 0.5; fx.dly.pp = off; fx.dly.mix = 0.25; fx.dly.spread = 0; fx.dly.duck = 0" },
        { "Digital", "Ping-Pong Eighths",    "fx.dly.type = Digital; fx.dly.sync = on; fx.dly.div = 1/8; fx.dly.fb = 0.45; fx.dly.tone = 0.45; fx.dly.pp = on; fx.dly.mix = 0.25; fx.dly.duck = 0.2" },
        { "Digital", "Dotted Eighth",        "fx.dly.type = Digital; fx.dly.sync = on; fx.dly.div = 1/8D; fx.dly.fb = 0.4; fx.dly.tone = 0.55; fx.dly.pp = off; fx.dly.spread = 0.3; fx.dly.mix = 0.3; fx.dly.duck = 0.3" },
        { "Tape RE-201", "Slapback",         "fx.dly.type = Tape RE-201; fx.dly.sync = off; fx.dly.time = 110; fx.dly.fb = 0.12; fx.dly.tone = 0.45; fx.dly.pp = off; fx.dly.wow = 0.4; fx.dly.flutter = 0.5; fx.dly.age = 0.4; fx.dly.mix = 0.3" },
        { "Tape RE-201", "Space Echo",       "fx.dly.type = Tape RE-201; fx.dly.sync = off; fx.dly.time = 320; fx.dly.fb = 0.55; fx.dly.tone = 0.4; fx.dly.pp = off; fx.dly.wow = 0.5; fx.dly.flutter = 0.5; fx.dly.age = 0.5; fx.dly.spread = 0.15; fx.dly.mix = 0.3" },
        { "Tape RE-201", "Dub Runaway",      "fx.dly.type = Tape RE-201; fx.dly.sync = on; fx.dly.div = 1/4D; fx.dly.fb = 0.88; fx.dly.tone = 0.35; fx.dly.pp = off; fx.dly.wow = 0.6; fx.dly.flutter = 0.6; fx.dly.age = 0.7; fx.dly.mix = 0.35" },
        { "Tape EP-3", "Echoplex Lead",      "fx.dly.type = Tape EP-3; fx.dly.sync = off; fx.dly.time = 380; fx.dly.fb = 0.4; fx.dly.tone = 0.45; fx.dly.pp = off; fx.dly.wow = 0.5; fx.dly.flutter = 0.4; fx.dly.age = 0.5; fx.dly.mix = 0.3" },
        { "Tape EP-3", "Worn Cassette",      "fx.dly.type = Tape EP-3; fx.dly.sync = off; fx.dly.time = 240; fx.dly.fb = 0.35; fx.dly.tone = 0.3; fx.dly.pp = off; fx.dly.wow = 0.9; fx.dly.flutter = 0.8; fx.dly.age = 0.9; fx.dly.mix = 0.3" },
        { "Analog DM-2", "Warm BBD",         "fx.dly.type = Analog DM-2; fx.dly.sync = off; fx.dly.time = 300; fx.dly.fb = 0.45; fx.dly.tone = 0.45; fx.dly.pp = off; fx.dly.wow = 0.5; fx.dly.age = 0.5; fx.dly.mix = 0.3" },
        { "Analog DM-2", "Chorus Echo",      "fx.dly.type = Analog DM-2; fx.dly.sync = off; fx.dly.time = 45; fx.dly.fb = 0.25; fx.dly.tone = 0.5; fx.dly.pp = off; fx.dly.wow = 1; fx.dly.age = 0.4; fx.dly.spread = 0.2; fx.dly.mix = 0.35" },
        { "Diffuse", "Cloud",                "fx.dly.type = Diffuse; fx.dly.sync = on; fx.dly.div = 1/4; fx.dly.fb = 0.6; fx.dly.tone = 0.4; fx.dly.pp = on; fx.dly.wow = 0.5; fx.dly.age = 0.4; fx.dly.mix = 0.35; fx.dly.duck = 0.2" },
        { "Diffuse", "Ambient Wash",         "fx.dly.type = Diffuse; fx.dly.sync = off; fx.dly.time = 600; fx.dly.fb = 0.8; fx.dly.tone = 0.3; fx.dly.pp = on; fx.dly.wow = 0.7; fx.dly.age = 0.5; fx.dly.spread = 0.4; fx.dly.mix = 0.4" },
        { "Lo-Fi", "Crushed",                "fx.dly.type = Lo-Fi; fx.dly.sync = on; fx.dly.div = 1/8; fx.dly.fb = 0.5; fx.dly.tone = 0.5; fx.dly.pp = on; fx.dly.age = 0.7; fx.dly.mix = 0.3" },
        { "Lo-Fi", "Telephone",              "fx.dly.type = Lo-Fi; fx.dly.sync = off; fx.dly.time = 260; fx.dly.fb = 0.4; fx.dly.tone = 0.75; fx.dly.pp = off; fx.dly.age = 0.5; fx.dly.mix = 0.3" },
    };
    return list;
}

inline const std::vector<FxPreset>& reverbPresets()
{
    static const std::vector<FxPreset> list = {
        { "Hall", "ZYNTH Hall",              "fx.rev.type = Hall; fx.rev.size = 0.6; fx.rev.damp = 0.4; fx.rev.pre = 20; fx.rev.decay = 0.5; fx.rev.lowcut = 20; fx.rev.diff = 0.5; fx.rev.mod = 0.5; fx.rev.early = 0.5; fx.rev.width = 0.5; fx.rev.mix = 0.25" },
        { "Hall", "Big Dark Hall",           "fx.rev.type = Hall; fx.rev.size = 0.85; fx.rev.damp = 0.65; fx.rev.pre = 35; fx.rev.decay = 0.6; fx.rev.lowcut = 80; fx.rev.diff = 0.6; fx.rev.mod = 0.5; fx.rev.early = 0.5; fx.rev.width = 0.6; fx.rev.mix = 0.3" },
        { "Room", "Small Studio",            "fx.rev.type = Room; fx.rev.size = 0.35; fx.rev.damp = 0.45; fx.rev.pre = 4; fx.rev.decay = 0.4; fx.rev.lowcut = 60; fx.rev.diff = 0.5; fx.rev.mod = 0.4; fx.rev.early = 0.55; fx.rev.width = 0.5; fx.rev.mix = 0.2" },
        { "Room", "Drum Room",               "fx.rev.type = Room; fx.rev.size = 0.5; fx.rev.damp = 0.3; fx.rev.pre = 2; fx.rev.decay = 0.5; fx.rev.lowcut = 40; fx.rev.diff = 0.6; fx.rev.mod = 0.3; fx.rev.early = 0.4; fx.rev.width = 0.7; fx.rev.mix = 0.25" },
        { "EMT 140 Plate", "Vocal Plate",    "fx.rev.type = EMT 140 Plate; fx.rev.size = 0.5; fx.rev.damp = 0.25; fx.rev.pre = 30; fx.rev.decay = 0.5; fx.rev.lowcut = 120; fx.rev.diff = 0.7; fx.rev.mod = 0.5; fx.rev.early = 0.5; fx.rev.width = 0.6; fx.rev.mix = 0.25" },
        { "EMT 140 Plate", "Bright Snare Plate", "fx.rev.type = EMT 140 Plate; fx.rev.size = 0.4; fx.rev.damp = 0.15; fx.rev.pre = 10; fx.rev.decay = 0.4; fx.rev.lowcut = 200; fx.rev.diff = 0.8; fx.rev.mod = 0.4; fx.rev.early = 0.5; fx.rev.width = 0.7; fx.rev.mix = 0.25" },
        { "Echo Chamber", "Studio Chamber",  "fx.rev.type = Echo Chamber; fx.rev.size = 0.55; fx.rev.damp = 0.45; fx.rev.pre = 15; fx.rev.decay = 0.5; fx.rev.lowcut = 90; fx.rev.diff = 0.55; fx.rev.mod = 0.4; fx.rev.early = 0.5; fx.rev.width = 0.5; fx.rev.mix = 0.25" },
        { "Echo Chamber", "Stone Chamber",   "fx.rev.type = Echo Chamber; fx.rev.size = 0.7; fx.rev.damp = 0.3; fx.rev.pre = 20; fx.rev.decay = 0.6; fx.rev.lowcut = 60; fx.rev.diff = 0.45; fx.rev.mod = 0.3; fx.rev.early = 0.45; fx.rev.width = 0.6; fx.rev.mix = 0.3" },
        { "Spring", "Guitar Amp Spring",     "fx.rev.type = Spring; fx.rev.size = 0.45; fx.rev.damp = 0.3; fx.rev.pre = 0; fx.rev.decay = 0.5; fx.rev.lowcut = 150; fx.rev.diff = 0.4; fx.rev.mod = 0.5; fx.rev.early = 0.5; fx.rev.width = 0.3; fx.rev.mix = 0.3" },
        { "Spring", "Dub Spring",            "fx.rev.type = Spring; fx.rev.size = 0.65; fx.rev.damp = 0.2; fx.rev.pre = 0; fx.rev.decay = 0.7; fx.rev.lowcut = 200; fx.rev.diff = 0.3; fx.rev.mod = 0.6; fx.rev.early = 0.5; fx.rev.width = 0.5; fx.rev.mix = 0.4" },
        { "Lexicon 224", "Concert Hall",     "fx.rev.type = Lexicon 224; fx.rev.size = 0.7; fx.rev.damp = 0.4; fx.rev.pre = 25; fx.rev.decay = 0.55; fx.rev.lowcut = 60; fx.rev.diff = 0.5; fx.rev.mod = 0.5; fx.rev.early = 0.5; fx.rev.width = 0.6; fx.rev.mix = 0.3" },
        { "Lexicon 224", "80s Lush Pad Hall", "fx.rev.type = Lexicon 224; fx.rev.size = 0.85; fx.rev.damp = 0.5; fx.rev.pre = 40; fx.rev.decay = 0.7; fx.rev.lowcut = 100; fx.rev.diff = 0.5; fx.rev.mod = 0.75; fx.rev.early = 0.6; fx.rev.width = 0.8; fx.rev.mix = 0.35" },
        { "Lexicon 480L", "Large Hall",      "fx.rev.type = Lexicon 480L; fx.rev.size = 0.8; fx.rev.damp = 0.35; fx.rev.pre = 30; fx.rev.decay = 0.55; fx.rev.lowcut = 50; fx.rev.diff = 0.6; fx.rev.mod = 0.5; fx.rev.early = 0.5; fx.rev.width = 0.6; fx.rev.mix = 0.3" },
        { "Lexicon 480L", "Rich Plate Hall", "fx.rev.type = Lexicon 480L; fx.rev.size = 0.6; fx.rev.damp = 0.25; fx.rev.pre = 15; fx.rev.decay = 0.5; fx.rev.lowcut = 120; fx.rev.diff = 0.75; fx.rev.mod = 0.6; fx.rev.early = 0.6; fx.rev.width = 0.7; fx.rev.mix = 0.28" },
        { "Bricasti M7", "Concert Hall",     "fx.rev.type = Bricasti M7; fx.rev.size = 0.75; fx.rev.damp = 0.35; fx.rev.pre = 25; fx.rev.decay = 0.5; fx.rev.lowcut = 40; fx.rev.diff = 0.6; fx.rev.mod = 0.4; fx.rev.early = 0.45; fx.rev.width = 0.55; fx.rev.mix = 0.28" },
        { "Bricasti M7", "Natural Room",     "fx.rev.type = Bricasti M7; fx.rev.size = 0.4; fx.rev.damp = 0.4; fx.rev.pre = 8; fx.rev.decay = 0.45; fx.rev.lowcut = 60; fx.rev.diff = 0.65; fx.rev.mod = 0.3; fx.rev.early = 0.4; fx.rev.width = 0.5; fx.rev.mix = 0.22" },
        { "Bricasti M7", "Cathedral",        "fx.rev.type = Bricasti M7; fx.rev.size = 1; fx.rev.damp = 0.45; fx.rev.pre = 45; fx.rev.decay = 0.75; fx.rev.lowcut = 40; fx.rev.diff = 0.7; fx.rev.mod = 0.4; fx.rev.early = 0.5; fx.rev.width = 0.7; fx.rev.mix = 0.35" },
        { "Shimmer", "Heaven",               "fx.rev.type = Shimmer; fx.rev.size = 0.85; fx.rev.damp = 0.35; fx.rev.pre = 40; fx.rev.decay = 0.7; fx.rev.lowcut = 150; fx.rev.diff = 0.6; fx.rev.mod = 0.6; fx.rev.early = 0.5; fx.rev.width = 0.8; fx.rev.mix = 0.35" },
        { "Shimmer", "Subtle Shimmer",       "fx.rev.type = Shimmer; fx.rev.size = 0.6; fx.rev.damp = 0.5; fx.rev.pre = 25; fx.rev.decay = 0.5; fx.rev.lowcut = 200; fx.rev.diff = 0.5; fx.rev.mod = 0.5; fx.rev.early = 0.5; fx.rev.width = 0.6; fx.rev.mix = 0.22" },
    };
    return list;
}

/** Parses "id = value; ..." into (parameter index, value) pairs. Unknown ids
    or values are skipped (tests check that every factory entry parses). */
inline std::vector<std::pair<int, float>> parseFxPreset(const FxPreset& preset, juce::StringArray* errors = nullptr)
{
    std::vector<std::pair<int, float>> out;
    for (const auto& item : juce::StringArray::fromTokens(preset.values, ";", ""))
    {
        const auto id = item.upToFirstOccurrenceOf("=", false, false).trim();
        const auto text = item.fromFirstOccurrenceOf("=", false, false).trim();
        const int index = findParamIndex(id.toRawUTF8());
        float value = 0.0f;
        if (index >= 0 && presetTextToValue(index, text, value))
            out.emplace_back(index, value);
        else if (errors != nullptr)
            errors->add(juce::String(preset.name) + ": " + item.trim());
    }
    return out;
}

} // namespace zsynth
