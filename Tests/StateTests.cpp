// JUCE-side tests: parameter model, presets, state, MIDI learn, fixed layout,
// editor construction and the factory library. Also hosts the preset audition
// tool:  ZSynthStateTests --preset-report [wavOutputDir]

#include "TestFramework.h"
#include "TestHelpers.h"

#include "MIDI/MidiLearn.h"
#include "Parameters/ParameterLayout.h"
#include "Plugin/ZSynthProcessor.h"
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
        return { { "Z.SYNTH Originals", juce::String::fromUTF8(BinaryData::FactoryBank_zsbank, BinaryData::FactoryBank_zsbankSize) },
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
    ZS_CHECK(pm.getEntries()[0].bank == "Z.SYNTH Originals");

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
        if (count[(size_t) i] != 1)
            std::printf("    %s appears %d times\n", paramDef(i).id, count[(size_t) i]);
        ZS_CHECK(count[(size_t) i] == 1);
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
            if (controls[a].page == controls[b].page && controls[a].bounds.intersects(controls[b].bounds))
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
        const bool advanced = id.startsWith("mod") || id.startsWith("fm.op") || id.startsWith("seq.s");
        ZS_CHECK(advanced == (c.page != ui::kMainPage));
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

int main(int argc, char** argv)
{
    if (argc > 1 && juce::String(argv[1]) == "--preset-report")
        return presetReport(argc > 2 ? juce::File(juce::String(argv[2])) : juce::File());
    return zstest::runAll(argc, argv);
}
