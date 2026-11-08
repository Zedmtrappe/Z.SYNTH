// JUCE-side tests: parameter model, presets, state, MIDI learn, fixed layout,
// editor construction and the factory library. Also hosts the preset audition
// tool:  ZSynthStateTests --preset-report [wavOutputDir]

#include "Presets/FxPresets.h"
#include "UI/Components/MusicalTyping.h"
#include "TestFramework.h"
#include "TestHelpers.h"

#include "MIDI/MidiLearn.h"
#include "Parameters/ParameterLayout.h"
#include "Plugin/ZSynthProcessor.h"
#include "DSP/Effects/DriveAndOutput.h"
#include "Presets/PresetFormat.h"
#include "Presets/PresetManager.h"
#include "UI/Layout/ActivityRules.h"
#include "UI/Layout/PanelLayout.h"
#include "UI/ZSynthEditor.h"

#include <BinaryData.h>
#include <map>
#include <juce_audio_formats/juce_audio_formats.h>

using namespace zstest;

namespace
{
    std::vector<PresetManager::Bank> banks()
    {
        return { { "ZYNTH Originals", juce::String::fromUTF8(BinaryData::FactoryBank_zsbank, BinaryData::FactoryBank_zsbankSize) },
                 { "Classic Machines", juce::String::fromUTF8(BinaryData::ClassicsBank_zsbank, BinaryData::ClassicsBank_zsbankSize) } };
    }

    /** Every factory preset from every bank. */
    std::vector<PresetData> allFactoryPresets(juce::StringArray& errors)
    {
        std::vector<PresetData> all;
        for (const auto& b : banks())
            for (auto& p : parsePresetBank(b.text, errors))
                all.push_back(std::move(p));
        return all;
    }

    juce::File tempDir(const juce::String& name)
    {
        auto d = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("zsynth_tests").getChildFile(name);
        d.deleteRecursively();
        d.createDirectory();
        return d;
    }

    struct ProcessorFixture
    {
        explicit ProcessorFixture(const juce::String& name) : dir(tempDir(name)), processor(dir) {}
        juce::File dir;
        ZSynthProcessor processor;
    };

    // ---------------------------------------------------------- audition
    struct Audition
    {
        double seconds;
        std::vector<std::pair<double, int>> ons;  // time, note
        std::vector<std::pair<double, int>> offs;
    };

    Audition auditionFor(const juce::String& category)
    {
        Audition a;
        if (category == "Bass")
        {
            a.seconds = 3.0;
            for (auto [t, n] : { std::pair<double, int> { 0.0, 36 }, { 0.6, 43 }, { 1.2, 41 } })
            {
                a.ons.push_back({ t, n });
                a.offs.push_back({ t + 0.5, n });
            }
        }
        else if (category == "Lead")
        {
            a.seconds = 3.0;
            a.ons = { { 0.0, 60 }, { 0.8, 67 }, { 1.4, 65 } };
            a.offs = { { 0.85, 60 }, { 1.45, 67 }, { 2.2, 65 } };
        }
        else if (category == "Sequence")
        {
            a.seconds = 3.5;
            a.ons = { { 0.0, 45 } };
            a.offs = { { 2.5, 45 } };
        }
        else if (category == "Percussion")
        {
            a.seconds = 2.0;
            for (double t : { 0.0, 0.5, 1.0 })
            {
                a.ons.push_back({ t, 48 });
                a.offs.push_back({ t + 0.1, 48 });
            }
        }
        else
        {
            // Poly categories: a held chord.
            a.seconds = 4.5;
            for (int n : { 60, 64, 67 })
            {
                a.ons.push_back({ 0.0, n });
                a.offs.push_back({ 2.2, n });
            }
        }
        return a;
    }

    struct PresetMetrics
    {
        float peak = 0.0f;
        double loudDb = -200.0;   // RMS of the loudest 400 ms window
        bool finite = true;
        std::vector<float> left, right;
    };

    PresetMetrics renderPreset(const PresetData& preset, double sampleRate = 48000.0)
    {
        EngineRig rig(sampleRate, 256);
        for (int i = 0; i < kNumParams; ++i)
            rig.set(i, preset.values[(size_t) i]);
        // Audition at the default global setup (presets do not recall globals).
        for (int i = 0; i < kNumParams; ++i)
            if (isGlobalSetupParam(i))
                rig.set(i, paramDef(i).def);

        const auto a = auditionFor(preset.category);
        for (auto [t, n] : a.ons)
            rig.noteOn(t, n, 100);
        for (auto [t, n] : a.offs)
            rig.noteOff(t, n);

        PresetMetrics m;
        m.left = rig.render(a.seconds, &m.right);
        m.finite = allFinite(m.left) && allFinite(m.right);
        m.peak = std::max(peakOf(m.left), peakOf(m.right));
        // Loudness = RMS of the loudest window: 400 ms, or 100 ms for percussion
        // (whose energy is concentrated in the attack).
        const size_t win = (size_t) ((preset.category == "Percussion" ? 0.1 : 0.4) * sampleRate);
        for (size_t start = 0; start + win <= m.left.size(); start += win / 4)
        {
            const double r = std::sqrt(0.5 * (rmsOf(m.left, start, start + win) * rmsOf(m.left, start, start + win)
                                              + rmsOf(m.right, start, start + win) * rmsOf(m.right, start, start + win)));
            m.loudDb = std::max(m.loudDb, 20.0 * std::log10(r + 1.0e-12));
        }
        return m;
    }

    constexpr double kLoudMin = -30.0, kLoudMax = -9.0;
}

// ======================================================== parameter model ====

ZS_TEST(param_table_matches_apvts)
{
    ProcessorFixture f("params");
    f.processor.getPresets().loadInit(); // the processor starts on factory preset 1
    auto& state = f.processor.getState();
    juce::StringArray ids;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = paramDef(i);
        ZS_CHECK(! ids.contains(d.id));
        ids.add(d.id);
        auto* p = state.getParameter(d.id);
        ZS_CHECK(p != nullptr);
        if (p == nullptr)
            continue;
        ZS_CHECK(d.min < d.max);
        ZS_CHECK(d.def >= d.min && d.def <= d.max);
        ZS_CHECK(juce::String(d.help).isNotEmpty());
        // Default round-trips through the normalised range.
        ZS_CHECK_NEAR(p->convertFrom0to1(p->convertTo0to1(d.def)), d.def, std::abs(d.max - d.min) * 1.0e-4 + 1.0e-4);
        // (The pattern sequencer starts on its demo pattern A, not on the plain defaults.)
        if (! juce::String(d.id).startsWith("pseq.") || juce::String(d.id) == "pseq.run")
            ZS_CHECK_NEAR(state.getRawParameterValue(d.id)->load(), d.def, 1.0e-4 * (1.0 + std::abs(d.def)));
    }
    ZS_CHECK((int) f.processor.getParameters().size() == kNumParams);
}

ZS_TEST(choice_lists_match_dsp_enums)
{
    ZS_CHECK(choiceCount(P::Engine) == (int) EngineMode::Count);
    for (int p : { P::Osc1Wave, P::Osc2Wave, P::Osc3Wave })
        ZS_CHECK(choiceCount(p) == (int) OscWave::Count);
    ZS_CHECK(choiceCount(P::Osc1Octave) == (int) OscOctave::Count);
    ZS_CHECK(choiceCount(P::SubWave) == (int) SubWave::Count);
    ZS_CHECK(choiceCount(P::NoiseType) == (int) NoiseType::Count);
    ZS_CHECK(choiceCount(P::WtTable) == (int) WavetableId::Count);
    ZS_CHECK(choiceCount(P::FltModel) == (int) FilterModel::Count);
    ZS_CHECK(choiceCount(P::WtSeqMode) == (int) WaveSeqMode::Count);
    ZS_CHECK(choiceCount(P::ChorusType) == (int) ChorusType::Count);
    ZS_CHECK(choiceCount(P::FltType) == (int) FilterType::Count);
    ZS_CHECK(choiceCount(P::FltSlope) == (int) FilterSlope::Count);
    ZS_CHECK(choiceCount(P::Lfo1Wave) == (int) LfoWave::Count);
    ZS_CHECK(choiceCount(P::Lfo1Mode) == (int) LfoMode::Count);
    ZS_CHECK(choiceCount(P::Lfo1Dest) == (int) LfoDest::Count);
    ZS_CHECK(choiceCount(P::Env3Dest) == (int) Env3Dest::Count);
    ZS_CHECK(choiceCount(P::Lfo1Div) == kNumDivisions);
    ZS_CHECK(choiceCount(P::VoiceMode) == (int) VoiceMode::Count);
    ZS_CHECK(choiceCount(P::VoicePriority) == (int) NotePriority::Count);
    ZS_CHECK(choiceCount(P::VoiceAlloc) == (int) VoiceAlloc::Count);
    ZS_CHECK(choiceCount(P::GlideMode) == (int) GlideMode::Count);
    ZS_CHECK(choiceCount(P::VelCurve) == (int) VelocityCurve::Count);
    ZS_CHECK(choiceCount(P::FmAlgo) == kNumFmAlgorithms);
    ZS_CHECK(choiceCount(P::Mod1Src) == (int) ModSource::Count);
    ZS_CHECK(choiceCount(P::Mod1Dst) == (int) ModDest::Count);
    ZS_CHECK(choiceCount(P::ArpMode) == (int) ArpMode::Count);
    ZS_CHECK(choiceCount(P::SeqMode) == (int) SeqMode::Count);
    ZS_CHECK(choiceCount(P::DriveType) == (int) DriveType::Count);
    ZS_CHECK(choiceCount(P::Oversampling) == (int) OversamplingMode::Count);
}

ZS_TEST(value_text_round_trip)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = paramDef(i);
        for (float t : { 0.0f, 0.37f, 1.0f })
        {
            float v = rangeFor(i).convertFrom0to1(t);
            if (d.type != ParamType::Float)
                v = std::round(v);
            const float back = parseValue(i, formatValue(i, v));
            const float tol = d.type == ParamType::Float ? std::max(0.02f * std::abs(v), 0.011f * (d.max - d.min)) : 0.01f;
            if (std::abs(back - v) > tol)
                std::printf("    %s: %g -> '%s' -> %g\n", d.id, v, formatValue(i, v).toRawUTF8(), back);
            ZS_CHECK(std::abs(back - v) <= tol);

            float fromPreset;
            ZS_CHECK(presetTextToValue(i, valueToPresetText(i, v), fromPreset));
            ZS_CHECK_NEAR(fromPreset, v, 1.0e-4 * (1.0 + std::abs(v)));
        }
    }
}

// ================================================================ presets ====

ZS_TEST(preset_xml_round_trip)
{
    Random rng(99);
    PresetData p;
    p.name = "Round Trip";
    p.category = "Test";
    p.author = "Tests";
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = paramDef(i);
        float v = d.min + (d.max - d.min) * rng.unipolar();
        if (d.type != ParamType::Float)
            v = std::round(v);
        p.values[(size_t) i] = v;
    }
    const auto xml = presetToXml(p);
    PresetData back;
    const auto r = presetFromXml(*xml, back);
    ZS_CHECK(r.ok);
    ZS_CHECK(r.warnings.isEmpty());
    ZS_CHECK(back.name == p.name && back.category == p.category);
    for (int i = 0; i < kNumParams; ++i)
        ZS_CHECK_NEAR(back.values[(size_t) i], p.values[(size_t) i], 1.0e-4 * (1.0 + std::abs(p.values[(size_t) i])));

    // And through a file.
    const auto file = tempDir("xml").getChildFile("rt.zspreset");
    ZS_CHECK(savePresetFile(file, p).wasOk());
    PresetData fromFile;
    ZS_CHECK(loadPresetFile(file, fromFile).ok);
    ZS_CHECK_NEAR(fromFile.values[P::FltCutoff], p.values[P::FltCutoff], 0.01);
}

ZS_TEST(invalid_presets_are_handled_safely)
{
    PresetData out;
    out.name = "Unchanged";

    // Wrong root element.
    auto wrong = juce::parseXML("<SomethingElse formatVersion=\"1\"/>");
    ZS_CHECK(! presetFromXml(*wrong, out).ok);
    ZS_CHECK(out.name == "Unchanged");

    // Missing version.
    auto noVersion = juce::parseXML("<ZSynthPreset name=\"x\"/>");
    ZS_CHECK(! presetFromXml(*noVersion, out).ok);

    // Unknown ids, bad values, out-of-range values, newer format: loads with warnings.
    auto messy = juce::parseXML(
        "<ZSynthPreset formatVersion=\"7\" name=\"Messy\">"
        "<Param id=\"flt.cutoff\" value=\"999999\"/>"
        "<Param id=\"osc1.wave\" value=\"NotAWave\"/>"
        "<Param id=\"no.such.param\" value=\"1\"/>"
        "<Param id=\"amp.level\" value=\"nan\"/>"
        "<Param id=\"voice.poly\" value=\"7\"/>"
        "</ZSynthPreset>");
    const auto r = presetFromXml(*messy, out);
    ZS_CHECK(r.ok);
    ZS_CHECK(r.warnings.size() >= 3);
    ZS_CHECK(out.values[P::FltCutoff] == 20000.0f);                 // clamped
    ZS_CHECK(out.values[P::Osc1Wave] == paramDef(P::Osc1Wave).def); // default kept
    ZS_CHECK(out.values[P::AmpLevel] == paramDef(P::AmpLevel).def);
    ZS_CHECK(out.values[P::Polyphony] == 7.0f);

    // Damaged files.
    const auto dir = tempDir("bad");
    const auto truncated = dir.getChildFile("truncated.zspreset");
    truncated.replaceWithText("<ZSynthPreset formatVersion=\"1\" name=\"Trunc\"><Param id=\"flt.cu");
    ZS_CHECK(! loadPresetFile(truncated, out).ok);
    const auto binary = dir.getChildFile("binary.zspreset");
    juce::MemoryBlock junk(4096);
    for (size_t i = 0; i < junk.getSize(); ++i)
        junk[i] = (char) (i * 37);
    binary.replaceWithData(junk.getData(), junk.getSize());
    ZS_CHECK(! loadPresetFile(binary, out).ok);
    ZS_CHECK(! loadPresetFile(dir.getChildFile("missing.zspreset"), out).ok);
}

ZS_TEST(processor_state_round_trip_including_midi_learn)
{
    ProcessorFixture a("stateA"), b("stateB");
    Random rng(7);
    auto& sa = a.processor.getState();
    for (int i = 0; i < kNumParams; ++i)
        if (auto* p = sa.getParameter(paramDef(i).id))
            p->setValueNotifyingHost(rng.unipolar());

    // MIDI learn: map CC 74 to cutoff by "turning a knob" while learning.
    a.processor.getMidiLearn().startLearning(P::FltCutoff);
    juce::MidiBuffer cc;
    cc.addEvent(juce::MidiMessage::controllerEvent(1, 74, 100), 0);
    a.processor.getMidiLearn().process(cc);
    ZS_CHECK(a.processor.getMidiLearn().getControllerFor(P::FltCutoff) == 74);

    juce::MemoryBlock blob;
    a.processor.getStateInformation(blob);
    b.processor.setStateInformation(blob.getData(), (int) blob.getSize());

    for (int i = 0; i < kNumParams; ++i)
        ZS_CHECK_NEAR(b.processor.getValue(i), a.processor.getValue(i), 1.0e-3 * (1.0 + std::abs(a.processor.getValue(i))));
    ZS_CHECK(b.processor.getMidiLearn().getControllerFor(P::FltCutoff) == 74);

    // Garbage state is ignored without crashing or changing anything.
    const float before = b.processor.getValue(P::FltRes);
    const char junk[] = "this is not a state blob";
    b.processor.setStateInformation(junk, (int) sizeof(junk));
    ZS_CHECK(b.processor.getValue(P::FltRes) == before);
}

ZS_TEST(midi_learn_drives_parameter)
{
    ProcessorFixture f("learn");
    auto& learn = f.processor.getMidiLearn();
    learn.startLearning(P::FltRes);
    juce::MidiBuffer first;
    first.addEvent(juce::MidiMessage::controllerEvent(1, 20, 0), 0);
    learn.process(first);
    learn.dispatchPending(f.processor.getState());

    juce::MidiBuffer move;
    move.addEvent(juce::MidiMessage::controllerEvent(1, 20, 127), 0);
    learn.process(move);
    learn.dispatchPending(f.processor.getState());
    ZS_CHECK_NEAR(f.processor.getValue(P::FltRes), 1.0, 1.0e-4);

    // Reserved controllers (mod wheel, sustain) cannot be learned.
    learn.startLearning(P::FltDrive);
    juce::MidiBuffer wheel;
    wheel.addEvent(juce::MidiMessage::controllerEvent(1, 1, 64), 0);
    learn.process(wheel);
    ZS_CHECK(learn.getControllerFor(P::FltDrive) == -1);
    learn.cancelLearning();
}

ZS_TEST(preset_manager_user_workflow)
{
    ProcessorFixture f("presetmgr");
    auto& pm = f.processor.getPresets();
    const int factoryCount = (int) pm.getEntries().size();
    ZS_CHECK(factoryCount >= 200);
    ZS_CHECK(pm.getBankNames().size() == 3);
    ZS_CHECK(pm.getCategories("Classic Machines").contains("Yamaha DX7"));
    ZS_CHECK(pm.getEntries()[0].bank == "ZYNTH Originals");

    // Globals are not part of a preset recall.
    f.processor.getState().getParameter("master.volume")->setValueNotifyingHost(0.25f);
    const float masterBefore = f.processor.getValue(P::MasterVolume);
    pm.load(3);
    ZS_CHECK_NEAR(f.processor.getValue(P::MasterVolume), masterBefore, 1.0e-4);

    // Save, recall after modification, rename, favourite, delete.
    f.processor.getState().getParameter("flt.res")->setValueNotifyingHost(0.77f);
    ZS_CHECK(pm.saveUser("My Test Sound", "User", false).wasOk());
    ZS_CHECK(pm.saveUser("My Test Sound", "User", false).failed()); // no silent overwrite
    ZS_CHECK((int) pm.getEntries().size() == factoryCount + 1);
    const int userIndex = pm.getCurrentIndex();
    ZS_CHECK(userIndex >= 0 && ! pm.getEntries()[(size_t) userIndex].factory);

    f.processor.getState().getParameter("flt.res")->setValueNotifyingHost(0.1f);
    ZS_CHECK(pm.load(userIndex));
    ZS_CHECK_NEAR(f.processor.getValue(P::FltRes), 0.77, 1.0e-3);

    ZS_CHECK(pm.renameUser(userIndex, "Renamed: Sound?").wasOk());
    int renamed = -1;
    for (int i = 0; i < (int) pm.getEntries().size(); ++i)
        if (pm.getEntries()[(size_t) i].name == "Renamed Sound")
            renamed = i;
    ZS_CHECK(renamed >= 0);
    pm.setFavourite(renamed, true);
    ZS_CHECK(pm.getEntries()[(size_t) renamed].favourite);

    // Factory presets are protected.
    ZS_CHECK(pm.renameUser(0, "Nope").failed());
    ZS_CHECK(pm.deleteUser(0).failed());

    ZS_CHECK(pm.deleteUser(renamed).wasOk());
    ZS_CHECK((int) pm.getEntries().size() == factoryCount);

    // A fresh manager on the same folder sees the same user presets (restart).
    ZS_CHECK(pm.saveUser("Persistent", "Pad", false).wasOk());
    PresetManager second(f.processor.getState(), banks(), f.dir);
    bool found = false;
    for (const auto& e : second.getEntries())
        found = found || (e.name == "Persistent" && e.category == "Pad");
    ZS_CHECK(found);
}

// ========================================================= factory library ====

ZS_TEST(factory_bank_parses_cleanly)
{
    juce::StringArray errors;
    const auto bank = allFactoryPresets(errors);
    for (const auto& e : errors)
        std::printf("    %s\n", e.toRawUTF8());
    ZS_CHECK(errors.isEmpty());
    ZS_CHECK(bank.size() >= 200);

    // Classic Machines: one folder per instrument, each with at least two sounds.
    juce::StringArray classicErrors;
    const auto classics = parsePresetBank(banks()[1].text, classicErrors);
    std::map<juce::String, int> perMachine;
    for (const auto& p : classics)
        ++perMachine[p.category];
    ZS_CHECK(perMachine.size() >= 18);
    for (const auto& [machine, count] : perMachine)
    {
        if (count < 2)
            std::printf("    folder '%s' has only %d preset(s)\n", machine.toRawUTF8(), count);
        ZS_CHECK(count >= 2);
    }
    juce::StringArray names;
    for (const auto& p : bank)
    {
        if (names.contains(p.name))
            std::printf("    duplicate name: %s\n", p.name.toRawUTF8());
        ZS_CHECK(! names.contains(p.name));
        names.add(p.name);
        ZS_CHECK(p.category != "Uncategorised");
    }

    // No two presets may be (near-)identical: every pair differs in several parameters.
    for (size_t a = 0; a < bank.size(); ++a)
        for (size_t b = a + 1; b < bank.size(); ++b)
        {
            int differing = 0;
            for (int i = 0; i < kNumParams; ++i)
                if (std::abs(bank[a].values[(size_t) i] - bank[b].values[(size_t) i]) > 1.0e-4f)
                    ++differing;
            if (differing < 3)
                std::printf("    near-duplicate: '%s' vs '%s' (%d params differ)\n", bank[a].name.toRawUTF8(), bank[b].name.toRawUTF8(), differing);
            ZS_CHECK(differing >= 3);
        }
}

ZS_TEST(first_install_stars_the_default_favourites)
{
    // A fresh user folder of its own (favourites.txt lives beside Presets/).
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("zsynth_tests_seed");
    root.deleteRecursively();
    const auto names = [](PresetManager& pm)
    {
        juce::StringArray s;
        for (int i : pm.getFavouriteIndices())
            s.add(pm.getEntries()[(size_t) i].name);
        s.sort(true);
        return s;
    };
    {
        ZSynthProcessor p(root.getChildFile("Presets"));
        ZS_CHECK(p.getPresets().getCurrentName() == "OB-Xa - Jump");
        const auto favs = names(p.getPresets());
        ZS_CHECK(favs == juce::StringArray({ "AXEL F", "JP-8000 - Resonant Sweep", "OB-Xa - Jump", "Synthex - Galactic Lead" }));
        // Un-starring one sticks: it is not seeded again next time.
        auto& pm = p.getPresets();
        for (int i : pm.getFavouriteIndices())
            if (pm.getEntries()[(size_t) i].name == "AXEL F")
                pm.setFavourite(i, false);
    }
    {
        ZSynthProcessor p(root.getChildFile("Presets"));
        ZS_CHECK(names(p.getPresets()) == juce::StringArray({ "JP-8000 - Resonant Sweep", "OB-Xa - Jump", "Synthex - Galactic Lead" }));
    }
    root.deleteRecursively();
}

ZS_TEST(favourites_folder_steps_through_favourites)
{
    ProcessorFixture f("favourites");
    auto& pm = f.processor.getPresets();
    for (int i : pm.getFavouriteIndices())
        pm.setFavourite(i, false);
    for (int i : { 5, 40, 120 })
        pm.setFavourite(i, true);
    ZS_CHECK(pm.getFavouriteIndices() == std::vector<int>({ 5, 40, 120 }));

    // Chosen from the Favourites folder: < > stay within the favourites, wrapping.
    pm.setFavouritesMode(true);
    pm.load(40);
    pm.next();
    ZS_CHECK(pm.getCurrentIndex() == 120);
    pm.next();
    ZS_CHECK(pm.getCurrentIndex() == 5);
    pm.previous();
    ZS_CHECK(pm.getCurrentIndex() == 120);

    // Chosen from a normal folder: the whole library.
    pm.setFavouritesMode(false);
    pm.load(40);
    pm.next();
    ZS_CHECK(pm.getCurrentIndex() == 41);

    // Favourites persist across sessions.
    {
        ProcessorFixture again("favourites");
        ZS_CHECK(again.processor.getPresets().getFavouriteIndices() == std::vector<int>({ 5, 40, 120 }));
    }
    // With no favourites left, favourites mode falls back to the library.
    for (int i : { 5, 40, 120 })
        pm.setFavourite(i, false);
    pm.setFavouritesMode(true);
    pm.load(10);
    pm.next();
    ZS_CHECK(pm.getCurrentIndex() == 11);
    ZS_CHECK(! pm.isFavouritesMode());
}

ZS_TEST(factory_presets_render_cleanly_and_level_matched)
{
    juce::StringArray errors;
    const auto bank = allFactoryPresets(errors);
    int failures = 0;
    for (const auto& p : bank)
    {
        const auto m = renderPreset(p);
        // Sustained sounds are judged by loudness; percussion by peak level,
        // because a transient's energy is too short for an RMS window.
        const double peakDb = 20.0 * std::log10(m.peak + 1.0e-9);
        const bool levelOk = p.category == "Percussion" ? (peakDb >= -14.0 && peakDb <= 0.0)
                                                         : (m.loudDb >= kLoudMin && m.loudDb <= kLoudMax);
        const bool ok = m.finite && m.peak > 1.0e-3f && m.peak <= 1.0f && levelOk;
        if (! ok)
        {
            ++failures;
            std::printf("    %-28s finite=%d peak=%.1f dB loud=%.1f dB\n", p.name.toRawUTF8(), (int) m.finite,
                        20.0 * std::log10(m.peak + 1e-9), m.loudDb);
        }
    }
    ZS_CHECK(failures == 0);
}

ZS_TEST(factory_presets_at_other_sample_rates)
{
    juce::StringArray errors;
    const auto bank = allFactoryPresets(errors);
    for (size_t i = 0; i < bank.size(); i += 9)
        for (double sr : { 44100.0, 96000.0 })
        {
            const auto m = renderPreset(bank[i], sr);
            ZS_CHECK(m.finite);
            ZS_CHECK_GT(m.peak, 1.0e-3);
        }
}

// ================================================================= layout ====

ZS_TEST(layout_every_parameter_exactly_once)
{
    const auto& layout = ui::PanelLayout::get();
    std::vector<int> count((size_t) kNumParams, 0);
    for (const auto& c : layout.controls())
    {
        const int i = findParamIndex(c.paramId);
        ZS_CHECK(i >= 0);
        if (i >= 0)
            ++count[(size_t) i];
    }
    for (int i = 0; i < kNumParams; ++i)
    {
        // MASTER is both the header knob and the OUTPUT fader (two views of one value).
        const int expected = i == P::MasterVolume ? 2 : 1;
        if (count[(size_t) i] != expected)
            std::printf("    %s appears %d times\n", paramDef(i).id, count[(size_t) i]);
        ZS_CHECK(count[(size_t) i] == expected);
    }
}

ZS_TEST(layout_controls_inside_sections_and_not_overlapping)
{
    const auto& layout = ui::PanelLayout::get();
    const auto& controls = layout.controls();
    for (const auto& c : controls)
    {
        ZS_CHECK(layout.pageBounds(c.page).contains(c.bounds));
        const auto section = layout.section(c.section);
        if (! section.contains(c.bounds))
            std::printf("    %s outside section %s\n", c.paramId, c.section.toRawUTF8());
        ZS_CHECK(section.contains(c.bounds));
    }
    for (size_t a = 0; a < controls.size(); ++a)
        for (size_t b = a + 1; b < controls.size(); ++b)
            if (controls[a].page == controls[b].page && controls[a].bounds.intersects(controls[b].bounds)
                && ! (controls[a].view.isNotEmpty() && controls[b].view.isNotEmpty() && controls[a].view != controls[b].view))
            {
                std::printf("    overlap: %s / %s\n", controls[a].paramId, controls[b].paramId);
                ZS_CHECK(false);
            }
    for (const auto& u : layout.utilities())
    {
        if (! layout.pageBounds(u.page).contains(u.bounds))
            std::printf("    utility %s outside page %s\n", u.id.toRawUTF8(), u.page.toRawUTF8());
        ZS_CHECK(layout.pageBounds(u.page).contains(u.bounds));
        for (const auto& c : controls)
            if (c.page == u.page && u.bounds.intersects(c.bounds))
            {
                std::printf("    overlap: utility %s / %s\n", u.id.toRawUTF8(), c.paramId);
                ZS_CHECK(false);
            }
        for (const auto& v : layout.utilities())
            if (&v < &u && v.page == u.page && v.bounds.intersects(u.bounds))
            {
                std::printf("    overlap: utility %s / %s\n", u.id.toRawUTF8(), v.id.toRawUTF8());
                ZS_CHECK(false);
            }
    }
    const auto& sections = layout.sections();
    for (size_t a = 0; a < sections.size(); ++a)
        for (size_t b = a + 1; b < sections.size(); ++b)
            if (sections[a].page == sections[b].page && ! sections[a].subPanel && ! sections[b].subPanel
                && sections[a].bounds.intersects(sections[b].bounds))
            {
                std::printf("    section overlap: %s / %s\n", sections[a].id.toRawUTF8(), sections[b].id.toRawUTF8());
                ZS_CHECK(false);
            }
    // Popup pages float inside the main canvas.
    for (const auto& pg : layout.popupPages())
        ZS_CHECK(layout.pageBounds(ui::kMainPage).contains(pg.boundsOnCanvas));
    // Most-used controls stay on the main panel: only the matrix, FM operator
    // detail and sequencer steps live in popups.
    for (const auto& c : controls)
    {
        const juce::String id(c.paramId);
        const bool advanced = id.startsWith("mod") || id.startsWith("fm.op") || id.startsWith("seq.s")
                            || (id.startsWith("out.valve.") && id != "out.valve.drive" && id != "out.valve.on")
                            || id == "fx.dly.wow" || id == "fx.dly.flutter" || id == "fx.dly.age" || id == "fx.dly.spread"
                            || id == "fx.dly.duck" || id == "fx.rev.decay" || id == "fx.rev.lowcut" || id == "fx.rev.diff"
                            || id == "fx.rev.mod" || id == "fx.rev.early" || id == "fx.rev.width"
                            || id == "comp.ratio" || id == "comp.attack" || id == "comp.release" || id == "comp.autorel"
                            || id == "comp.knee" || id == "comp.emph" || id == "comp.color"
                            || id.startsWith("fx.rev.eq.") || (id.startsWith("peq.") && id != "peq.on" && id != "peq.pre")
                            || id.startsWith("wt.seq.") || id.startsWith("vec.")
                            || (id.startsWith("trk.") && id != "trk.on")
                            || id == "eq.drive" || id.startsWith("pul.") || id.startsWith("nev.") || id.startsWith("a5.")
                            || id.startsWith("b5.") || id.startsWith("g560.")
                            || id == "tape.speed" || id == "tape.bias" || id == "tape.hiss" || id == "tape.out"
                            || (id.contains(".wt.") && ! id.endsWith(".wt.pos")) || id.startsWith("wt.evo.")
                            || id == "wt.drift" || id == "wt.drate"
                            || (id.startsWith("split.") && id != "split.mode")
                            || (id.startsWith("harm.") && id != "harm.on")
                            || id == "out.character" || id.startsWith("pseq.");
        ZS_CHECK(advanced == (c.page != ui::kMainPage));
    }
}

ZS_TEST(fx_presets_parse_and_cover_every_type)
{
    juce::StringArray errors;
    juce::StringArray delayGroups, reverbGroups;
    for (const auto& p : delayPresets())
    {
        ZS_CHECK(! parseFxPreset(p, &errors).empty());
        delayGroups.addIfNotAlreadyThere(p.group);
    }
    for (const auto& p : reverbPresets())
    {
        ZS_CHECK(! parseFxPreset(p, &errors).empty());
        reverbGroups.addIfNotAlreadyThere(p.group);
    }
    for (const auto& e : errors)
        std::printf("    bad fx preset entry: %s\n", e.toRawUTF8());
    ZS_CHECK(errors.isEmpty());
    // Every type has presets, and each group name is a real type.
    ZS_CHECK(delayGroups.size() == (int) DelayType::Count);
    ZS_CHECK(reverbGroups.size() == (int) ReverbType::Count);
    for (const auto& g : delayGroups)
        ZS_CHECK(juce::StringArray::fromTokens(ZS_CH_DELAY_TYPE, "|", "").contains(g));
    for (const auto& g : reverbGroups)
        ZS_CHECK(juce::StringArray::fromTokens(ZS_CH_REVERB_TYPE, "|", "").contains(g));
}

ZS_TEST(new_instance_starts_on_the_startup_preset)
{
    ProcessorFixture f("startup");
    ZS_CHECK(f.processor.getPresets().getCurrentName() == "OB-Xa - Jump");
    // The sound, not the globals: master volume stays at its default.
    ZS_CHECK_NEAR(f.processor.getValue(P::MasterVolume), paramDef(P::MasterVolume).def, 1e-4);
    ZS_CHECK(f.processor.getValue(P::Osc2Fine) > 0.0f); // the detuned second saw
}

ZS_TEST(sequencer_phrases_load_into_rows)
{
    const auto names = ZSynthProcessor::getPhraseNames();
    ZS_CHECK(names.size() == 35);
    ZS_CHECK(names.contains("Jump"));
    ZS_CHECK(names[0] == "Mono Bassline");
    ProcessorFixture f("phrases");
    for (int i = 0; i < names.size(); ++i)
    {
        ZS_CHECK(f.processor.loadPhraseIntoRow(i, i % 2));
        // Every phrase leaves at least one step playing, within the knobs' range.
        const int base = P::PseqAOn + (i % 2) * kPseqRowStride - P::PseqAOn;
        int gates = 0;
        for (int st = 0; st < kPseqSteps; ++st)
        {
            gates += f.processor.getValue(P::PseqAGate1 + base + st * kPseqStepStride) >= 0.5f ? 1 : 0;
            const float pitch = f.processor.getValue(P::PseqAPitch1 + base + st * kPseqStepStride);
            ZS_CHECK(pitch >= -12.0f && pitch <= 24.0f);
        }
        ZS_CHECK(gates > 0);
    }
    ZS_CHECK(! f.processor.loadPhraseIntoRow(names.size(), 0));
}

ZS_TEST(sequencer_imports_a_midi_file_and_plays)
{
    // A typical DAW export: type 1, a tempo track, then a note track on
    // channel 2 with a program change, volume and a few chords.
    juce::MidiMessageSequence tempo, notes;
    tempo.addEvent(juce::MidiMessage::textMetaEvent(3, "Tempo"), 0.0);
    tempo.addEvent(juce::MidiMessage::tempoMetaEvent(600000), 0.0);
    tempo.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
    tempo.addEvent(juce::MidiMessage::endOfTrack(), 3840.0);
    notes.addEvent(juce::MidiMessage::textMetaEvent(3, "Piano"), 0.0);
    notes.addEvent(juce::MidiMessage::programChange(2, 4), 0.0);
    notes.addEvent(juce::MidiMessage::controllerEvent(2, 7, 100), 0.0);
    for (int bar = 0; bar < 2; ++bar)
        for (int n : { 60, 64, 67 })
        {
            notes.addEvent(juce::MidiMessage::noteOn(2, n, (uint8_t) 96), bar * 1920.0);
            notes.addEvent(juce::MidiMessage::noteOff(2, n), bar * 1920.0 + 1800.0);
        }
    notes.addEvent(juce::MidiMessage::endOfTrack(), 3840.0);
    notes.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(480);
    file.addTrack(tempo);
    file.addTrack(notes);

    ProcessorFixture f("external-midi");
    const auto midiFile = f.dir.getChildFile("export.mid");
    {
        juce::FileOutputStream out(midiFile);
        ZS_CHECK(out.openedOk() && file.writeTo(out, 1));
    }
    ZS_CHECK(f.processor.loadMidiFileIntoRow(midiFile, 0).wasOk());
    // Row A takes the top line: G (67) on the two bar downbeats, held (slides) between them.
    ZS_CHECK(juce::roundToInt(f.processor.getValue(P::PseqARoot)) == 67);
    ZS_CHECK(f.processor.getValue(P::PseqAGate1) >= 0.5f);
    ZS_CHECK(juce::roundToInt(f.processor.getValue(P::PseqAPitch1)) == 0);

    f.processor.prepareToPlay(48000.0, 512);
    auto* run = f.processor.getState().getParameter("pseq.run");
    run->setValueNotifyingHost(1.0f);
    juce::AudioBuffer<float> buffer(2, 512);
    double energy = 0.0;
    int noteOnsSeen = 0;
    for (int b = 0; b < 200; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        f.processor.processBlock(buffer, midi);
        for (const auto m : midi)
            noteOnsSeen += m.getMessage().isNoteOn() ? 1 : 0;
        energy += buffer.getRMSLevel(0, 0, 512);
    }
    std::printf("    imported file playing: mean RMS %.4f (steps A %d)\n", energy / 200.0, f.processor.getSeqStep(0));
    ZS_CHECK_GT(energy / 200.0, 0.005);
    ZS_CHECK(f.processor.isSeqRunning());
    ZS_CHECK(f.processor.getSeqStep(0) >= 0);
    run->setValueNotifyingHost(0.0f);
    buffer.clear();
    juce::MidiBuffer none;
    f.processor.processBlock(buffer, none);
    ZS_CHECK(f.processor.getSeqStep(0) == -1);
    juce::ignoreUnused(noteOnsSeen);
}

ZS_TEST(audition_trims_silent_intro_and_tail)
{
    // Notes start at bar 5 and the file runs on to bar 40 (a typical export).
    juce::MidiMessageSequence track;
    track.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0.0);
    track.addEvent(juce::MidiMessage::controllerEvent(1, 1, 64), 0.0);
    track.addEvent(juce::MidiMessage::noteOn(1, 60, (uint8_t) 100), 4 * 1920.0);
    track.addEvent(juce::MidiMessage::noteOff(1, 60), 4 * 1920.0 + 960.0);
    track.addEvent(juce::MidiMessage::noteOn(1, 62, (uint8_t) 100), 5 * 1920.0);
    track.addEvent(juce::MidiMessage::noteOff(1, 62), 5 * 1920.0 + 1940.0); // overhangs the bar line slightly
    track.addEvent(juce::MidiMessage::endOfTrack(), 40 * 1920.0);
    juce::MidiFile file;
    file.setTicksPerQuarterNote(480);
    file.addTrack(track);
    juce::MemoryOutputStream out;
    ZS_CHECK(file.writeTo(out));
    juce::String error;
    auto clip = AuditionPlayer::parse(out.getData(), out.getDataSize(), "late", error);
    ZS_CHECK(clip != nullptr);
    if (clip == nullptr)
        return;
    ZS_CHECK_NEAR(clip->trimmedSeconds, 8.0, 1e-6);     // four 2 s bars of silence removed
    ZS_CHECK_NEAR(clip->lengthSeconds, 4.0, 1e-6);      // two bars of music, not 36
    bool firstNoteAtZero = false, offInside = true;
    for (const auto& e : clip->events)
    {
        firstNoteAtZero = firstNoteAtZero || (e.status == 0x90 && e.data1 == 60 && e.seconds == 0.0);
        offInside = offInside && e.seconds <= clip->lengthSeconds;
    }
    ZS_CHECK(firstNoteAtZero);
    ZS_CHECK(offInside);
    int offs = 0;
    for (const auto& e : clip->events)
        offs += e.status == 0x80 ? 1 : 0;
    ZS_CHECK(offs == 2); // the overhanging note-off was kept (moved to the loop end)
}

ZS_TEST(audition_octave_shift)
{
    juce::MidiMessageSequence track;
    track.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0.0);
    track.addEvent(juce::MidiMessage::noteOn(1, 60, (uint8_t) 100), 0.0);
    track.addEvent(juce::MidiMessage::noteOff(1, 60), 1800.0);
    track.addEvent(juce::MidiMessage::noteOn(1, 120, (uint8_t) 100), 960.0); // goes out of range at +1
    track.addEvent(juce::MidiMessage::noteOff(1, 120), 1440.0);
    track.addEvent(juce::MidiMessage::endOfTrack(), 1920.0);
    track.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(480);
    file.addTrack(track);
    juce::MemoryOutputStream out;
    ZS_CHECK(file.writeTo(out));
    juce::String error;
    AuditionPlayer player;
    player.setClip(AuditionPlayer::parse(out.getData(), out.getDataSize(), "oct", error));
    player.setOctave(5);
    ZS_CHECK(player.getOctave() == AuditionPlayer::kMaxOctave);
    player.setOctave(1);
    player.play();
    std::vector<juce::MidiMessage> got;
    auto run = [&](double seconds) {
        for (int b = 0; b < (int) (seconds * 48000.0 / 512); ++b)
        {
            juce::MidiBuffer midi;
            player.render(midi, 512, 48000.0, 1);
            for (const auto m : midi)
                got.push_back(m.getMessage());
        }
    };
    run(0.5);
    ZS_CHECK(got.size() == 1 && got[0].isNoteOn() && got[0].getNoteNumber() == 72);
    // Changing octave mid-note ends the note it started.
    got.clear();
    player.setOctave(-1);
    run(1.4);
    ZS_CHECK(! got.empty() && got[0].isNoteOff() && got[0].getNoteNumber() == 72);
    // 120 - 12 = 108 now plays; its off matches.
    int on108 = 0, off108 = 0;
    for (const auto& m : got)
    {
        on108 += m.isNoteOn() && m.getNoteNumber() == 108;
        off108 += m.isNoteOff() && m.getNoteNumber() == 108;
    }
    ZS_CHECK(on108 == 1 && off108 == 1);
    // At +1 the 120 is out of range: neither its on nor its off is sent.
    player.setOctave(1);
    got.clear();
    run(2.0);
    for (const auto& m : got)
        ZS_CHECK(m.getNoteNumber() <= 127 && m.getNoteNumber() != 108);
}

ZS_TEST(audition_player_timing_loop_and_release)
{
    // Two quarter notes at 120 BPM in a one-bar (2 s) file.
    juce::MidiMessageSequence track;
    track.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0.0);
    track.addEvent(juce::MidiMessage::noteOn(3, 60, (uint8_t) 100), 0.0);
    track.addEvent(juce::MidiMessage::noteOff(3, 60), 480.0);
    track.addEvent(juce::MidiMessage::noteOn(5, 64, (uint8_t) 90), 960.0);   // another channel: merged
    track.addEvent(juce::MidiMessage::noteOff(5, 64), 1440.0);
    track.addEvent(juce::MidiMessage::endOfTrack(), 1920.0);
    track.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(480);
    file.addTrack(track);
    juce::MemoryOutputStream out;
    ZS_CHECK(file.writeTo(out));

    juce::String error;
    auto clip = AuditionPlayer::parse(out.getData(), out.getDataSize(), "test", error);
    ZS_CHECK(clip != nullptr);
    if (clip == nullptr)
        return;
    ZS_CHECK(clip->notes == 2);
    ZS_CHECK_NEAR(clip->lengthSeconds, 2.0, 1e-6);

    AuditionPlayer player;
    player.setClip(std::move(clip));
    player.setLooping(true);
    player.play();
    const double sr = 48000.0;
    const int block = 512;
    std::vector<std::pair<int64_t, juce::MidiMessage>> got; // absolute sample, message
    int64_t t = 0;
    for (int b = 0; b < (int) (4.5 * sr / block); ++b, t += block)
    {
        juce::MidiBuffer midi;
        player.render(midi, block, sr, 2);
        for (const auto m : midi)
            got.emplace_back(t + m.samplePosition, m.getMessage());
    }
    // Two loops plus the start of a third: notes at 0, 1, 2, 3, 4 s.
    std::vector<int64_t> ons;
    for (const auto& [when, m] : got)
    {
        ZS_CHECK(m.getChannel() == 2);
        if (m.isNoteOn())
            ons.push_back(when);
    }
    ZS_CHECK(ons.size() == 5);
    for (size_t i = 0; i < ons.size(); ++i)
        ZS_CHECK(std::llabs(ons[i] - (int64_t) (i * sr)) <= 1);

    // Stopping mid-note releases it.
    juce::MidiBuffer midi;
    player.stop();
    player.render(midi, block, sr, 2);
    bool released = false;
    for (const auto m : midi)
        released = released || (m.getMessage().isNoteOff() && m.getMessage().getNoteNumber() == 60);
    ZS_CHECK(released);

    // Garbage: a file without notes is refused with a message.
    juce::MidiMessageSequence empty;
    empty.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0.0);
    juce::MidiFile emptyFile;
    emptyFile.setTicksPerQuarterNote(480);
    emptyFile.addTrack(empty);
    juce::MemoryOutputStream out2;
    emptyFile.writeTo(out2);
    ZS_CHECK(AuditionPlayer::parse(out2.getData(), out2.getDataSize(), "empty", error) == nullptr);
    ZS_CHECK(error.isNotEmpty());
}

ZS_TEST(musical_typing_keys_play_notes)
{
    ProcessorFixture f("typing");
    auto& kb = f.processor.getKeyboardState();
    ui::MusicalTyping t(f.processor);
    ZS_CHECK(t.isMapped(juce::KeyPress('a')));
    ZS_CHECK(t.isMapped(juce::KeyPress(juce::KeyPress::tabKey)));
    ZS_CHECK(! t.isMapped(juce::KeyPress('q')));
    ZS_CHECK(! t.isMapped(juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))); // shortcuts pass through

    t.press('a');
    ZS_CHECK(kb.isNoteOn(1, 60));
    t.press('w');
    ZS_CHECK(kb.isNoteOn(1, 61));
    t.release('w');
    ZS_CHECK(! kb.isNoteOn(1, 61));
    // Octave up while A is held: A keeps its note, new presses use the new octave.
    t.press('x');
    t.release('x');
    ZS_CHECK(t.getBaseNote() == 72);
    t.press('s');
    ZS_CHECK(kb.isNoteOn(1, 74));
    t.release('a');
    ZS_CHECK(! kb.isNoteOn(1, 60));
    t.press('c');
    t.release('c');
    ZS_CHECK(t.getVelocity() == 84);
    t.press(juce::KeyPress::tabKey);
    ZS_CHECK(t.isSustaining());
    t.allOff();
    ZS_CHECK(! kb.isNoteOn(1, 74));
    ZS_CHECK(! t.isSustaining());
    // Layout rows are complete (14.5 key units each, like a Mac keyboard).
    for (const auto& row : ui::MusicalTyping::layout())
    {
        float w = 0.0f;
        for (const auto& k : row)
            w += k.width;
        ZS_CHECK_NEAR(w, 14.5, 1e-4);
    }
}

ZS_TEST(activity_rules_examples)
{
    float v[kNumParams];
    fillDefaults(v);
    ZS_CHECK(! ui::activityFor(P::FmDepth, v, false).active);   // Analog engine: FM inactive
    v[P::Engine] = (float) EngineMode::FM;
    ZS_CHECK(ui::activityFor(P::FmDepth, v, false).active);
    ZS_CHECK(! ui::activityFor(P::Osc1Level, v, false).active);
    v[P::Engine] = (float) EngineMode::Analog;
    ZS_CHECK(! ui::activityFor(P::Osc1Pw, v, false).active);    // saw: PW inactive
    v[P::Osc1Wave] = (float) OscWave::Pulse;
    ZS_CHECK(ui::activityFor(P::Osc1Pw, v, false).active);
    ZS_CHECK(! ui::activityFor(P::Tempo, v, true).active);      // host tempo
    ZS_CHECK(ui::activityFor(P::Tempo, v, false).active);
    v[P::FltModel] = (float) FilterModel::K35;
    ZS_CHECK(! ui::activityFor(P::FltSlope, v, false).active);
}

// ================================================================ editor ====


// ============================================================ keyboard split ====

namespace
{
    struct SplitRig
    {
        explicit SplitRig(const char* name) : p(tempDir(name), ZSynthProcessor::AudioInput::Disabled)
        {
            p.prepareToPlay(48000.0, 256);
        }
        void set(const char* id, float v)
        {
            auto* prm = p.getState().getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1(v));
        }
        float run(std::initializer_list<juce::MidiMessage> msgs, int blocks = 20)
        {
            juce::AudioBuffer<float> buffer(2, 256);
            float peak = 0.0f;
            for (int b = 0; b < blocks; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0)
                    for (const auto& m : msgs)
                        midi.addEvent(m, 0);
                buffer.clear();
                p.processBlock(buffer, midi);
                peak = std::max(peak, buffer.getMagnitude(0, 256));
            }
            return peak;
        }
        int voices(int part) const { return p.getPartEngine(part).getActiveVoices(); }
        ZSynthProcessor p;
    };
}

ZS_TEST(layer_mix_is_limited)
{
    // Two loud parts, both levels up: each is limited on its own, and the final
    // mix limiter keeps their sum under full scale.
    SplitRig r("split5");
    r.set("split.mode", (float) SplitMode::Layer);
    r.set("split.lo.vol", 6.0f);
    r.set("split.up.vol", 6.0f);
    r.set("master.volume", 6.0f);
    r.set("amp.level", 1.0f);
    const float peak = r.run({ juce::MidiMessage::noteOn(1, 48, (uint8_t) 127), juce::MidiMessage::noteOn(1, 52, (uint8_t) 127),
                               juce::MidiMessage::noteOn(1, 55, (uint8_t) 127), juce::MidiMessage::noteOn(1, 60, (uint8_t) 127),
                               juce::MidiMessage::noteOn(1, 64, (uint8_t) 127) }, 80);
    std::printf("    layered mix peak %.3f\n", peak);
    ZS_CHECK(peak > 0.5f);
    ZS_CHECK(peak <= 0.97f);
}

ZS_TEST(sequencer_patterns_state_routing_and_key_transpose)
{
    SplitRig r("pseq1");
    auto& p = r.p;
    // Pattern memories: an edit stays with its pattern; switching recalls the other.
    const float demoRoot = p.getValue(P::PseqARoot);
    r.set("pseq.a.root", 48.0f);
    p.selectPattern(3);
    ZS_CHECK(p.getPattern() == 3);
    ZS_CHECK(std::abs(p.getValue(P::PseqARoot) - 48.0f) > 0.5f);
    p.selectPattern(0);
    ZS_CHECK_NEAR(p.getValue(P::PseqARoot), 48.0f, 1e-3);
    juce::ignoreUnused(demoRoot);

    // ...and they are saved with the project.
    juce::MemoryBlock state;
    p.selectPattern(5);
    r.set("pseq.b.len", 7.0f);
    p.getStateInformation(state);
    SplitRig r2("pseq2");
    r2.p.setStateInformation(state.getData(), (int) state.getSize());
    ZS_CHECK(r2.p.getPattern() == 5);
    ZS_CHECK(juce::roundToInt(r2.p.getValue(P::PseqBLen)) == 7);
    r2.p.selectPattern(0);
    ZS_CHECK_NEAR(r2.p.getValue(P::PseqARoot), 48.0f, 1e-3);

    // Routing: with SPLIT, row A plays UPPER and row B plays LOWER; with SINGLE both play the panel's sound.
    p.selectPattern(0);
    r.set("split.mode", (float) SplitMode::Split);
    r.set("pseq.run", 1.0f);
    r.run({}, 60);
    ZS_CHECK(p.isSeqRunning());
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) + r.voices(ZSynthProcessor::kLower) > 0);
    bool sawLower = false, sawUpper = false;
    for (int b = 0; b < 200; ++b)
    {
        r.run({}, 1);
        sawUpper = sawUpper || r.voices(ZSynthProcessor::kUpper) > 0;
        sawLower = sawLower || r.voices(ZSynthProcessor::kLower) > 0;
    }
    ZS_CHECK(sawUpper && sawLower);
    r.set("split.mode", (float) SplitMode::Single);
    r.run({}, 80);
    for (int b = 0; b < 100; ++b)
    {
        r.run({}, 1);
        ZS_CHECK(r.voices(ZSynthProcessor::kLower) == 0);
    }

    // KEY TRANSPOSE: a played key moves the rows instead of sounding (no extra voice of its own).
    r.set("pseq.keytr", 1.0f);
    r.run({ juce::MidiMessage::noteOn(1, 67, (uint8_t) 100) }, 1);
    r.run({ juce::MidiMessage::noteOff(1, 67) }, 1);
    r.run({}, 40);
    ZS_CHECK(p.isSeqRunning());

    // Stopping releases everything.
    r.set("pseq.run", 0.0f);
    r.run({}, 400);
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) == 0 && r.voices(ZSynthProcessor::kLower) == 0);
    ZS_CHECK(p.getSeqStep(0) == -1);
}

ZS_TEST(split_routes_keys_to_two_sounds)
{
    SplitRig r("split1");
    ZS_CHECK(r.p.getPartName(ZSynthProcessor::kLower) == "Classic Saw Bass");
    // SINGLE: everything plays the part on the panel.
    r.run({ juce::MidiMessage::noteOn(1, 40, (uint8_t) 100) });
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) > 0);
    ZS_CHECK(r.voices(ZSynthProcessor::kLower) == 0);
    r.run({ juce::MidiMessage::noteOff(1, 40) }, 200);

    // SPLIT at C4: below to LOWER, from C4 up to UPPER.
    r.set("split.mode", (float) SplitMode::Split);
    r.set("split.point", 60.0f);
    r.run({ juce::MidiMessage::noteOn(1, 48, (uint8_t) 100) });
    ZS_CHECK(r.voices(ZSynthProcessor::kLower) > 0);
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) == 0);
    r.run({ juce::MidiMessage::noteOn(1, 72, (uint8_t) 100) });
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) > 0);

    // Moving the split point while a key is held: its release still reaches the part it went to.
    r.set("split.point", 40.0f);
    const float peak = r.run({ juce::MidiMessage::noteOff(1, 48), juce::MidiMessage::noteOff(1, 72) }, 400);
    ZS_CHECK(r.voices(ZSynthProcessor::kLower) == 0);
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) == 0);
    ZS_CHECK(std::isfinite(peak));

    // LAYER: both sounds on every key.
    r.set("split.mode", (float) SplitMode::Layer);
    const float layered = r.run({ juce::MidiMessage::noteOn(1, 64, (uint8_t) 100) });
    ZS_CHECK(r.voices(ZSynthProcessor::kLower) > 0 && r.voices(ZSynthProcessor::kUpper) > 0);
    ZS_CHECK(layered > 0.01f);
    // Part volumes: both at -40 dB is (nearly) silent.
    r.run({ juce::MidiMessage::allNotesOff(1) }, 400);
    r.set("split.up.vol", -40.0f);
    r.set("split.lo.vol", -40.0f);
    r.run({}, 2); // the level change glides over one block
    const float quiet = r.run({ juce::MidiMessage::noteOn(1, 64, (uint8_t) 100) });
    std::printf("    layer peak %.3f, both parts at -40 dB %.4f\n", layered, quiet);
    ZS_CHECK(quiet < 0.02f * layered);
}

ZS_TEST(preset_change_keeps_or_turns_off_split_and_harmony)
{
    SplitRig r("split4");
    using KP = ZSynthProcessor::KeepPolicy;
    auto& pm = r.p.getPresets();
    const int other = pm.findFactory("Classic Saw Bass");
    ZS_CHECK(other >= 0);
    auto engaged = [&] { return std::make_pair(juce::roundToInt(r.p.getValue(P::SplitMode)), r.p.getValue(P::HarmOn) >= 0.5f); };

    // ASK (the default): both stay on and the editor is asked about both.
    r.p.setKeepPolicy(ZSynthProcessor::kKeepSplit, KP::Ask);
    r.p.setKeepPolicy(ZSynthProcessor::kKeepHarmony, KP::Ask);
    r.set("split.mode", (float) SplitMode::Split);
    r.set("harm.on", 1.0f);
    r.p.takeKeepQuestion();
    pm.load(other);
    ZS_CHECK(engaged() == std::make_pair((int) SplitMode::Split, true));
    ZS_CHECK(r.p.takeKeepQuestion() == (ZSynthProcessor::kKeepSplit | ZSynthProcessor::kKeepHarmony));
    r.p.turnOff(ZSynthProcessor::kKeepHarmony); // the answer "TURN OFF"
    ZS_CHECK(r.p.getValue(P::HarmOn) < 0.5f);

    // TURN OFF / KEEP without asking.
    r.p.setKeepPolicy(ZSynthProcessor::kKeepSplit, KP::TurnOff);
    r.p.setKeepPolicy(ZSynthProcessor::kKeepHarmony, KP::Keep);
    r.set("harm.on", 1.0f);
    pm.load(other);
    ZS_CHECK(engaged() == std::make_pair((int) SplitMode::Single, true));
    ZS_CHECK(r.p.takeKeepQuestion() == 0);

    // A preset saved with SPLIT on brings back the split and both sounds, whatever the policy.
    r.set("split.mode", (float) SplitMode::Split);
    r.set("split.point", 53.0f);
    const auto lowerName = r.p.getPartName(ZSynthProcessor::kLower);
    ZS_CHECK(pm.saveUser("Split Test", "User", true).wasOk());
    const int saved = pm.getCurrentIndex();
    r.set("split.mode", (float) SplitMode::Single);
    r.set("split.point", 70.0f);
    pm.load(pm.findFactory("Classic Saw Bass"));
    r.p.setEditPart(ZSynthProcessor::kLower);
    pm.loadInit();
    pm.load(saved);
    ZS_CHECK(juce::roundToInt(r.p.getValue(P::SplitMode)) == (int) SplitMode::Split);
    ZS_CHECK(juce::roundToInt(r.p.getValue(P::SplitPoint)) == 53);
    ZS_CHECK(r.p.getEditPart() == ZSynthProcessor::kUpper);
    ZS_CHECK(r.p.getPartName(ZSynthProcessor::kLower) == lowerName);

    r.p.setKeepPolicy(ZSynthProcessor::kKeepSplit, KP::Ask);
    r.p.setKeepPolicy(ZSynthProcessor::kKeepHarmony, KP::Ask);
}

ZS_TEST(split_edit_part_swaps_the_panel)
{
    SplitRig r("split2");
    const auto upperName = r.p.getPartName(ZSynthProcessor::kUpper);
    PresetData upper = capturePreset(r.p.getState());
    r.p.setEditPart(ZSynthProcessor::kLower);
    ZS_CHECK(r.p.getEditPart() == ZSynthProcessor::kLower);
    ZS_CHECK(r.p.getPresets().getCurrentName() == "Classic Saw Bass");
    ZS_CHECK(r.p.getPartName(ZSynthProcessor::kUpper) == upperName);
    // Editing the LOWER part changes only it.
    r.set("flt.cutoff", 321.0f);
    r.p.setEditPart(ZSynthProcessor::kUpper);
    const PresetData back = capturePreset(r.p.getState());
    int differing = 0;
    for (int i = 0; i < kNumParams; ++i)
        differing += std::abs(back.values[(size_t) i] - upper.values[(size_t) i]) > 1.0e-4f ? 1 : 0;
    ZS_CHECK(differing == 0);
    ZS_CHECK(r.p.getPresets().getCurrentName() == upperName);
    r.p.setEditPart(ZSynthProcessor::kLower);
    ZS_CHECK_NEAR(r.p.getValue(P::FltCutoff), 321.0, 0.5);

    // The whole setup survives a session save / recall.
    r.set("split.mode", (float) SplitMode::Split);
    r.set("split.point", 55.0f);
    juce::MemoryBlock state;
    r.p.getStateInformation(state);
    SplitRig q("split3");
    q.p.setStateInformation(state.getData(), (int) state.getSize());
    ZS_CHECK(q.p.getEditPart() == ZSynthProcessor::kLower);
    ZS_CHECK_NEAR(q.p.getValue(P::FltCutoff), 321.0, 0.5);
    ZS_CHECK(q.p.getPartName(ZSynthProcessor::kUpper) == upperName);
    ZS_CHECK(juce::roundToInt(q.p.getValue(P::SplitPoint)) == 55);
    q.p.setEditPart(ZSynthProcessor::kUpper);
    ZS_CHECK(q.p.getPresets().getCurrentName() == upperName);
}

ZS_TEST(split_learn_takes_the_next_key)
{
    SplitRig r("split4");
    r.p.setLearningSplit(true);
    r.run({ juce::MidiMessage::noteOn(1, 53, (uint8_t) 100) });
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) == 0 && r.voices(ZSynthProcessor::kLower) == 0); // the learn key is silent
    ZS_CHECK(r.p.applyLearnedSplit());
    ZS_CHECK(juce::roundToInt(r.p.getValue(P::SplitPoint)) == 53);
    ZS_CHECK(juce::roundToInt(r.p.getValue(P::SplitMode)) == (int) SplitMode::Split);
    ZS_CHECK(! r.p.isLearningSplit());
    ZS_CHECK(formatValue(P::SplitPoint, 60.0f) == "C4");
    ZS_CHECK(std::abs(parseValue(P::SplitPoint, "F#3") - 54.0f) < 0.01f);
}


ZS_TEST(harmony_plays_through_the_processor)
{
    SplitRig plain("harm1");
    plain.run({ juce::MidiMessage::noteOn(1, 64, (uint8_t) 100) });
    const int one = plain.voices(ZSynthProcessor::kUpper);

    SplitRig r("harm2");
    r.set("harm.on", 1.0f);
    r.set("harm.voices", 2.0f);
    r.run({ juce::MidiMessage::noteOn(1, 64, (uint8_t) 100) });
    const int three = r.voices(ZSynthProcessor::kUpper);
    std::printf("    voices: %d without harmony, %d with two harmony notes\n", one, three);
    ZS_CHECK(three == 3 * one);
    r.run({ juce::MidiMessage::noteOff(1, 64) }, 400);
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) == 0);

    // Switched off mid-note: nothing is left hanging.
    r.run({ juce::MidiMessage::noteOn(1, 60, (uint8_t) 100) });
    r.set("harm.on", 0.0f);
    r.run({ juce::MidiMessage::noteOff(1, 60) }, 400);
    ZS_CHECK(r.voices(ZSynthProcessor::kUpper) == 0);

    // DETECT sets KEY and SCALE (an A minor tune).
    SplitRig d("harm3");
    d.p.startKeyDetect();
    for (int k : { 57, 60, 64, 69, 68, 69, 71, 72, 71, 69, 64, 57, 60, 64, 69, 57 })
    {
        d.run({ juce::MidiMessage::noteOn(1, k, (uint8_t) 100) }, 10);
        d.run({ juce::MidiMessage::noteOff(1, k) }, 10);
    }
    ZS_CHECK(d.p.applyDetectedKey());
    ZS_CHECK(juce::roundToInt(d.p.getValue(P::HarmKey)) == 9);
    ZS_CHECK(juce::roundToInt(d.p.getValue(P::HarmScale)) == (int) HarmScale::NaturalMinor);
    ZS_CHECK(! d.p.isDetectingKey());
}

ZS_TEST(pitch_track_plays_the_synth_from_the_audio_input)
{
    const auto dir = tempDir("track");
    ZSynthProcessor p(dir, ZSynthProcessor::AudioInput::Enabled);
    ZS_CHECK(p.hasAudioInput());
    auto set = [&](const char* id, float v) {
        auto* prm = p.getState().getParameter(id);
        prm->setValueNotifyingHost(prm->convertTo0to1(v));
    };
    set("trk.on", 1.0f);
    set("trk.dry", 0.0f);
    p.prepareToPlay(48000.0, 256);

    // An A string (110 Hz) on input 1, and an on-screen key that must be ignored.
    p.getKeyboardState().noteOn(1, 72, 1.0f);
    juce::AudioBuffer<float> buffer(2, 256);
    double phase = 0.0;
    float outPeak = 0.0f;
    int maxVoices = 0;
    for (int b = 0; b < 48000 / 256; ++b)
    {
        for (int i = 0; i < 256; ++i)
        {
            phase += 2.0 * juce::MathConstants<double>::pi * 110.0 / 48000.0;
            const float t = (float) (b * 256 + i) / 48000.0f;
            const float s = 0.3f * std::exp(-t * 2.0f) * (float) (std::sin(phase) + 0.5 * std::sin(2.0 * phase) + 0.25 * std::sin(3.0 * phase));
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, 0.0f);
        }
        juce::MidiBuffer midi;
        p.processBlock(buffer, midi);
        outPeak = std::max(outPeak, buffer.getMagnitude(0, 256));
        maxVoices = std::max(maxVoices, p.getEngine().getActiveVoices());
    }
    ZS_CHECK(p.getTracker().getNote() == 45 || p.getTracker().getNote() == -1);
    std::printf("    tracked note %d, output peak %.3f, voices %d\n", p.getTracker().getNote(), outPeak, maxVoices);
    ZS_CHECK(outPeak > 0.01f);
    ZS_CHECK(maxVoices >= 1);
    // The on-screen key was ignored: only the tracked note's voices played.
    ZS_CHECK(maxVoices <= 2 * juce::roundToInt(p.getValue(P::UnisonVoices)) + 2);

    // Switching off ends the note.
    set("trk.on", 0.0f);
    p.getKeyboardState().allNotesOff(1);
    for (int b = 0; b < 48000 * 3 / 256; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        p.processBlock(buffer, midi);
    }
    ZS_CHECK(p.getTracker().getNote() == -1);
    ZS_CHECK(p.getEngine().getActiveVoices() == 0);

    // The plug-in formats have no input: nothing to track.
    ZSynthProcessor plugin(dir, ZSynthProcessor::AudioInput::Disabled);
    ZS_CHECK(! plugin.hasAudioInput());
    ZS_CHECK(! plugin.isTracking());
}

ZS_TEST(editor_layout_is_fixed_across_presets_and_modes)
{
    juce::ScopedJuceInitialiser_GUI gui;
    ProcessorFixture f("editor");
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.processor.createEditor());
    auto* zeditor = dynamic_cast<ZSynthEditor*>(editor.get());
    ZS_CHECK(zeditor != nullptr);
    if (zeditor == nullptr)
        return;

    auto& panel = zeditor->getPanel();
    ZS_CHECK((int) panel.getControls().size() == (int) ui::PanelLayout::get().controls().size());

    // Bounds of every control and panel child, keyed by component identity
    // (z-order changes when popups come to the front; positions must not).
    auto snapshot = [&] {
        std::map<const juce::Component*, juce::Rectangle<int>> b;
        for (auto& c : panel.getControls())
            b[c.get()] = c->getBounds();
        for (int i = 0; i < panel.getNumChildComponents(); ++i)
            if (dynamic_cast<ui::PageGlow*>(panel.getChildComponent(i)) == nullptr) // follows the open window
                b[panel.getChildComponent(i)] = panel.getChildComponent(i)->getBounds();
        return b;
    };
    const auto reference = snapshot();

    auto& pm = f.processor.getPresets();
    for (int i = 0; i < (int) pm.getEntries().size(); i += 7)
    {
        pm.load(i);
        ZS_CHECK(snapshot() == reference);
    }
    for (int mode = 0; mode < 3; ++mode)
    {
        f.processor.getState().getParameter("osc.engine")->setValueNotifyingHost((float) mode / 2.0f);
        ZS_CHECK(snapshot() == reference);
    }

    // Uniform scaling only: at the minimum size the panel transform is a pure scale.
    editor->setSize(960, 600);
    ZS_CHECK_NEAR(panel.getTransform().mat00, 0.5, 1.0e-4);
    ZS_CHECK_NEAR(panel.getTransform().mat11, 0.5, 1.0e-4);
    ZS_CHECK(snapshot() == reference);
    editor->setSize(1920, 1200);
    ZS_CHECK_NEAR(panel.getTransform().mat00, 1.0, 1.0e-4);
    ZS_CHECK(snapshot() == reference);

    // Opening the browser or a popup editor overlays the panel without moving anything.
    panel.showPresetBrowser(true);
    ZS_CHECK(panel.isPresetBrowserVisible());
    panel.showPresetBrowser(false);
    ZS_CHECK(snapshot() == reference);
    for (const auto& pg : ui::PanelLayout::get().popupPages())
    {
        panel.showPage(pg.id);
        ZS_CHECK(panel.getVisiblePage() == pg.id);
        ZS_CHECK(snapshot() == reference);
    }
    panel.showPage({});
    ZS_CHECK(panel.getVisiblePage().isEmpty());
}

// ============================================================== processor ====

ZS_TEST(processor_renders_midi_and_onscreen_keyboard)
{
    ProcessorFixture f("render");
    f.processor.setRateAndBufferSizeDetails(48000.0, 512);
    f.processor.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 10);
    double energy = 0.0;
    for (int block = 0; block < 40; ++block)
    {
        buffer.clear();
        f.processor.processBlock(buffer, midi);
        midi.clear();
        energy += buffer.getRMSLevel(0, 0, 512);
    }
    ZS_CHECK_GT(energy, 0.1);

    // On-screen keyboard path: the keyboard state injects notes into the block.
    f.processor.getKeyboardState().noteOn(1, 72, 0.8f);
    buffer.clear();
    f.processor.processBlock(buffer, midi);
    ZS_CHECK(f.processor.getEngine().getActiveVoices() >= 1);

    // Panic silences everything.
    f.processor.panic();
    for (int block = 0; block < 4; ++block)
    {
        buffer.clear();
        f.processor.processBlock(buffer, midi);
    }
    ZS_CHECK(f.processor.getEngine().getActiveVoices() == 0);

    // Mono output bus.
    juce::AudioBuffer<float> mono(1, 256);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
    f.processor.processBlock(mono, midi);
    ZS_CHECK(std::isfinite(mono.getRMSLevel(0, 0, 256)));
}

// ============================================================ preset tool ====

static int presetReport(const juce::File& wavDir)
{
    juce::StringArray errors;
    const auto bank = allFactoryPresets(errors);
    for (const auto& e : errors)
        std::printf("BANK ERROR: %s\n", e.toRawUTF8());
    // (all banks)
    if (wavDir != juce::File())
        wavDir.createDirectory();

    std::printf("%-30s %-13s %8s %8s  %s\n", "preset", "category", "peak dB", "loud dB", "suggest amp.level x");
    for (const auto& p : bank)
    {
        const auto m = renderPreset(p);
        const double target = -17.0;
        std::printf("%-30s %-13s %8.1f %8.1f  %.2f%s\n", p.name.toRawUTF8(), p.category.toRawUTF8(),
                    20.0 * std::log10(m.peak + 1e-9), m.loudDb, std::pow(10.0, (target - m.loudDb) / 20.0),
                    ((p.category == "Percussion" ? (m.peak < 0.2f) : (m.loudDb < kLoudMin || m.loudDb > kLoudMax)) || ! m.finite)
                        ? "   <-- OUT OF RANGE" : "");
        if (wavDir != juce::File())
        {
            juce::AudioBuffer<float> buf(2, (int) m.left.size());
            buf.copyFrom(0, 0, m.left.data(), (int) m.left.size());
            buf.copyFrom(1, 0, m.right.data(), (int) m.right.size());
            auto file = wavDir.getChildFile(PresetManager::sanitiseName(p.name) + ".wav");
            file.deleteFile();
            juce::WavAudioFormat wav;
            if (auto out = std::unique_ptr<juce::OutputStream>(file.createOutputStream().release()))
                if (auto writer = wav.createWriterFor(out, juce::AudioFormatWriterOptions().withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(24)))
                    writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
        }
    }
    return errors.isEmpty() ? 0 : 1;
}

// Diagnostic: how much the output transformer changes a real preset, measured
// through the whole engine (difference vs. transformer off, in dB re signal).
static int xfmrReport(const juce::String& presetName)
{
    ZSynthProcessor processor(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("zynth-xfmr-diag"));
    auto& pm = processor.getPresets();
    int index = -1;
    for (int i = 0; i < (int) pm.getEntries().size(); ++i)
        if (pm.getEntries()[(size_t) i].name == presetName)
            index = i;
    if (index < 0)
        return 1;
    auto render = [&](bool on, int type, float amount, bool hot, std::vector<float>& out)
    {
        pm.load(index);
        auto set = [&](const char* id, float v) {
            auto* prm = processor.getState().getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        set("out.xfmr.on", on ? 1.0f : 0.0f);
        set("out.xfmr.type", (float) type);
        set("out.xfmr.amt", amount);
        set("out.xfmr.hot", hot ? 1.0f : 0.0f);
        processor.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        out.clear();
        for (int b = 0; b < 48000 * 3 / 512; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 2)
                for (int note : { 45, 57, 60, 64 })
                    midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
            if (b == 220)
                for (int note : { 45, 57, 60, 64 })
                    midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
            processor.processBlock(buffer, midi);
            for (int i = 0; i < 512; ++i)
                out.push_back(buffer.getSample(0, i));
        }
    };
    std::vector<float> ref, y;
    render(false, 0, 0.5f, false, ref);
    // The transformer sits near the end of the chain, before master volume:
    // undo the master gain to get (close to) the signal it actually sees.
    const float master = dbToGain(processor.getValue(P::MasterVolume));
    double sig = 0, pk = 0;
    for (float v : ref) { sig += v * v; pk = std::max(pk, (double) std::abs(v / master)); }
    std::printf("%s: transformer input RMS %.1f dBFS, peak %.1f dBFS\n", presetName.toRawUTF8(),
                10 * std::log10(sig / ref.size() / (master * master)), 20 * std::log10(pk));
    for (bool hot : { false, true })
        for (int type = 0; type < 5; ++type)
        {
            std::printf("  %s %-12s", hot ? "HOT " : "real", juce::StringArray::fromTokens(ZS_CH_XFMR_TYPE, "|", "")[type].toRawUTF8());
            for (float amt : { 0.25f, 0.5f, 0.75f, 1.0f })
            {
                auto run = [&](float scale, std::vector<float>& out)
                {
                    OutputTransformer xf;
                    xf.prepare(48000.0f);
                    std::vector<float> l(ref.size()), r(ref.size());
                    for (size_t i = 0; i < ref.size(); ++i) l[i] = r[i] = ref[i] / master * scale;
                    for (size_t i = 0; i + 512 <= l.size(); i += 512)
                        xf.process(l.data() + i, r.data() + i, 512, (XfmrType) type, amt, hot);
                    out.resize(l.size());
                    for (size_t i = 0; i < l.size(); ++i) out[i] = l[i] * master / scale;
                };
                std::vector<float> wet, lin;
                run(1.0f, wet);
                run(1e-3f, lin);
                double d = 0, e = 0;
                for (size_t i = 24000; i < ref.size(); ++i) { const double dv = wet[i] - lin[i]; d += dv * dv; e += (double) wet[i] * wet[i]; }
                double s2 = 0;
                for (size_t i = 24000; i < ref.size(); ++i) s2 += (double) ref[i] * ref[i];
                std::printf("  %3d%%: %6.1f dB (%+4.1f)", (int) (amt * 100), 10 * std::log10(d / e + 1e-12), 10 * std::log10(e / s2));
                continue;
                std::printf("  %3d%%: %6.1f dB", (int) (amt * 100), 10 * std::log10(d / sig + 1e-12));
            }
            std::printf("\n");
        }
    return 0;
}

// Diagnostic: renders the panel with each compressor model and reverb type
// (main panel and their ADVANCED pages) into PNGs in `dir`.
static int snapshots(const juce::File& dir)
{
    juce::ScopedJuceInitialiser_GUI gui;
    dir.createDirectory();
    ZSynthProcessor processor(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("zynth-snap"),
                              ZSynthProcessor::AudioInput::Enabled);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* zeditor = dynamic_cast<ZSynthEditor*>(editor.get());
    if (zeditor == nullptr)
        return 1;
    auto& panel = zeditor->getPanel();
    // On screen, so the displays refresh (ZS_OFFSCREEN=1 skips it where there is no usable display server).
    if (std::getenv("ZS_OFFSCREEN") == nullptr)
        editor->addToDesktop(0);
    editor->setVisible(true);
    auto set = [&](const char* id, float v) {
        auto* prm = processor.getState().getParameter(id);
        prm->setValueNotifyingHost(prm->convertTo0to1(v));
    };
    auto shoot = [&](const juce::String& name, const juce::String& page) {
        panel.showPage(page);
        for (int k = 0; k < 12; ++k)
            panel.refreshNow(); // activity, meters, faceplates
        const auto img = panel.createComponentSnapshot(panel.getLocalBounds(), true, 1.0f);
        juce::FileOutputStream out(dir.getChildFile(name + ".png"));
        out.setPosition(0);
        out.truncate();
        juce::PNGImageFormat().writeImageToStream(img, out);
    };
    if (const char* only = std::getenv("ZS_SNAP_ONLY"); only != nullptr && juce::String(only) == "pseq")
    {
        shoot("pseq", "pseq");
        shoot("pseqmain", {});
        return 0;
    }
    // Keyboard split.
    set("split.mode", (float) SplitMode::Split);
    set("split.point", 55.0f);
    set("split.lo.oct", -1.0f);
    set("split.lo.vol", -3.0f);
    shoot("split", "split");
    shoot("splitmain", {});
    set("harm.on", 1.0f);
    processor.getPresets().load(processor.getPresets().findFactory("Classic Saw Bass"));
    shoot("keepask", {});
    set("harm.on", 0.0f);
    set("split.mode", (float) SplitMode::Single);
    if (const char* only = std::getenv("ZS_SNAP_ONLY"); only != nullptr && juce::String(only) == "split")
        return 0;

    // Wavetable engine: three oscillators on different tables, warps and stacks.
    set("osc1.wave", (float) OscWave::Wavetable);
    set("osc1.wt.table", 1.0f + (float) WavetableId::Aurora);
    set("osc1.wt.voices", 5.0f); set("osc1.wt.pspr", 0.35f); set("osc1.wt.width", 0.8f);
    set("wt.pos", 0.35f);
    set("osc2.on", 1.0f); set("osc2.wave", (float) OscWave::Wavetable);
    set("osc2.wt.table", 1.0f + (float) WavetableId::ChoirMorph);
    set("osc2.wt.warp", (float) WtWarp::Formant); set("osc2.wt.wamt", 0.45f); set("osc2.wt.pos", 0.2f);
    set("osc3.on", 1.0f); set("osc3.wave", (float) OscWave::RomGlassPad);
    set("osc3.wt.warp", (float) WtWarp::BendPlus); set("osc3.wt.wamt", 0.6f);
    set("wt.evo.mode", (float) WtEvolve::Sweep); set("wt.drift", 0.3f);
    shoot("wtengine", "wtengine");
    shoot("wtmain", {});
    if (const char* only = std::getenv("ZS_SNAP_ONLY"); only != nullptr && juce::String(only) == "wt")
        return 0;

    set("comp.on", 1.0f);
    set("fx.rev.on", 1.0f);
    for (int m = 0; m < (int) CompModel::Count; ++m)
    {
        set("comp.model", (float) m);
        shoot("comp" + juce::String(m), "comp");
    }
    for (int r = 0; r < (int) ReverbType::Count; ++r)
    {
        set("fx.rev.type", (float) r);
        shoot("reverb" + juce::String(r), "reverb");
        shoot("reverbm" + juce::String(r), {});
    }
    // Classic EQs (with some settings so the curves show) and tape machines.
    set("peq.on", 1.0f);
    set("pul.lb", 6.0f); set("pul.la", 4.0f); set("pul.hb", 5.0f); set("pul.ha", 3.0f);
    set("nev.hpf", 1.0f); set("nev.lg", 4.0f); set("nev.mg", 6.0f); set("nev.hg", 5.0f);
    set("a5.lg", 7.0f); set("a5.mg", 3.0f); set("a5.hg", 8.0f); set("a5.hs", 1.0f);
    set("b5.lg", 9.0f); set("b5.lmg", 5.0f); set("b5.hmg", 10.0f); set("b5.hg", 9.0f);
    for (int b = 1; b <= 10; ++b)
        set(("g560.b" + juce::String(b)).toRawUTF8(), (float) ((b * 7) % 13 - 6));
    for (int m = 0; m < (int) EqModel::Count; ++m)
    {
        set("eq.model", (float) m);
        shoot("eq" + juce::String(m), "peq");
    }
    set("tape.on", 1.0f);
    for (int m = 0; m < (int) TapeModel::Count; ++m)
    {
        set("tape.model", (float) m);
        shoot("tape" + juce::String(m), "tape");
    }
    set("harm.on", 1.0f);
    set("harm.key", 9.0f); // A minor: the white keys, A outlined boldest
    set("harm.scale", (float) HarmScale::NaturalMinor);
    shoot("harmkeys", {});
    set("harm.key", 2.0f); // D major: F# and C# join
    set("harm.scale", (float) HarmScale::Major);
    shoot("harm", "harm");
    set("harm.on", 0.0f);
    set("trk.on", 1.0f);
    shoot("track", "track");
    set("trk.on", 0.0f);
    panel.showPage({});
    return 0;
}

int main(int argc, char** argv)
{
    if (argc > 2 && juce::String(argv[1]) == "--snapshots")
        return snapshots(juce::File(juce::String(argv[2])));
    if (argc > 2 && juce::String(argv[1]) == "--xfmr-report")
        return xfmrReport(juce::String(argv[2]));
    if (argc > 1 && juce::String(argv[1]) == "--preset-report")
        return presetReport(argc > 2 ? juce::File(juce::String(argv[2])) : juce::File());
    return zstest::runAll(argc, argv);
}
