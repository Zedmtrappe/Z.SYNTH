#pragma once

// Band-limited oscillator for all ZYNTH oscillator waveforms.
//
// Anti-aliasing: every discontinuity is corrected with a 2-point polynomial
// BLEP (value steps) and/or BLAMP (slope changes), placed at its exact
// sub-sample time. To correct the sample *before* an event, output is delayed
// by one sample (the "lookahead" form of PolyBLEP). Natural edges (saw wrap,
// pulse edges, triangle corners) and hard-sync resets share this code, so
// sync sweeps stay clean. Wavetable frames are already band-limited by their
// mipmaps; they only need corrections when hard-synced.
//
// Phase is in cycles [0, 1); the increment is in cycles per sample and may
// change every sample (audio-rate FM).

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"
#include "../Wavetable/WavetableBank.h"

namespace zsynth::dsp
{

class Oscillator
{
public:
    void reset(float startPhase)
    {
        phase = startPhase - std::floor(startPhase);
        delayed = 0.0f;
        lastWrapAfter = -1.0f;
    }

    void setWave(OscWave w) { wave = w; }
    void setPulseWidth(float pw) { pulseWidth = clampf(pw, 0.02f, 0.98f); }

    /** Wavetable frames for the current control block (mip already chosen). */
    void setWavetableFrames(const float* a, const float* b, int size) { frameA = a; frameB = b; tableSize = size; }
    void setWavetableMix(float m) { frameMix = clampf(m, 0.0f, 1.0f); }

    float getPhase() const { return phase; }

    /** Time (in samples, 0..1) between this oscillator's last wrap and the most
        recent output instant, or -1 if it did not wrap this sample. Used as
        the hard-sync trigger for slave oscillators and the sub-oscillator. */
    float wrapAfter() const { return lastWrapAfter; }

    /** Advances one sample and returns the (one-sample delayed) output.
        syncAfter >= 0 resets the phase at that sub-sample time (see wrapAfter). */
    float process(float inc, float syncAfter)
    {
        inc = clampf(inc, 0.0f, 0.49f);
        lastWrapAfter = -1.0f;
        currentCorrection = 0.0f;

        float p = phase;
        float t = 0.0f;

        if (syncAfter >= 0.0f)
        {
            const float resetTime = 1.0f - syncAfter;
            p = advance(p, inc, 0.0f, resetTime);

            const float before = valueAt(p);
            const float after  = valueAt(0.0f);
            addBlep(syncAfter, after - before);
            addBlamp(syncAfter, (slopeAt(0.0f) - slopeAt(p)) * inc);

            p = 0.0f;
            t = resetTime;
            lastWrapAfter = syncAfter;
        }

        p = advance(p, inc, t, 1.0f);
        p = wrapPhase(p);
        phase = p;

        const float out = delayed;
        delayed = valueAt(p) + currentCorrection;
        return out;
    }

private:
    float nextEventPhase(float p) const
    {
        switch (wave)
        {
            case OscWave::Square:   return p < 0.5f ? 0.5f : 1.0f;
            case OscWave::Pulse:    return p < pulseWidth ? pulseWidth : 1.0f;
            case OscWave::Triangle: return p < 0.5f ? 0.5f : 1.0f;
            default:                return 1.0f;
        }
    }

    /** Moves phase from time t0 to t1 (fractions of this sample), applying
        corrections for every natural event crossed. */
    float advance(float p, float inc, float t0, float t1)
    {
        if (inc <= 0.0f)
            return p;

        float t = t0;
        for (int guard = 0; guard < 4; ++guard)
        {
            const float target = nextEventPhase(p);
            const float end = p + inc * (t1 - t);
            if (end < target)
                return end; // common case: no edge this sample, no division needed
            const float dt = (target - p) / inc;
            if (t + dt > t1)
                return p + inc * (t1 - t);

            t += dt;
            const float after = clampf(1.0f - t, 0.0f, 1.0f);
            handleEvent(target, after, inc);
            if (target >= 1.0f)
            {
                p = 0.0f;
                lastWrapAfter = after;
            }
            else
            {
                p = target;
            }
        }
        return p;
    }

    void handleEvent(float target, float after, float inc)
    {
        const bool wrap = target >= 1.0f;
        switch (wave)
        {
            case OscWave::Saw:      if (wrap) addBlep(after, -2.0f); break;
            case OscWave::Square:
            case OscWave::Pulse:    addBlep(after, wrap ? 2.0f : -2.0f); break;
            case OscWave::Triangle: addBlamp(after, (wrap ? 8.0f : -8.0f) * inc); break;
            default: break;
        }
    }

    /** Step of height h occurring 'after' samples before the current instant. */
    void addBlep(float after, float h)
    {
        const float a = after;
        const float b = 1.0f - after;
        delayed += 0.5f * h * a * a;          // sample before the step
        currentCorrection -= 0.5f * h * b * b; // sample after the step
    }

    /** Slope change of m (value per sample) 'after' samples before now. */
    void addBlamp(float after, float m)
    {
        const float a = after;
        const float b = 1.0f - after;
        delayed += m * a * a * a * (1.0f / 6.0f);
        currentCorrection += m * b * b * b * (1.0f / 6.0f);
    }

    float tableValue(const float* table, float p) const
    {
        const float pos = p * (float) tableSize;
        const int i = std::min((int) pos, tableSize - 1);
        const float f = pos - (float) i;
        return table[i] + (table[i + 1] - table[i]) * f;
    }

    float valueAt(float p) const
    {
        switch (wave)
        {
            case OscWave::Sine:     return fastSin(p);
            case OscWave::Triangle: return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            case OscWave::Saw:      return 2.0f * p - 1.0f;
            case OscWave::Square:   return p < 0.5f ? 1.0f : -1.0f;
            case OscWave::Pulse:    return p < pulseWidth ? 1.0f : -1.0f;
            case OscWave::Wavetable:
                if (frameA == nullptr)
                    return 0.0f;
                return tableValue(frameA, p) + (tableValue(frameB, p) - tableValue(frameA, p)) * frameMix;
            default: return 0.0f;
        }
    }

    /** d(value)/d(phase), used for sync BLAMP corrections. */
    float slopeAt(float p) const
    {
        switch (wave)
        {
            case OscWave::Sine:     return kTwoPi * fastSin(p + 0.25f);
            case OscWave::Triangle: return p < 0.5f ? 4.0f : -4.0f;
            case OscWave::Saw:      return 2.0f;
            case OscWave::Wavetable:
            {
                const float d = 1.0f / (float) tableSize;
                const float p2 = p + d >= 1.0f ? p + d - 1.0f : p + d;
                return (valueAt(p2) - valueAt(p)) * (float) tableSize;
            }
            default: return 0.0f;
        }
    }

    OscWave wave = OscWave::Saw;
    float phase = 0.0f;
    float pulseWidth = 0.5f;
    float delayed = 0.0f;
    float currentCorrection = 0.0f;
    float lastWrapAfter = -1.0f;

    const float* frameA = nullptr;
    const float* frameB = nullptr;
    float frameMix = 0.0f;
    int tableSize = Wavetable::kSize;
};

// -----------------------------------------------------------------------------
/** Stacks of detuned sawtooths.

    SuperSaw (after the JP-8000): seven saws with Roland's uneven detune
    spacing and non-linear DETUNE / MIX curves (from Adam Szabo's analysis of
    the original), free-running phases, and a high-pass at the fundamental
    that removes the stack's sub-fundamental aliasing as the original does.

    HyperSaw (Virus style): nine saws spread evenly, a smoother, wider and
    more even detune curve, and no fundamental high-pass (fuller low end).

    Each saw is a 2-point PolyBLEP saw; the stack is mono (stereo width comes
    from unison spread and the chorus, as on the hardware). */
class SuperSaw
{
public:
    static constexpr int kMaxSaws = 9;

    void reset(Random& rng)
    {
        for (auto& p : phase)
            p = rng.unipolar();
        hp = 0.0f;
    }

    /** detune and mix are 0..1 knob positions. */
    void setup(bool hyper, float detune, float mix)
    {
        hyperMode = hyper;
        detune = clampf(detune, 0.0f, 1.0f);
        mix = clampf(mix, 0.0f, 1.0f);
        float centre, side;
        if (! hyper)
        {
            static constexpr float offsets[7] = { -0.11002313f, -0.06288439f, -0.01952356f, 0.0f, 0.01991221f, 0.06216538f, 0.10745242f };
            const double x = detune;
            // Szabo's 11th-order fit of the JP-8000 detune knob.
            const double d = ((((((((((10028.7312891634 * x - 50818.8652045924) * x + 111363.4808729368) * x - 138150.6761080548) * x
                              + 106649.6679158292) * x - 53046.9642751875) * x + 17019.9518580080) * x - 3425.0836591318) * x
                              + 404.2703938388) * x - 24.1878824391) * x + 0.6717417634) * x + 0.0030115596;
            count = 7;
            for (int i = 0; i < 7; ++i)
                ratio[i] = 1.0f + offsets[i] * (float) d;
            centre = -0.55366f * mix + 0.99785f;
            side = -0.73764f * mix * mix + 1.2841f * mix + 0.044372f;
            for (int i = 0; i < 7; ++i)
                gain[i] = i == 3 ? centre : side;
        }
        else
        {
            count = 9;
            const float spread = 0.0015f + 0.075f * detune * detune; // up to about +/- 1.3 semitones
            for (int i = 0; i < 9; ++i)
            {
                const float pos = ((float) i - 4.0f) / 4.0f;            // -1 .. 1
                ratio[i] = 1.0f + spread * pos * (0.6f + 0.4f * std::abs(pos));
            }
            centre = 1.0f - 0.45f * mix;
            side = 0.1f + 0.8f * mix;
            for (int i = 0; i < 9; ++i)
                gain[i] = i == 4 ? centre : side * (1.0f - 0.15f * std::abs((float) i - 4.0f) / 4.0f);
        }
        // Keep loudness roughly constant across MIX (random-phase sum).
        float power = 0.0f;
        for (int i = 0; i < count; ++i)
            power += gain[i] * gain[i];
        norm = 1.0f / std::sqrt(std::max(1.0e-6f, power));
    }

    float process(float inc)
    {
        inc = clampf(inc, 0.0f, 0.45f);
        float sum = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            const float dt = std::min(0.45f, inc * ratio[i]);
            float p = phase[i] + dt;
            if (p >= 1.0f)
                p -= 1.0f;
            phase[i] = p;
            float saw = 2.0f * p - 1.0f;
            if (p < dt)
            {
                const float x = p / dt;
                saw -= x + x - x * x - 1.0f;
            }
            else if (p > 1.0f - dt)
            {
                const float x = (p - 1.0f) / dt;
                saw -= x * x + x + x + 1.0f;
            }
            sum += saw * gain[i];
        }
        float y = sum * norm;
        if (! hyperMode)
        {
            // One-pole high-pass at the fundamental (TPT form).
            const float g = kPi * inc; // tan(pi f/fs) ~ pi f/fs at audio pitches
            const float G = g / (1.0f + g);
            const float v = (y - hp) * G;
            const float lp = v + hp;
            hp = undenormalise(lp + v);
            y -= lp;
            y *= 1.15f;
        }
        return y;
    }

private:
    float phase[kMaxSaws] {};
    float ratio[kMaxSaws] {};
    float gain[kMaxSaws] {};
    int count = 7;
    float norm = 1.0f;
    float hp = 0.0f;
    bool hyperMode = false;
};

// -----------------------------------------------------------------------------
/** Sub-oscillator locked to oscillator 1. It is a band-limited oscillator
    running at 1/2 (or 1/4) of osc 1's frequency that is hard-synced every
    2 (or 4) master cycles, exactly where it would wrap anyway. This keeps it
    phase-locked like a hardware divider while reusing the BLEP machinery. */
class SubOscillator
{
public:
    void reset(float startPhase) { osc.reset(startPhase); count = 0; }

    void setWave(SubWave w)
    {
        wave = w;
        switch (w)
        {
            case SubWave::SineMinus1:  osc.setWave(OscWave::Sine); break;
            case SubWave::PulseMinus1: osc.setWave(OscWave::Pulse); osc.setPulseWidth(0.25f); break;
            default:                   osc.setWave(OscWave::Square); break;
        }
    }

    /** masterWrapAfter: osc 1's wrapAfter() for this sample; masterInc: its increment. */
    float process(float masterWrapAfter, float masterInc)
    {
        const int divide = wave == SubWave::SquareMinus2 ? 4 : 2;
        float sync = -1.0f;
        if (masterWrapAfter >= 0.0f && ++count >= divide)
        {
            count = 0;
            sync = masterWrapAfter;
        }
        return osc.process(masterInc / (float) divide, sync);
    }

private:
    Oscillator osc;
    SubWave wave = SubWave::SquareMinus1;
    int count = 0;
};

// -----------------------------------------------------------------------------
/** White and pink noise. Pink uses Paul Kellet's economy filter. */
class NoiseGenerator
{
public:
    void setSeed(uint32_t s) { rng.setSeed(s); }

    float process(NoiseType type)
    {
        const float white = rng.bipolar();
        if (type == NoiseType::White)
            return white * 0.7f;

        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        return (b0 + b1 + b2 + white * 0.1848f) * 0.22f;
    }

private:
    Random rng;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
};

} // namespace zsynth::dsp
