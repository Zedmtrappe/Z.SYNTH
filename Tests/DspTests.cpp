// DSP engine tests: deterministic, synthetic, JUCE-free.

#include "TestFramework.h"
#include "TestHelpers.h"

#include "DSP/Common/HalfBand.h"
#include "DSP/Envelopes/Envelope.h"
#include "DSP/Effects/Compressor.h"
#include "DSP/Effects/DriveAndOutput.h"
#include "DSP/Effects/SmoothBand.h"
#include "DSP/Effects/TimeEffects.h"
#include "DSP/Effects/OutputEq.h"
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
    for (int mi = 0; mi < (int) FilterModel::Count; ++mi)
    {
        const auto m = (FilterModel) mi;
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
    for (int mi = 0; mi < (int) FilterModel::Count; ++mi)
    {
        FilterUnit f;
        f.setModel((FilterModel) mi);
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
            if (10.0 * std::log10(diff / ref) <= -20.0)
                std::printf("    models %d and %d sound alike\n", (int) a, (int) b);
            ZS_CHECK_GT(10.0 * std::log10(diff / ref), -20.0);
        }
}

ZS_TEST(filter_stability_under_abuse)
{
    Random rng(77);
    for (int m = 0; m < (int) FilterModel::Count; ++m)
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
    for (FilterModel m : { FilterModel::Ladder, FilterModel::Ota, FilterModel::K35, FilterModel::Jp8000, FilterModel::Virus })
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

ZS_TEST(digital_filters_resonance_character)
{
    // The QS filter has no resonant peak, the Korg Digital a moderate one, and the
    // Roland TVF stops short of self-oscillation.
    auto peakDb = [](FilterModel m) {
        return sineGainDb(m, FilterType::LowPass, FilterSlope::Pole4, 1000.0f, 1.0f, 1000.0)
             - sineGainDb(m, FilterType::LowPass, FilterSlope::Pole4, 1000.0f, 1.0f, 100.0);
    };
    const double qs = peakDb(FilterModel::AlesisQs), korg = peakDb(FilterModel::KorgDigital), tvf = peakDb(FilterModel::RolandTvf);
    std::printf("    resonant peak at max RES: QS %.1f  Korg %.1f  TVF %.1f dB\n", qs, korg, tvf);
    ZS_CHECK_LT(qs, 1.0);
    ZS_CHECK_GT(korg, 6.0);
    ZS_CHECK_GT(tvf, 10.0);
    for (FilterModel m : { FilterModel::RolandTvf, FilterModel::KorgDigital, FilterModel::AlesisQs })
    {
        FilterUnit f;
        f.setModel(m);
        std::vector<float> y(65536);
        for (size_t i = 0; i < y.size(); ++i)
            y[i] = f.process(i < 10 ? 0.5f : 0.0f, prewarp(1000.0f, 96000.0f), 1.0f, 0.0f, FilterType::LowPass, FilterSlope::Pole4);
        ZS_CHECK_LT(rmsOf(y, 40000), 1.0e-4); // rings out: no self-oscillation
    }
}

// ============================================================ super saws ====

ZS_TEST(supersaw_level_detune_and_pitch)
{
    const float fs = 96000.0f, f0 = 220.0f;
    for (bool hyper : { false, true })
    {
        double minDb = 1e9, maxDb = -1e9;
        for (float mix : { 0.0f, 0.5f, 1.0f })
        {
            Random rng(5);
            SuperSaw ss;
            ss.reset(rng);
            ss.setup(hyper, 0.5f, mix);
            std::vector<float> y(96000);
            for (auto& v : y)
                v = ss.process(f0 / fs);
            ZS_CHECK(allFinite(y));
            const double db = 20.0 * std::log10(rmsOf(y, 9600));
            minDb = std::min(minDb, db);
            maxDb = std::max(maxDb, db);
        }
        std::printf("    %s: level across MIX %.1f .. %.1f dB\n", hyper ? "HyperSaw" : "SuperSaw", minDb, maxDb);
        ZS_CHECK_LT(maxDb - minDb, 4.0);

        // No detune: a plain saw at the right pitch. Full detune: the stack spreads
        // (the waveform beats, so short-term level varies much more).
        Random rng(9);
        SuperSaw tight, wide;
        tight.reset(rng);
        wide.reset(rng);
        tight.setup(hyper, 0.0f, 0.0f);
        wide.setup(hyper, 1.0f, 1.0f);
        std::vector<float> a(96000), b(96000);
        for (size_t i = 0; i < a.size(); ++i)
        {
            a[i] = tight.process(f0 / fs);
            b[i] = wide.process(f0 / fs);
        }
        ZS_CHECK_NEAR(dominantFrequency(a.data() + 16384, 65536, fs), f0, 6.0);
        double diff = 0.0, ref = 0.0;
        for (size_t i = 9600; i < a.size(); ++i)
        {
            diff += (a[i] - b[i]) * (a[i] - b[i]);
            ref += a[i] * a[i];
        }
        ZS_CHECK_GT(diff / ref, 0.3);
    }
}

// ============================================== ROM waves, sequence, vector ====

ZS_TEST(rom_waves_evolve_and_render)
{
    const auto& bank = WavetableBank::get();
    for (int r = 0; r < kNumRomWaves; ++r)
    {
        // The attack frame differs from the loop: each ROM wave has a "sample" attack.
        const auto ev = WavetableBank::romEvolution(r);
        const float* first = bank.rom(r).frame(0, 0);
        const float* loop = bank.rom(r).frame((int) ev.loopEnd, 0);
        double diff = 0.0;
        for (int i = 0; i < Wavetable::kSize; ++i)
            diff += (first[i] - loop[i]) * (first[i] - loop[i]);
        ZS_CHECK_GT(diff / Wavetable::kSize, 1.0e-3);

        EngineRig rig;
        rig.set(P::Osc1Wave, (float) ((int) OscWave::RomAhhChoir + r));
        rig.set(P::Env1S, 1.0f);
        rig.noteOn(0.0, 60);
        const auto y = rig.render(1.0);
        ZS_CHECK(allFinite(y));
        ZS_CHECK_GT(rmsOf(y, 4800), 0.01);
        ZS_CHECK_LT(peakOf(y), 1.0f);
    }
}

ZS_TEST(wave_sequence_steps_through_the_table)
{
    // Seq Harmonics: frame f is the fundamental plus harmonic f + 2. Four steps
    // spread through the table play frames 0, 8, 16, 24: harmonics 2, 10, 18, 26.
    EngineRig rig(48000.0, 256);
    rig.set(P::Osc1Wave, (float) OscWave::Wavetable);
    rig.set(P::WtTable, (float) WavetableId::SeqHarmonics);
    rig.set(P::WtSeqMode, (float) WaveSeqMode::Loop);
    rig.set(P::WtSeqSteps, 4.0f);
    rig.set(P::WtSeqTime, 0.5f);
    rig.set(P::WtSeqXfade, 0.0f);
    rig.set(P::FltCutoff, 20000.0f);
    rig.set(P::Env1S, 1.0f);
    rig.noteOn(0.0, 36); // 65.4 Hz
    const auto y = rig.render(2.0);
    const double f0 = 440.0 * std::pow(2.0, (36 - 69) / 12.0);
    const int harmonic[4] = { 2, 10, 18, 26 };
    for (int k = 0; k < 4; ++k)
    {
        const size_t start = (size_t) ((k * 0.5 + 0.1) * 48000.0);
        const double f = dominantFrequency(y.data() + start, 16384, 48000.0);
        std::printf("    step %d: %.0f Hz (expected %.0f)\n", k, f, f0 * harmonic[k]);
        ZS_CHECK_NEAR(f, f0 * harmonic[k], f0 * 0.6);
    }
}

ZS_TEST(vector_mixing_corners)
{
    // A = osc 1 (sine 8'), B = osc 2 (sine 4'): the joystick picks between them.
    auto dominant = [](float x, float y) {
        EngineRig rig(48000.0, 256);
        rig.set(P::Osc1Wave, (float) OscWave::Sine);
        rig.set(P::Osc2On, 1.0f);
        rig.set(P::Osc2Wave, (float) OscWave::Sine);
        rig.set(P::Osc2Octave, (float) OscOctave::Ft4);
        rig.set(P::VecOn, 1.0f);
        rig.set(P::VecX, x);
        rig.set(P::VecY, y);
        rig.set(P::Env1S, 1.0f);
        rig.noteOn(0.0, 57); // 220 Hz
        const auto out = rig.render(1.0);
        return dominantFrequency(out.data() + 16384, 16384, 48000.0);
    };
    ZS_CHECK_NEAR(dominant(-1.0f, 0.0f), 220.0, 8.0);
    ZS_CHECK_NEAR(dominant(0.0f, 1.0f), 440.0, 8.0);
    // C (osc 3, off) and D (FM part, Analog engine): silence.
    EngineRig rig;
    rig.set(P::VecOn, 1.0f);
    rig.set(P::VecX, 1.0f);
    rig.noteOn(0.0, 60);
    ZS_CHECK_LT(rmsOf(rig.render(0.5), 4800), 1.0e-4);
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
        // The safety limiter holds peaks below full scale, unless this trial
        // randomised it off.
        if (rig.params[P::Limiter] >= 0.5f)
            ZS_CHECK_LT(peakOf(x), 1.0001);
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

ZS_TEST(reverb_parametric_eq)
{
    // Flat settings are an exact bypass; each band does what it says.
    EqSettings flat;
    ZS_CHECK(flat.isFlat());
    for (double f : { 50.0, 500.0, 5000.0 })
        ZS_CHECK_NEAR(ParametricEq::magnitudeDb(flat, f, 48000.0), 0.0, 1e-6);
    EqSettings s;
    s.bell1Freq = 1000.0f;
    s.bell1Gain = 9.0f;
    s.bell1Q = 2.0f;
    ZS_CHECK_NEAR(ParametricEq::magnitudeDb(s, 1000.0, 48000.0), 9.0, 0.05);
    ZS_CHECK_LT(std::abs(ParametricEq::magnitudeDb(s, 100.0, 48000.0)), 0.5);
    EqSettings shelf;
    shelf.lowGain = -12.0f;
    shelf.lowFreq = 200.0f;
    shelf.highGain = 6.0f;
    shelf.highFreq = 4000.0f;
    ZS_CHECK_NEAR(ParametricEq::magnitudeDb(shelf, 30.0, 48000.0), -12.0, 0.6);
    ZS_CHECK_NEAR(ParametricEq::magnitudeDb(shelf, 18000.0, 48000.0), 6.0, 0.6);

    // The processed signal matches the analytic response.
    ParametricEq eq;
    eq.prepare(48000.0f);
    eq.setSettings(s);
    double in = 0.0, out = 0.0;
    for (int i = 0; i < 48000; ++i)
    {
        float l = std::sin(kTwoPi * 1000.0f * (float) i / 48000.0f), r = l;
        const float x = l;
        eq.process(l, r);
        if (i > 4800)
        {
            in += (double) x * x;
            out += (double) l * l;
        }
    }
    ZS_CHECK_NEAR(10.0 * std::log10(out / in), 9.0, 0.1);
}

ZS_TEST(output_parametric_eq)
{
    const float fs = 48000.0f;
    auto run = [&](const OutputEqSettings& s, float freq, std::vector<float>& l) {
        OutputEq eq;
        eq.prepare(fs);
        const int n = 24064;
        l.assign((size_t) n, 0.0f);
        std::vector<float> r((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = 0.1f * std::sin(kTwoPi * freq * (float) i / fs);
        for (int i = 0; i < n; i += 256)
            eq.process(l.data() + i, r.data() + i, 256, s);
        return 20.0 * std::log10(rmsOf(l, 9600) / (0.1 / std::sqrt(2.0)));
    };
    // Flat = dry: bit-exact bypass.
    OutputEqSettings flat;
    std::vector<float> l;
    run(flat, 1000.0f, l);
    for (int i = 0; i < 2000; ++i)
        ZS_CHECK(l[(size_t) i] == 0.1f * std::sin(kTwoPi * 1000.0f * (float) i / fs));
    // A bell lands where it should, matching the drawn response; Q sets its width.
    OutputEqSettings s;
    s.freq[2] = 1000.0f;
    s.gainDb[2] = 9.0f;
    ZS_CHECK_NEAR(run(s, 1000.0f, l), 9.0, 0.5);
    ZS_CHECK_NEAR(OutputEq::magnitudeDb(s, 1000.0, fs), 9.0, 0.1);
    s.q[2] = 0.5f;
    const double wide = OutputEq::magnitudeDb(s, 2000.0, fs);
    s.q[2] = 6.0f;
    const double narrow = OutputEq::magnitudeDb(s, 2000.0, fs);
    ZS_CHECK_GT(wide, narrow + 4.0);
    ZS_CHECK_LT(std::abs(run(s, 100.0f, l)), 0.5);
    // Cuts: 12 dB/oct, -3 dB at the corner.
    OutputEqSettings cuts;
    cuts.hpf = 200.0f;
    cuts.lpf = 5000.0f;
    ZS_CHECK_NEAR(run(cuts, 200.0f, l), -3.0, 0.6);
    ZS_CHECK_LT(run(cuts, 50.0f, l), -20.0);
    ZS_CHECK_LT(run(cuts, 20000.0f, l), -20.0);
    ZS_CHECK_NEAR(run(cuts, 1000.0f, l), 0.0, 0.5);
    // Output trim.
    OutputEqSettings trim;
    trim.outputDb = -6.0f;
    ZS_CHECK_NEAR(run(trim, 1000.0f, l), -6.0, 0.1);
}

ZS_TEST(smooth_band_tames_only_the_presence_band)
{
    const float fs = 48000.0f;
    auto levelAfter = [&](float freq, float amp, float amount, float& gr) {
        SmoothBand sm;
        sm.prepare(fs);
        const int n = 24064;
        std::vector<float> l((size_t) n), r((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = amp * std::sin(kTwoPi * freq * (float) i / fs);
        gr = 0.0f;
        for (int i = 0; i < n; i += 256)
        {
            sm.process(l.data() + i, r.data() + i, 256, amount);
            gr = std::max(gr, sm.takeGainReduction());
        }
        return 20.0 * std::log10(rmsOf(l, 9600) / (amp / std::sqrt(2.0)));
    };
    float gr = 0.0f;
    // AMOUNT 0 is an exact bypass.
    ZS_CHECK_NEAR(levelAfter(2200.0f, 0.5f, 0.0f, gr), 0.0, 1e-3); // bypass (the sine window itself measures 0.0003 dB off)
    ZS_CHECK(gr == 0.0f);
    // A loud 2.2 kHz tone is turned down; the same level at 200 Hz barely is.
    const double cut = levelAfter(2200.0f, 0.5f, 0.7f, gr);
    ZS_CHECK_LT(cut, -3.0);
    ZS_CHECK_GT(gr, 3.0f);
    ZS_CHECK_GT(levelAfter(200.0f, 0.5f, 0.7f, gr), -1.0);
    // Quiet material passes untouched.
    ZS_CHECK_NEAR(levelAfter(2200.0f, 0.005f, 0.7f, gr), 0.0, 0.05);
    // More AMOUNT, more reduction.
    float grLow = 0.0f, grHigh = 0.0f;
    levelAfter(2200.0f, 0.3f, 0.3f, grLow);
    levelAfter(2200.0f, 0.3f, 0.9f, grHigh);
    ZS_CHECK_GT(grHigh, grLow);
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
        ZS_CHECK_GT(thd, 0.02); // it still colours (the power pairs least: clean until pushed)
        ZS_CHECK_NEAR(level, 0.0, 1.5);
        run(t, 0.8f, thd, level);
        ZS_CHECK_LT(thd, 2.5);
        run(t, 1.0f, thd, level);
        ZS_CHECK_GT(thd, 10.0);
        ZS_CHECK_NEAR(level, 0.0, 6.0);
    }

    // The types have different signatures at 90 % DRIVE: triodes lead with
    // the 2nd harmonic or balance it, push-pull pairs are dominated by the 3rd.
    auto harmonics = [&](int type, double& h2, double& h3) {
        ValveStage valve;
        valve.prepare(fs);
        const int n = 48000;
        std::vector<float> l((size_t) n), r((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = 0.5f * std::sin(kTwoPi * 220.0f * (float) i / fs);
        for (int i = 0; i < n; i += 256)
            valve.process(l.data() + i, r.data() + i, std::min(256, n - i), (ValveType) type, 0.9f, 0.5f, 0.3f, 0.5f, 1.0f);
        auto amp = [&](double f) {
            double c = 0.0, sn = 0.0;
            for (int i = 9600; i < 9600 + 24000; ++i)
            {
                c += l[(size_t) i] * std::cos(2.0 * 3.14159265358979 * f * i / fs);
                sn += l[(size_t) i] * std::sin(2.0 * 3.14159265358979 * f * i / fs);
            }
            return std::sqrt(c * c + sn * sn);
        };
        h2 = 20.0 * std::log10(amp(440.0) / amp(220.0));
        h3 = 20.0 * std::log10(amp(660.0) / amp(220.0));
    };
    double h2[4], h3[4];
    for (int t = 0; t < 4; ++t)
    {
        harmonics(t, h2[t], h3[t]);
        std::printf("    valve %d at 90%%: H2 %.1f dB, H3 %.1f dB\n", t, h2[t], h3[t]);
    }
    ZS_CHECK_GT(h2[(int) ValveType::Preamp12AX7] - h3[(int) ValveType::Preamp12AX7], 3.0);
    ZS_CHECK_GT(h3[(int) ValveType::PushPullEL84] - h2[(int) ValveType::PushPullEL84], 15.0);
    ZS_CHECK_GT(h3[(int) ValveType::Power6L6] - h2[(int) ValveType::Power6L6], 15.0);
    ZS_CHECK_GT(h3[(int) ValveType::PushPullEL84], h3[(int) ValveType::Power6L6] + 2.0); // EL84 breaks up earlier
}

ZS_TEST(output_transformer_models)
{
    // Calibrated against each model's published character: low-frequency
    // distortion at +12 dBu (AMOUNT 50 %) near its target, rising steeply when
    // driven, while the midrange stays clean; nickel (Jensen) far cleaner than
    // steel; each has its own low-end corner.
    const float fs = 48000.0f;
    auto run = [&](XfmrType t, float amount, float amp, float f, double& thd, double& gainDb) {
        OutputTransformer x;
        x.prepare(fs);
        const int n = 96000;
        std::vector<float> l((size_t) n), r((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = amp * std::sin(kTwoPi * f * (float) i / fs);
        for (int i = 0; i < n; i += 256)
            x.process(l.data() + i, r.data() + i, std::min(256, n - i), t, amount);
        ZS_CHECK(allFinite(l));
        auto a = [&](double fr) {
            double c = 0.0, sn = 0.0;
            for (int i = 48000; i < 96000; ++i)
            {
                c += l[(size_t) i] * std::cos(2.0 * 3.14159265358979 * fr * i / fs);
                sn += l[(size_t) i] * std::sin(2.0 * 3.14159265358979 * fr * i / fs);
            }
            return std::sqrt(c * c + sn * sn) / 24000.0;
        };
        const double h1 = a(f);
        double h = 0.0;
        for (int k = 2; k <= 7; ++k)
            h += a(f * k) * a(f * k);
        thd = 100.0 * std::sqrt(h) / h1;
        gainDb = 20.0 * std::log10(h1 / amp);
    };
    for (int t = 0; t < (int) XfmrType::Count; ++t)
    {
        const auto& m = OutputTransformer::model((XfmrType) t);
        double nominal, driven, mid, g, lf;
        run((XfmrType) t, 0.5f, 0.5f, 40.0f, nominal, g);
        run((XfmrType) t, 1.0f, 0.5f, 40.0f, driven, g);
        run((XfmrType) t, 1.0f, 0.5f, 1000.0f, mid, g);
        double dummy;
        run((XfmrType) t, 0.0f, 0.05f, m.lfHz, dummy, lf);
        std::printf("    transformer %d: 40 Hz THD %.3f %% nominal, %.2f %% driven; 1 kHz driven %.4f %%; %.1f dB at %.0f Hz\n",
                    t, nominal, driven, mid, lf, m.lfHz);
        ZS_CHECK_NEAR(nominal, m.thd40, m.thd40 * 0.25 + 0.01);
        ZS_CHECK_GT(driven, nominal * 10.0);
        ZS_CHECK_LT(mid, 0.05);
        ZS_CHECK_NEAR(lf, -3.0, 1.0);
    }

    // +HOT: a usable drive range on a synth-level signal (-9 dBFS): clean at
    // 0, clearly saturated at 75 %, heavily at full; the Jensen stays clean.
    auto hotThdAt = [&](XfmrType t, float amount, float freq) {
        OutputTransformer x;
        x.prepare(fs);
        const int n = 96000;
        std::vector<float> l((size_t) n), r((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = 0.35f * std::sin(kTwoPi * freq * (float) i / fs);
        for (int i = 0; i < n; i += 256)
            x.process(l.data() + i, r.data() + i, 256, t, amount, true);
        ZS_CHECK(allFinite(l));
        auto a2 = [&](double fr) {
            double c = 0.0, sn = 0.0;
            for (int i = 48000; i < 96000; ++i)
            {
                c += l[(size_t) i] * std::cos(2.0 * 3.14159265358979 * fr * i / fs);
                sn += l[(size_t) i] * std::sin(2.0 * 3.14159265358979 * fr * i / fs);
            }
            return std::sqrt(c * c + sn * sn);
        };
        double h = 0.0;
        for (int k = 2; k <= 9; ++k)
            h += a2(freq * k) * a2(freq * k);
        return 100.0 * std::sqrt(h) / a2(freq);
    };
    auto hotThd = [&](XfmrType t, float amount) { return hotThdAt(t, amount, 110.0f); };
    for (int t = 0; t < (int) XfmrType::Count; ++t)
        std::printf("    transformer %d +HOT at 110 Hz: %.2f %% / %.2f %% / %.2f %% (0 / 75 / 100 %%)\n", t,
                    hotThd((XfmrType) t, 0.0f), hotThd((XfmrType) t, 0.75f), hotThd((XfmrType) t, 1.0f));
    ZS_CHECK_LT(hotThd(XfmrType::NeveLo1166, 0.0f), 0.2);
    ZS_CHECK_GT(hotThd(XfmrType::NeveLo1166, 0.75f), 1.5);
    ZS_CHECK_GT(hotThd(XfmrType::NeveLo1166, 1.0f), 15.0);
    // Tube-era UTC colours early; the API's op-amp stays clean until it clips.
    ZS_CHECK_GT(hotThdAt(XfmrType::UtcA24, 0.25f, 440.0f), hotThdAt(XfmrType::Api2503, 0.25f, 440.0f) * 5.0);
    ZS_CHECK_LT(hotThd(XfmrType::JensenJt11, 1.0f), 5.0);

    // ...and not only in the bass: the drive amplifier saturates the mids
    // too, rising steadily with AMOUNT (no dead zone in the middle of the knob).
    for (auto t : { XfmrType::NeveLo1166, XfmrType::Api2503, XfmrType::UtcA24, XfmrType::HaufeV76 })
    {
        const double q = hotThdAt(t, 0.25f, 440.0f), h = hotThdAt(t, 0.5f, 440.0f), f = hotThdAt(t, 1.0f, 440.0f);
        std::printf("    transformer %d +HOT at 440 Hz: %.2f %% / %.2f %% / %.2f %% (25 / 50 / 100 %%)\n", (int) t, q, h, f);
        ZS_CHECK_GT(h, 1.0);
        ZS_CHECK_GT(f, 8.0);
        ZS_CHECK_GT(f, h * 1.5);
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
