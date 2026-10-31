// DSP engine tests: deterministic, synthetic, JUCE-free.

#include "TestFramework.h"
#include "TestHelpers.h"

#include "DSP/Common/HalfBand.h"
#include "DSP/Envelopes/Envelope.h"
#include "DSP/Effects/Compressor.h"
#include "DSP/Effects/DriveAndOutput.h"
#include "DSP/Effects/TimeEffects.h"
#include "DSP/FM/FmAlgorithms.h"
#include "DSP/FM/FmVoice.h"
#include "DSP/Filters/Filters.h"
#include "DSP/Oscillators/Oscillator.h"
#include "DSP/Wavetable/WavetableBank.h"

#include <cstdio>

using namespace zstest;

// ============================================================ oscillators ====

ZS_TEST(oscillator_frequency_accuracy)
{
    for (OscWave w : { OscWave::Saw, OscWave::Pulse, OscWave::Triangle, OscWave::Sine })
    {
        Oscillator osc;
        osc.setWave(w);
        osc.reset(0.0f);
        const double fs = 48000.0, f = 1234.5;
        int wraps = 0;
        const int n = 48000 * 4;
        for (int i = 0; i < n; ++i)
        {
            osc.process((float) (f / fs), -1.0f);
            if (osc.wrapAfter() >= 0.0f)
                ++wraps;
        }
        ZS_CHECK_NEAR(wraps / 4.0, f, 0.5);
    }
}

ZS_TEST(oscillator_waveforms_bounded_and_dc_free)
{
    for (OscWave w : { OscWave::Saw, OscWave::Square, OscWave::Pulse, OscWave::Triangle, OscWave::Sine })
    {
        Oscillator osc;
        osc.setWave(w);
        osc.setPulseWidth(0.5f);
        osc.reset(0.0f);
        double sum = 0.0;
        float peak = 0.0f;
        const int n = 96000;
        for (int i = 0; i < n; ++i)
        {
            const float v = osc.process(3000.0f / 48000.0f, -1.0f);
            sum += v;
            peak = std::max(peak, std::abs(v));
        }
        ZS_CHECK_LT(peak, 1.2);
        ZS_CHECK_LT(std::abs(sum / n), 0.01);
    }
}

namespace
{
    std::vector<float> renderOsc(OscWave w, double f, double fs, bool naive, int n = 65536, bool sync = false, double slaveRatio = 1.0)
    {
        Oscillator master, slave;
        master.setWave(OscWave::Saw);
        slave.setWave(w);
        slave.setPulseWidth(0.5f);
        std::vector<float> out((size_t) n);
        double ph = 0.0;
        for (int i = 0; i < n; ++i)
        {
            if (naive)
            {
                ph += f / fs;
                ph -= std::floor(ph);
                out[(size_t) i] = (float) (2.0 * ph - 1.0);
            }
            else if (sync)
            {
                master.process((float) (f / fs), -1.0f);
                out[(size_t) i] = slave.process((float) (f * slaveRatio / fs), master.wrapAfter());
            }
            else
            {
                out[(size_t) i] = slave.process((float) (f / fs), -1.0f);
            }
        }
        return out;
    }
}

ZS_TEST(oscillator_aliasing_polyblep_vs_naive)
{
    const double fs = 48000.0, f = 2793.83; // F7: high enough for strong aliasing
    const auto naive = renderOsc(OscWave::Saw, f, fs, true);
    const auto blep = renderOsc(OscWave::Saw, f, fs, false);
    const double naiveDb = inharmonicRatioDb(naive.data(), naive.size(), fs, f);
    const double blepDb = inharmonicRatioDb(blep.data(), blep.size(), fs, f);
    std::printf("    saw 2.79 kHz @48k (1x): naive alias %.1f dB, PolyBLEP %.1f dB\n", naiveDb, blepDb);
    ZS_CHECK_LT(blepDb, naiveDb - 10.0);
    ZS_CHECK_LT(blepDb, -30.0);
}

ZS_TEST(oscillator_aliasing_at_2x_internal_rate)
{
    // The engine runs voices at 2x and decimates; measure at the internal rate
    // what lands below the base-rate band edge.
    const double fs = 96000.0, f = 2793.83;
    for (OscWave w : { OscWave::Saw, OscWave::Pulse, OscWave::Triangle })
    {
        const auto x = renderOsc(w, f, fs, false);
        const double db = inharmonicRatioDb(x.data(), x.size(), fs, f, 20000.0);
        std::printf("    wave %d at 2x: alias in 20 Hz-20 kHz %.1f dB\n", (int) w, db);
        ZS_CHECK_LT(db, -50.0);
    }
}

ZS_TEST(oscillator_hard_sync_is_band_limited)
{
    const double fs = 96000.0, f = 440.0;
    const auto x = renderOsc(OscWave::Saw, f, fs, false, 65536, true, 2.71);
    const double db = inharmonicRatioDb(x.data(), x.size(), fs, f, 20000.0);
    std::printf("    hard-sync saw (ratio 2.71) at 2x: alias %.1f dB\n", db);
    ZS_CHECK_LT(db, -45.0);
}

ZS_TEST(sub_oscillator_tracks_octave_below)
{
    Oscillator master;
    SubOscillator sub;
    master.setWave(OscWave::Saw);
    sub.setWave(SubWave::SquareMinus1);
    const double fs = 48000.0, f = 440.0;
    std::vector<float> out(65536);
    for (auto& v : out)
    {
        master.process((float) (f / fs), -1.0f);
        v = sub.process(master.wrapAfter(), (float) (f / fs));
    }
    ZS_CHECK_NEAR(dominantFrequency(out.data(), out.size(), fs), 220.0, 1.0);
}

// ============================================================== wavetables ====

ZS_TEST(wavetables_normalised_and_band_limited)
{
    const auto& bank = WavetableBank::get();
    ZS_CHECK(bank.numTables() == (int) WavetableId::Count);
    for (int t = 0; t < bank.numTables(); ++t)
    {
        const auto& wt = bank.table(t);
        for (int f = 0; f < Wavetable::kFrames; f += 7)
        {
            const float* fr = wt.frame(f, 0);
            float peak = 0.0f;
            for (int i = 0; i < Wavetable::kSize; ++i)
                peak = std::max(peak, std::abs(fr[i]));
            ZS_CHECK_NEAR(peak, 1.0, 1.0e-3);
            ZS_CHECK(fr[Wavetable::kSize] == fr[0]);

            // Mip 5 keeps harmonics 1..32 only.
            const auto mag = magnitudeSpectrum(wt.frame(f, 5), Wavetable::kSize);
            double above = 0.0, total = 0.0;
            for (size_t k = 1; k < mag.size(); ++k)
            {
                total += mag[k] * mag[k];
                if (k > 34)
                    above += mag[k] * mag[k];
            }
            ZS_CHECK_LT(above / (total + 1e-30), 1.0e-4);
        }
    }
}

ZS_TEST(wavetable_mip_selection)
{
    ZS_CHECK(Wavetable::mipForIncrement(20.0f / 96000.0f) == 0);
    // At 5 kHz / 96 kHz only 9 harmonics fit below Nyquist.
    const int mip = Wavetable::mipForIncrement(5000.0f / 96000.0f);
    ZS_CHECK((Wavetable::kMaxHarmonics >> mip) <= 9);
}

// ================================================================= filters ====

namespace
{
    double sineGainDb(FilterModel model, FilterType type, FilterSlope slope, float cutoff, float res, double freq, double fs = 96000.0)
    {
        FilterUnit f;
        f.setModel(model);
        const float g = prewarp(cutoff, (float) fs);
        double inE = 0.0, outE = 0.0;
        const int n = (int) fs;
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.1f * (float) std::sin(2.0 * 3.141592653589793 * freq * i / fs);
            const float y = f.process(x, g, res, 0.0f, type, slope);
            if (i > n / 2)
            {
                inE += (double) x * x;
                outE += (double) y * y;
            }
        }
        return 10.0 * std::log10(outE / inE);
    }
}

ZS_TEST(filter_responses_all_models)
{
    for (FilterModel m : { FilterModel::Ladder, FilterModel::Ota, FilterModel::Sem, FilterModel::K35 })
    {
        const FilterSlope slope = m == FilterModel::K35 ? FilterSlope::Pole2 : FilterSlope::Pole4;
        const double pass = sineGainDb(m, FilterType::LowPass, slope, 1000.0f, 0.0f, 100.0);
        const double stop = sineGainDb(m, FilterType::LowPass, slope, 1000.0f, 0.0f, 8000.0);
        const double hpPass = sineGainDb(m, FilterType::HighPass, slope, 1000.0f, 0.0f, 8000.0);
        const double hpStop = sineGainDb(m, FilterType::HighPass, slope, 1000.0f, 0.0f, 100.0);
        std::printf("    model %d: LP pass %.1f stop %.1f | HP pass %.1f stop %.1f dB\n", (int) m, pass, stop, hpPass, hpStop);
        ZS_CHECK_GT(pass, -4.0);
        ZS_CHECK_LT(stop, slope == FilterSlope::Pole4 ? -45.0 : -25.0);
        ZS_CHECK_GT(hpPass, -4.0);
        // The K35 high-pass is a resonant 6 dB/oct topology (as on the original
        // semi-modular design): one decade below cutoff is ~-20 dB.
        ZS_CHECK_LT(hpStop, m == FilterModel::K35 ? -17.0 : -45.0);
    }
}

ZS_TEST(filter_models_sound_different)
{
    // Same settings, resonant: the four models must not produce near-identical output.
    std::vector<std::vector<float>> outs;
    for (FilterModel m : { FilterModel::Ladder, FilterModel::Ota, FilterModel::Sem, FilterModel::K35 })
    {
        FilterUnit f;
        f.setModel(m);
        Oscillator osc;
        osc.setWave(OscWave::Saw);
        std::vector<float> y(48000);
        for (auto& v : y)
            v = f.process(osc.process(110.0f / 96000.0f, -1.0f) * 0.5f, prewarp(800.0f, 96000.0f), 0.7f, 0.3f, FilterType::LowPass, FilterSlope::Pole2);
        outs.push_back(y);
    }
    for (size_t a = 0; a < outs.size(); ++a)
        for (size_t b = a + 1; b < outs.size(); ++b)
        {
            double diff = 0.0, ref = 0.0;
            for (size_t i = 24000; i < 48000; ++i)
            {
                diff += (outs[a][i] - outs[b][i]) * (outs[a][i] - outs[b][i]);
                ref += outs[a][i] * outs[a][i];
            }
            ZS_CHECK_GT(10.0 * std::log10(diff / ref), -20.0);
        }
}

ZS_TEST(filter_stability_under_abuse)
{
    Random rng(77);
    for (int m = 0; m < 4; ++m)
        for (int t = 0; t < 4; ++t)
            for (int s = 0; s < 2; ++s)
            {
                FilterUnit f;
                f.setModel((FilterModel) m);
                float firstHalf = 0.0f, secondHalf = 0.0f;
                bool finite = true;
                const int n = 192000;
                for (int i = 0; i < n; ++i)
                {
                    // Hot noise (+12 dB), cutoff swept at audio rate across the whole
                    // range, maximum resonance and drive.
                    const float cutoff = 20.0f * std::pow(1000.0f, 0.5f + 0.5f * std::sin(i * 0.013f));
                    const float y = f.process(rng.bipolar() * 4.0f, prewarp(cutoff, 96000.0f), 1.0f, 1.0f, (FilterType) t, (FilterSlope) s);
                    finite = finite && std::isfinite(y);
                    (i < n / 2 ? firstHalf : secondHalf) = std::max(i < n / 2 ? firstHalf : secondHalf, std::abs(y));
                }
                ZS_CHECK(finite);
                // Bounded: no growth over time (a hot input legitimately passes
                // through the HP/BP responses, so absolute level is not the test).
                ZS_CHECK_LT(secondHalf, firstHalf * 1.5f + 1.0f);
                ZS_CHECK_LT(secondHalf, 500.0f);
            }
}

ZS_TEST(filter_self_oscillation)
{
    for (FilterModel m : { FilterModel::Ladder, FilterModel::Ota, FilterModel::K35 })
    {
        FilterUnit f;
        f.setModel(m);
        const double fs = 96000.0;
        std::vector<float> y(65536);
        for (size_t i = 0; i < y.size(); ++i)
            y[i] = f.process(i < 10 ? 0.5f : 0.0f, prewarp(1000.0f, (float) fs), 1.0f, 0.0f, FilterType::LowPass, FilterSlope::Pole4);
        const double level = rmsOf(y, 40000);
        const double freq = dominantFrequency(y.data() + 32768, 32768, fs);
        std::printf("    model %d self-osc: rms %.3f at %.0f Hz\n", (int) m, level, freq);
        ZS_CHECK_GT(level, 0.05);
        ZS_CHECK_NEAR(freq, 1000.0, 120.0);
    }
}

// =============================================================== envelopes ====

ZS_TEST(envelope_timing_and_levels)
{
    const float fs = 48000.0f;
    EnvelopeCoeffs c;
    c.set(0.010f, 0.100f, 0.5f, 0.200f, fs);
    Envelope e;
    e.noteOn();
    int attackSamples = 0;
    while (e.getStage() == Envelope::Stage::Attack && attackSamples < 48000)
    {
        e.process(c);
        ++attackSamples;
    }
    ZS_CHECK_NEAR(attackSamples / fs, 0.010, 0.0005);

    // After the decay time the level is within 0.1 % of the way to sustain.
    for (int i = 0; i < (int) (0.1f * fs); ++i)
        e.process(c);
    ZS_CHECK_NEAR(e.getLevel(), 0.5, 0.5 * 0.002);

    e.noteOff();
    const float start = e.getLevel();
    for (int i = 0; i < (int) (0.2f * fs); ++i)
        e.process(c);
    ZS_CHECK_NEAR(20.0 * std::log10(e.getLevel() / start), -60.0, 1.0);
}

ZS_TEST(envelope_retrigger_has_no_jump)
{
    EnvelopeCoeffs c;
    c.set(0.05f, 0.1f, 0.6f, 0.5f, 48000.0f);
    Envelope e;
    e.noteOn();
    for (int i = 0; i < 6000; ++i)
        e.process(c);
    e.noteOff();
    for (int i = 0; i < 2000; ++i)
        e.process(c);
    const float before = e.getLevel();
    e.noteOn();
    const float after = e.process(c);
    ZS_CHECK_LT(std::abs(after - before), 0.01);
}

// ================================================================ voices ====

ZS_TEST(voice_allocation_and_stealing)
{
    VoiceManager vm;
    vm.prepare(96000.0f);
    VoiceSettings s;
    s.polyLimit = 4;
    vm.setSettings(s);
    VoiceShared shared;
    float l[32], r[32];
    for (int n = 0; n < 6; ++n)
    {
        vm.noteOn(60 + n, 100);
        vm.render(l, r, 32, shared);
    }
    ZS_CHECK(vm.activeVoiceCount() == 4);
    // The four newest notes must be the ones sounding (after steal fades complete).
    for (int b = 0; b < 100; ++b)
        vm.render(l, r, 32, shared);
    int found = 0;
    for (int i = 0; i < kMaxVoices; ++i)
        if (vm.voice(i).isActive() && vm.voice(i).getNote() >= 62)
            ++found;
    ZS_CHECK(found == 4);

    for (int n = 0; n < 6; ++n)
        vm.noteOff(60 + n);
    for (int b = 0; b < 4000; ++b)
        vm.render(l, r, 32, shared);
    ZS_CHECK(vm.activeVoiceCount() == 0);
}

ZS_TEST(voice_note_off_correctness_overlapping_and_repeated)
{
    VoiceManager vm;
    vm.prepare(96000.0f);
    vm.setSettings(VoiceSettings {});
    VoiceShared shared;
    float l[32], r[32];
    vm.noteOn(60, 100);
    vm.noteOn(64, 100);
    vm.noteOn(60, 90); // repeated note: replaces the first voice of note 60
    vm.render(l, r, 32, shared);
    vm.noteOff(60);
    int held = 0;
    for (int i = 0; i < kMaxVoices; ++i)
        if (vm.voice(i).isActive() && vm.voice(i).keyHeld)
            ++held;
    ZS_CHECK(held == 1); // only 64 still held
    vm.noteOff(64);
    for (int b = 0; b < 4000; ++b)
        vm.render(l, r, 32, shared);
    ZS_CHECK(vm.activeVoiceCount() == 0);
}

ZS_TEST(sustain_pedal)
{
    VoiceManager vm;
    vm.prepare(96000.0f);
    vm.setSettings(VoiceSettings {});
    VoiceShared shared;
    float l[32], r[32];
    vm.noteOn(60, 100);
    vm.setSustain(true);
    vm.noteOff(60);
    for (int b = 0; b < 3000; ++b)
        vm.render(l, r, 32, shared);
    ZS_CHECK(vm.activeVoiceCount() == 1);
    ZS_CHECK(! vm.voice(0).isReleasing() || ! vm.voice(0).isActive() ? vm.activeVoiceCount() == 1 : true);
    vm.setSustain(false);
    for (int b = 0; b < 6000; ++b)
        vm.render(l, r, 32, shared);
    ZS_CHECK(vm.activeVoiceCount() == 0);
}

ZS_TEST(unison_uses_stacked_voices)
{
    VoiceManager vm;
    vm.prepare(96000.0f);
    VoiceSettings s;
    s.unison = 5;
    s.polyLimit = 16;
    vm.setSettings(s);
    VoiceShared shared;
    float l[32], r[32];
    vm.noteOn(60, 100);
    vm.noteOn(67, 100);
    vm.render(l, r, 32, shared);
    ZS_CHECK(vm.activeVoiceCount() == 10);
}

ZS_TEST(mono_priority_and_legato)
{
    VoiceManager vm;
    vm.prepare(96000.0f);
    VoiceSettings s;
    s.mode = VoiceMode::Legato;
    s.priority = NotePriority::Low;
    vm.setSettings(s);
    VoiceShared shared;
    float l[32], r[32];
    vm.noteOn(60, 100);
    vm.noteOn(55, 100);
    vm.noteOn(64, 100);
    vm.render(l, r, 32, shared);
    ZS_CHECK(vm.activeVoiceCount() == 1);
    ZS_CHECK(vm.voice(0).getNote() == 55);
    vm.noteOff(55);
    ZS_CHECK(vm.voice(0).getNote() == 60);
    vm.noteOff(60);
    vm.noteOff(64);
    for (int b = 0; b < 4000; ++b)
        vm.render(l, r, 32, shared);
    ZS_CHECK(vm.activeVoiceCount() == 0);
}

// ================================================================= engine ====

namespace
{
    void setPlainSine(EngineRig& rig)
    {
        rig.set(P::Osc1Wave, (float) OscWave::Sine);
        rig.set(P::FltCutoff, 20000.0f);
        rig.set(P::FltKey, 0.0f);
        rig.set(P::Env1A, 0.005f);
        rig.set(P::Env1S, 1.0f);
        rig.set(P::Env1R, 0.05f);
        rig.set(P::Env1Vel, 0.0f);
    }
}

ZS_TEST(engine_render_known_midi)
{
    EngineRig rig;
    rig.noteOn(0.05, 60);
    rig.noteOn(0.05, 64);
    rig.noteOn(0.05, 67);
    rig.noteOff(1.0, 60);
    rig.noteOff(1.0, 64);
    rig.noteOff(1.0, 67);
    std::vector<float> right;
    const auto left = rig.render(2.0, &right);
    ZS_CHECK(allFinite(left) && allFinite(right));
    ZS_CHECK(peakOf(left, 0, 2000) == 0.0f);                // silent before the first note
    ZS_CHECK_GT(rmsOf(left, 4800, 48000), 0.01);           // sound while held
    ZS_CHECK_LT(peakOf(left, 80000), 1.0e-3);               // silent after release
    ZS_CHECK_LT(peakOf(left), 1.0);
    std::printf("    chord peak %.3f, rms %.3f\n", peakOf(left), rmsOf(left, 4800, 48000));
}

ZS_TEST(engine_no_discontinuities_on_note_events)
{
    EngineRig rig;
    setPlainSine(rig);
    for (int i = 0; i < 8; ++i)
    {
        rig.noteOn(0.1 + i * 0.1, 57 + i);
        rig.noteOff(0.15 + i * 0.1, 57 + i);
    }
    const auto x = rig.render(1.2);
    // A 220-440 Hz sine of amplitude < 1 moves < 0.06 per sample at 48 kHz;
    // clicks would show up as much larger steps.
    std::printf("    max step %.4f, peak %.3f\n", maxStep(x), peakOf(x));
    ZS_CHECK_LT(maxStep(x), 0.08);
}

ZS_TEST(engine_pitch_accuracy_and_bend)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        EngineRig rig(fs);
        setPlainSine(rig);
        rig.noteOn(0.0, 69);
        auto x = rig.render(1.5);
        const double f = dominantFrequency(x.data() + (size_t) (fs * 0.3), 32768, fs);
        std::printf("    fs %.0f: A4 measured %.2f Hz\n", fs, f);
        ZS_CHECK_NEAR(f, 440.0, 0.5);

        rig.bend(0.0, 16383); // full up, default range 2 st
        rig.queue.clear();
        rig.bend(0.0, 16383);
        rig.renderedSamples = 0;
        x = rig.render(1.5);
        const double fb = dominantFrequency(x.data() + (size_t) (fs * 0.3), 32768, fs);
        ZS_CHECK_NEAR(fb, 440.0 * std::pow(2.0, 2.0 / 12.0), 1.0);
    }
}

ZS_TEST(engine_midi_channel_filter)
{
    EngineRig rig;
    rig.set(P::MidiChannel, 2.0f); // receive channel 2 only (index 1)
    rig.noteOn(0.0, 60, 100, 0);   // channel 1: ignored
    auto x = rig.render(0.3);
    ZS_CHECK(peakOf(x) == 0.0f);
    rig.noteOn(0.31, 60, 100, 1);  // channel 2: plays
    x = rig.render(0.3);
    ZS_CHECK_GT(peakOf(x), 0.01);
}

ZS_TEST(engine_sustain_pedal_cc64)
{
    EngineRig rig;
    rig.set(P::Env1R, 0.01f);
    rig.cc(0.0, 64, 127);
    rig.noteOn(0.01, 60);
    rig.noteOff(0.1, 60);
    rig.cc(0.5, 64, 0);
    const auto x = rig.render(1.0);
    ZS_CHECK_GT(rmsOf(x, 19200, 23000), 0.01); // held at 0.4-0.48 s by the pedal
    ZS_CHECK_LT(peakOf(x, 30000), 1.0e-3);     // released after the pedal lifts
}

ZS_TEST(engine_parameter_smoothing)
{
    EngineRig rig;
    setPlainSine(rig);
    rig.noteOn(0.0, 69);
    rig.render(0.2);
    rig.set(P::AmpLevel, 0.1f); // abrupt GUI change
    const auto x = rig.render(0.1);
    // Smoothing spreads the change over ~15 ms instead of a step.
    ZS_CHECK_LT(maxStep(x), 0.08);
    ZS_CHECK_LT(rmsOf(x, 3000), rmsOf(x, 0, 200) * 0.3);
}

ZS_TEST(engine_all_sample_rates_and_block_sizes)
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int block : { 1, 17, 64, 512, 2048 })
        {
            EngineRig rig(fs, block);
            rig.set(P::DelayOn, 1.0f);
            rig.set(P::ReverbOn, 1.0f);
            rig.noteOn(0.0, 48);
            rig.noteOn(0.0, 55);
            rig.noteOff(0.3, 48);
            rig.noteOff(0.3, 55);
            const auto x = rig.render(0.5);
            ZS_CHECK(allFinite(x));
            ZS_CHECK_GT(rmsOf(x), 0.005);
        }
}

ZS_TEST(engine_extreme_parameters_stay_finite)
{
    Random rng(1234);
    for (int trial = 0; trial < 40; ++trial)
    {
        EngineRig rig(48000.0, 128);
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& d = paramDef(i);
            const float r = rng.unipolar();
            float v = r < 0.33f ? d.min : (r < 0.66f ? d.max : d.min + (d.max - d.min) * rng.unipolar());
            if (d.type != ParamType::Float)
                v = std::round(v);
            rig.set(i, v);
        }
        rig.set(P::MasterVolume, 0.0f);
        rig.set(P::Polyphony, 32.0f);
        for (int n = 0; n < 6; ++n)
            rig.noteOn(0.01 * n, 24 + n * 15, 127);
        std::vector<float> right;
        const auto x = rig.render(0.6, &right);
        ZS_CHECK(allFinite(x) && allFinite(right));
        ZS_CHECK_LT(peakOf(x), 1.0001); // limiter on by default... unless randomised off
        if (rig.params[P::Limiter] < 0.5f)
            continue;
    }
}

ZS_TEST(engine_effects_stability_max_feedback)
{
    EngineRig rig;
    for (int p : { P::DriveOn, P::ChorusOn, P::EnsOn, P::PhaserOn, P::FlangerOn, P::DelayOn, P::ReverbOn })
        rig.set(p, 1.0f);
    rig.set(P::PhaserFb, 0.95f);
    rig.set(P::FlangerFb, 0.95f);
    rig.set(P::DelayFb, 0.95f);
    rig.set(P::ReverbSize, 1.0f);
    rig.set(P::ReverbDamp, 0.0f);
    rig.set(P::DriveAmount, 1.0f);
    rig.set(P::Limiter, 0.0f);
    rig.noteOn(0.0, 60, 127);
    rig.noteOff(0.2, 60);
    std::vector<float> right;
    const auto x = rig.render(8.0, &right);
    ZS_CHECK(allFinite(x) && allFinite(right));
    ZS_CHECK_LT(peakOf(x), 4.0);
    std::printf("    peak with all FX at max feedback: %.2f\n", peakOf(x));
}

ZS_TEST(compressor_models_reduce_and_recover)
{
    const float fs = 48000.0f;
    for (int m = 0; m < (int) CompModel::Count; ++m)
    {
        OutputCompressor comp;
        comp.prepare(fs);
        CompSettings s;
        s.model = (CompModel) m;
        s.thresholdDb = -24.0f;
        s.ratioIndex = 3;      // 4:1
        s.attackMs = 1.0f;
        s.releaseMs = 200.0f;
        const int n = (int) fs * 2;
        std::vector<float> l((size_t) n), r((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = i < n / 2 ? 0.5f * std::sin(kTwoPi * 330.0f * (float) i / fs) : 0.0f;
        float grLoud = 0.0f;
        for (int i = 0; i < n; i += 256)
        {
            comp.process(l.data() + i, r.data() + i, std::min(256, n - i), s);
            const float gr = comp.takeGainReduction();
            if (i > n / 4 && i < n / 2)
                grLoud = std::max(grLoud, gr);
        }
        ZS_CHECK(allFinite(l));
        // -6 dBFS against a -24 dB threshold: clear, but not absurd, reduction.
        ZS_CHECK_GT(grLoud, 4.0f);
        ZS_CHECK_LT(grLoud, 24.0f);
        ZS_CHECK_LT(rmsOf(l, (size_t) n / 4, (size_t) n / 2), 0.5 / std::sqrt(2.0) * 0.7);
        // After 1 s of silence the gain has recovered (feed a quiet probe). The
        // LA-2A's optical cell remembers heavy compression for several seconds,
        // as the real one does: give it 4 s more.
        if ((CompModel) m == CompModel::LA2A)
        {
            std::vector<float> zl((size_t) fs * 4, 0.0f), zr((size_t) fs * 4, 0.0f);
            for (size_t i = 0; i < zl.size(); i += 256)
                comp.process(zl.data() + i, zr.data() + i, 256, s);
            comp.takeGainReduction();
        }
        std::vector<float> pl(4096), pr(4096);
        for (size_t i = 0; i < pl.size(); ++i)
            pl[i] = pr[i] = 0.001f * std::sin(kTwoPi * 330.0f * (float) i / fs);
        comp.process(pl.data(), pr.data(), (int) pl.size(), s);
        ZS_CHECK_LT(comp.takeGainReduction(), 3.0f);
    }
    // Mix 0 passes the dry signal unchanged.
    OutputCompressor comp;
    comp.prepare(fs);
    CompSettings s;
    s.mix = 0.0f;
    s.thresholdDb = -40.0f;
    std::vector<float> l(512), r(512);
    for (size_t i = 0; i < l.size(); ++i)
        l[i] = r[i] = 0.7f * std::sin((float) i * 0.1f);
    const auto ref = l;
    comp.process(l.data(), r.data(), 512, s);
    for (size_t i = 0; i < l.size(); ++i)
        ZS_CHECK_NEAR(l[i], ref[i], 1e-6);
}

ZS_TEST(delay_types_stable_and_decaying)
{
    // Every delay character at maximum feedback and wear: bounded, finite, and
    // (below runaway) the repeats die away once the input stops.
    const float fs = 48000.0f;
    for (int t = 0; t < (int) DelayType::Count; ++t)
        for (float fb : { 0.95f, 0.6f })
        {
            StereoDelay d;
            d.prepare(fs);
            DelaySettings s;
            s.type = (DelayType) t;
            s.seconds = 0.12f;
            s.feedback = fb;
            s.mix = 1.0f;
            s.wow = s.flutter = s.age = 1.0f;
            s.spread = 1.0f;
            const int n = (int) (fs * 6.0f);
            std::vector<float> l((size_t) n, 0.0f), r((size_t) n, 0.0f);
            for (int i = 0; i < 4800; ++i)
                l[(size_t) i] = r[(size_t) i] = 0.5f * std::sin(kTwoPi * 330.0f * (float) i / fs);
            for (int i = 0; i < n; i += 128)
                d.process(l.data() + i, r.data() + i, 128, s);
            ZS_CHECK(allFinite(l) && allFinite(r));
            ZS_CHECK_LT(peakOf(l), 2.0);
            if (fb < 0.7f)
                ZS_CHECK_LT(rmsOf(l, (size_t) n - 24000), 0.01); // repeats have died away
        }
}

ZS_TEST(reverb_types_stable_and_decaying)
{
    const float fs = 48000.0f;
    for (int t = 0; t < (int) ReverbType::Count; ++t)
    {
        // Worst case: longest decay, no damping, maximum modulation and diffusion.
        Reverb rv;
        rv.prepare(fs);
        ReverbSettings s;
        s.type = (ReverbType) t;
        s.size = 1.0f;
        s.decay = 1.0f;
        s.damping = 0.0f;
        s.diffusion = 1.0f;
        s.modulation = 1.0f;
        s.mix = 1.0f;
        s.width = 1.0f;
        const int n = (int) (fs * 8.0f);
        std::vector<float> l((size_t) n, 0.0f), r((size_t) n, 0.0f);
        for (int i = 0; i < 2400; ++i)
            l[(size_t) i] = r[(size_t) i] = (i % 97 < 48 ? 0.5f : -0.5f);
        for (int i = 0; i < n; i += 128)
            rv.process(l.data() + i, r.data() + i, 128, s);
        ZS_CHECK(allFinite(l) && allFinite(r));
        ZS_CHECK_LT(peakOf(l), 2.0);

        // Normal settings: the tail decays well below the onset within 8 s.
        Reverb rv2;
        rv2.prepare(fs);
        ReverbSettings s2;
        s2.type = (ReverbType) t;
        s2.mix = 1.0f;
        std::vector<float> a((size_t) n, 0.0f), b((size_t) n, 0.0f);
        for (int i = 0; i < 2400; ++i)
            a[(size_t) i] = b[(size_t) i] = (i % 97 < 48 ? 0.5f : -0.5f);
        for (int i = 0; i < n; i += 128)
            rv2.process(a.data() + i, b.data() + i, 128, s2);
        const double early = rmsOf(a, 2400, 24000), late = rmsOf(a, (size_t) n - 24000);
        ZS_CHECK_GT(early, 1.0e-3);
        ZS_CHECK_LT(20.0 * std::log10((late + 1e-12) / early), -40.0);
    }
}

ZS_TEST(engine_high_polyphony_and_cpu)
{
    EngineRig rig(48000.0, 256);
    rig.set(P::Polyphony, 32.0f);
    rig.set(P::Unison, 1.0f);
    rig.set(P::UnisonVoices, 2.0f);
    rig.set(P::Osc2On, 1.0f);
    rig.set(P::Osc3On, 1.0f);
    rig.set(P::Engine, (float) EngineMode::Hybrid);
    rig.set(P::FltRes, 0.6f);
    rig.set(P::ChorusOn, 1.0f);
    rig.set(P::DelayOn, 1.0f);
    rig.set(P::ReverbOn, 1.0f);
    for (int n = 0; n < 16; ++n)
        rig.noteOn(0.0, 36 + n * 3);
    Stopwatch sw;
    const double seconds = 4.0;
    const auto x = rig.render(seconds);
    const double t = sw.seconds();
    std::printf("    32 voices (16 notes x2 unison, 3 osc + 6-op FM, 2x OS, FX): %.1f%% of one core\n", 100.0 * t / seconds);
    ZS_CHECK(rig.engine.getActiveVoices() == 32);
    ZS_CHECK(allFinite(x));
    ZS_CHECK_LT(t / seconds, 1.0); // must be real-time capable on one core
}

ZS_TEST(engine_dense_midi_no_stuck_notes)
{
    EngineRig rig(48000.0, 64);
    rig.set(P::Polyphony, 8.0f);
    rig.set(P::Env1R, 0.01f);
    Random rng(5);
    for (int i = 0; i < 2000; ++i)
    {
        const int note = 30 + (int) (rng.unipolar() * 60);
        const double t = rng.unipolar() * 2.0;
        rig.noteOn(t, note);
        rig.noteOff(t + rng.unipolar() * 0.05, note);
    }
    rig.render(2.2);
    const auto tail = rig.render(0.5);
    ZS_CHECK(rig.engine.getActiveVoices() == 0);
    ZS_CHECK_LT(peakOf(tail, 12000), 1.0e-3);
}

ZS_TEST(engine_spectrum_of_saw)
{
    EngineRig rig;
    rig.set(P::FltCutoff, 20000.0f);
    rig.set(P::FltKey, 0.0f);
    rig.set(P::Env1S, 1.0f);
    rig.noteOn(0.0, 45); // 110 Hz
    const auto x = rig.render(2.0);
    const auto mag = magnitudeSpectrum(x.data() + 24000, 65536);
    const double binHz = 48000.0 / 65536.0;
    auto at = [&](double f) { const size_t b = (size_t) std::lround(f / binHz); double m = 0; for (size_t i = b - 2; i <= b + 2; ++i) m = std::max(m, mag[i]); return m; };
    // Saw harmonics fall as 1/n: the 2nd is ~6 dB below the 1st, the 4th ~12 dB.
    const double h1 = at(110.0), h2 = at(220.0), h4 = at(440.0);
    ZS_CHECK_NEAR(20.0 * std::log10(h2 / h1), -6.0, 1.5);
    ZS_CHECK_NEAR(20.0 * std::log10(h4 / h1), -12.0, 2.0);
    ZS_CHECK_LT(inharmonicRatioDb(x.data() + 24000, 65536, 48000.0, 110.0, 18000.0), -40.0);
}

// ===================================================================== FM ====

ZS_TEST(fm_algorithms_respect_processing_order)
{
    for (int a = 0; a < kFmAlgorithmCount; ++a)
    {
        const auto& alg = fmAlgorithm(a);
        ZS_CHECK(alg.carriers != 0);
        for (int op = 0; op < 6; ++op)
            for (int m = 0; m <= op; ++m)
                ZS_CHECK(((alg.modulators[op] >> m) & 1u) == 0); // modulators must have higher index
    }
}

ZS_TEST(fm_engine_produces_sidebands)
{
    EngineRig rig;
    rig.set(P::Engine, (float) EngineMode::FM);
    rig.set(P::FltCutoff, 20000.0f);
    rig.set(P::FltKey, 0.0f);
    rig.set(P::FmAlgo, 2.0f); // three pairs
    rig.set(P::FmDepth, 0.0f);
    rig.noteOn(0.0, 57);
    const auto plain = rig.render(1.0);
    EngineRig rig2;
    rig2.set(P::Engine, (float) EngineMode::FM);
    rig2.set(P::FltCutoff, 20000.0f);
    rig2.set(P::FltKey, 0.0f);
    rig2.set(P::FmAlgo, 2.0f);
    rig2.set(P::FmDepth, 0.6f);
    rig2.noteOn(0.0, 57);
    const auto rich = rig2.render(1.0);
    ZS_CHECK_GT(rmsOf(plain, 4800), 0.01);
    // Brightness: energy above 1 kHz rises with modulation depth.
    auto highRatio = [](const std::vector<float>& x) {
        const auto mag = magnitudeSpectrum(x.data() + 8192, 32768);
        double hi = 0, all = 0;
        for (size_t i = 1; i < mag.size(); ++i) { all += mag[i] * mag[i]; if (i * 48000.0 / 32768 > 1000) hi += mag[i] * mag[i]; }
        return hi / all;
    };
    std::printf("    FM HF energy ratio: depth 0 %.4f, depth 0.6 %.4f\n", highRatio(plain), highRatio(rich));
    ZS_CHECK_GT(highRatio(rich), highRatio(plain) * 20.0);
}

// ====================================================== arp and sequencer ====

ZS_TEST(fm_dx_mode_levels_and_envelopes)
{
    // DX level taper: 0.75 dB per step of 99; clean taper is level squared.
    ZS_CHECK_NEAR(20.0 * std::log10(FmVoice::levelToGain(91.0f / 99.0f, true)), -6.0, 0.05);
    ZS_CHECK_NEAR(FmVoice::levelToGain(0.5f, false), 0.25, 1e-6);
    ZS_CHECK(FmVoice::levelToGain(0.0f, true) == 0.0f);

    // A DX decay falls at a constant rate in dB and stops exactly on sustain.
    const float fs = 48000.0f;
    EnvelopeCoeffs c;
    c.set(0.001f, 1.0f, 0.1f, 0.5f, fs);
    DxEnvRates d;
    d.set(0.001f, 1.0f, 0.5f, fs);
    OpEnvelope env;
    env.noteOn();
    const float a = std::exp(d.attackLog), dm = std::exp(d.decayLog), rm = std::exp(d.releaseLog);
    std::vector<float> lv;
    for (int i = 0; i < (int) fs; ++i)
        lv.push_back(env.process(c, true, a, dm, rm));
    const auto dbAt = [&](double t) { return 20.0 * std::log10(lv[(size_t) (t * fs)]); };
    // 60 dB per second -> -6 dB at ~0.1 s, -12 dB at ~0.2 s: a straight line in dB.
    ZS_CHECK_NEAR(dbAt(0.2) - dbAt(0.1), -6.0, 0.3);
    ZS_CHECK_NEAR(dbAt(0.3) - dbAt(0.2), -6.0, 0.3);
    ZS_CHECK_NEAR(lv.back(), 0.1, 1e-6); // reached sustain (-20 dB) after ~0.33 s and holds
}

ZS_TEST(fm_operator_velocity_and_key_scaling)
{
    // Renders operator 1 alone (organ algorithm) and measures its level.
    auto level = [](float velSens, float velocity, float kls, float pitch) {
        FmShared s;
        s.algorithm = 10; // six carriers
        for (int i = 0; i < 6; ++i)
            s.level[i] = i == 0 ? 1.0f : 0.0f;
        s.velSens[0] = velSens;
        s.levelScale[0] = kls;
        for (auto& e : s.env)
            e.set(0.0005f, 10.0f, 1.0f, 1.0f, 48000.0f);
        FmVoice v;
        v.setFastRelease(48000.0f);
        v.noteOn(velocity, true);
        v.prepareBlock(s, 0.0f, 0.0f, pitch);
        double sum = 0.0;
        for (int i = 0; i < 4800; ++i)
        {
            const float x = v.process(440.0f / 48000.0f);
            if (i >= 2400)
                sum += (double) x * x;
        }
        return 10.0 * std::log10(sum / 2400.0);
    };
    const double ref = level(0.0f, 0.2f, 0.0f, 60.0f);
    ZS_CHECK_NEAR(level(1.0f, 0.5f, 0.0f, 60.0f) - ref, -15.0, 0.5);  // 30 dB range, half velocity
    ZS_CHECK_NEAR(level(1.0f, 1.0f, 0.0f, 60.0f) - ref, 0.0, 0.2);    // full velocity = full level
    ZS_CHECK_NEAR(level(0.0f, 1.0f, 1.0f, 84.0f) - ref, -18.0, 0.5);  // two octaves up, 9 dB/oct
    ZS_CHECK_NEAR(level(0.0f, 1.0f, 1.0f, 48.0f) - ref, 0.0, 0.2);    // no scaling below C4
}

ZS_TEST(valve_stage_warms_without_level_jumps)
{
    // A hot -6 dBFS sine through every valve type: low and middle DRIVE only
    // warm (well under 1 % THD), audible distortion starts near the top, and
    // the level stays steady throughout.
    const float fs = 48000.0f;
    auto run = [&](int type, float drive, double& thdPercent, double& levelDb) {
        ValveStage valve;
        valve.prepare(fs);
        const int n = 48000;
        std::vector<float> l((size_t) n), r((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = 0.5f * std::sin(kTwoPi * 220.0f * (float) i / fs);
        const auto inRms = rmsOf(l, 0);
        for (int i = 0; i < n; i += 256)
            valve.process(l.data() + i, r.data() + i, std::min(256, n - i), (ValveType) type, drive, 0.5f, 0.3f, 0.5f, 1.0f);
        for (float x : l)
            ZS_CHECK(std::isfinite(x) && std::abs(x) < 1.5f);
        auto amp = [&](double f) {
            double c = 0.0, sn = 0.0;
            for (int i = 9600; i < 9600 + 24000; ++i)
            {
                c += l[(size_t) i] * std::cos(2.0 * 3.14159265358979 * f * i / fs);
                sn += l[(size_t) i] * std::sin(2.0 * 3.14159265358979 * f * i / fs);
            }
            return std::sqrt(c * c + sn * sn);
        };
        const double h1 = amp(220.0);
        double h = 0.0;
        for (int k = 2; k <= 5; ++k)
            h += amp(220.0 * k) * amp(220.0 * k);
        thdPercent = 100.0 * std::sqrt(h) / h1;
        levelDb = 20.0 * std::log10(rmsOf(l, 4800) / inRms);
    };
    for (int t = 0; t < (int) ValveType::Count; ++t)
    {
        double thd = 0.0, level = 0.0;
        run(t, 0.5f, thd, level);
        ZS_CHECK_LT(thd, 1.0);
        ZS_CHECK_GT(thd, 0.05); // it still colours
        ZS_CHECK_NEAR(level, 0.0, 1.5);
        run(t, 0.8f, thd, level);
        ZS_CHECK_LT(thd, 2.5);
        run(t, 1.0f, thd, level);
        ZS_CHECK_GT(thd, 10.0);
        ZS_CHECK_NEAR(level, 0.0, 6.0);
    }
}

ZS_TEST(arpeggiator_timing)
{
    Arpeggiator arp;
    arp.prepare(48000.0);
    Arpeggiator::Settings s;
    s.mode = ArpMode::Up;
    s.division = 13; // 1/16
    s.bpm = 120.0;   // 1/16 = 0.125 s = 6000 samples
    s.gate = 0.5f;
    arp.setSettings(s);
    arp.noteOn(60, 100);
    arp.noteOn(64, 100);
    arp.noteOn(67, 100);

    std::vector<std::pair<long long, int>> ons;
    GeneratedNoteList out;
    long long t = 0;
    while (t < 48000)
    {
        out.clear();
        arp.fireDueEvents(out);
        for (int i = 0; i < out.count; ++i)
            if (out.events[i].velocity > 0)
                ons.push_back({ t, out.events[i].note });
        const int step = arp.samplesToNextEvent();
        arp.advance(step);
        t += step;
    }
    ZS_CHECK(ons.size() == 8);
    const int expected[3] = { 60, 64, 67 };
    for (size_t i = 0; i < ons.size(); ++i)
    {
        ZS_CHECK(ons[i].first == (long long) i * 6000);
        ZS_CHECK(ons[i].second == expected[i % 3]);
    }
}

ZS_TEST(sequencer_note_mode_pattern_and_slide)
{
    EngineRig rig;
    rig.set(P::SeqMode, (float) SeqMode::Note);
    rig.set(P::SeqLength, 4.0f);
    rig.set(P::SeqVal1, 0.0f);
    rig.set(P::SeqVal2, 7.0f / 12.0f);
    rig.set(P::SeqVal3, 12.0f / 12.0f);
    rig.set(P::SeqGate4, 0.0f);
    rig.set(P::SeqSlide2, 1.0f);
    rig.set(P::Osc1Wave, (float) OscWave::Sine);
    rig.set(P::FltCutoff, 20000.0f);
    rig.noteOn(0.0, 57);
    const auto x = rig.render(0.5);
    // 1/16 at 120 BPM = 0.125 s per step: step 1 = 220 Hz, step 2 = 7 st up.
    const double f1 = dominantFrequency(x.data() + 1000, 4096, 48000.0);
    const double f2 = dominantFrequency(x.data() + 6000 + 1500, 4096, 48000.0);
    ZS_CHECK_NEAR(f1, 220.0, 12.0);
    ZS_CHECK_NEAR(f2, 220.0 * std::pow(2.0, 7.0 / 12.0), 15.0);
    ZS_CHECK_LT(rmsOf(x, 18000 + 3000, 24000), 0.01); // step 4 is a rest
}

// ============================================================ resampling ====

ZS_TEST(halfband_response)
{
    auto gainAt = [](double fNorm) {
        HalfBandDecimator d;
        double e = 0.0;
        const int n = 20000;
        for (int i = 0; i < n; ++i)
        {
            const float a = (float) std::sin(2.0 * 3.141592653589793 * fNorm * (2 * i));
            const float b = (float) std::sin(2.0 * 3.141592653589793 * fNorm * (2 * i + 1));
            const float y = d.process(a, b);
            if (i > 1000) e += (double) y * y;
        }
        return 10.0 * std::log10(e / ((n - 1001) * 0.5));
    };
    // fNorm is relative to the oversampled rate.
    ZS_CHECK_NEAR(gainAt(0.1), 0.0, 0.1);   // 0.2 x base rate
    ZS_CHECK_NEAR(gainAt(0.2), 0.0, 0.5);   // 0.4 x base rate (19.2 kHz at 48 kHz)
    ZS_CHECK_LT(gainAt(0.30), -70.0);       // folds to 0.4 x base rate: rejected
    ZS_CHECK_LT(gainAt(0.40), -70.0);
}

int main(int argc, char** argv)
{
    return zstest::runAll(argc, argv);
}
