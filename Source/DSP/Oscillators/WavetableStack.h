#pragma once

// Wavetable stack: up to eight detuned copies of one wavetable oscillator,
// each reading its own pair of frames (so the copies can sit at different
// positions in the table), spread across the stereo field, with a WARP that
// reshapes how the table is read.
//
// Warps act on the read-out phase (bend, sync, formant, mirror, squeeze,
// quantize) or on the output (fold, self-FM). The phase maps are smooth and
// periodic (no corners), and the oscillator reads a duller mip in proportion
// to how much a warp widens the spectrum (bandwidthFactor), so warped waves
// stay clean of fold-over. Sync's reset and Quantize's steps are deliberate
// digital exceptions.
//
// A single copy with the warp off never uses this class: that path stays on
// the band-limited Oscillator (with BLEP hard sync), exactly as before.

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"
#include "../Wavetable/WavetableBank.h"

#include <cmath>

namespace zsynth::dsp
{

class WavetableStack
{
public:
    static constexpr int kMax = 8;

    void reset(Random& rng)
    {
        for (int j = 0; j < kMax; ++j)
        {
            phase[j] = j == 0 ? 0.0f : rng.unipolar();
            fb1[j] = fb2[j] = 0.0f;
            key[j] = -1;
            mixStart[j] = mixEnd[j] = 0.0f;
        }
    }

    /** Per control block, before setFrames. detune and width are 0..1 knobs. */
    void setup(int voiceCount, float detune, float width, WtWarp w, float warpAmount)
    {
        voices = std::clamp(voiceCount, 1, kMax);
        warp = w;
        amount = clampf(warpAmount, 0.0f, 1.0f);

        const float cents = 70.0f * std::pow(clampf(detune, 0.0f, 1.0f), 1.7f);
        const float norm = 1.0f / std::sqrt((float) voices);
        for (int j = 0; j < voices; ++j)
        {
            const float x = position(j);
            // Slightly uneven spacing (as on the SuperSaw) so the beating never lines up.
            const float xd = x * (0.8f + 0.2f * std::abs(x)) + (j % 2 == 0 ? 0.0f : 0.03f * x);
            ratio[j] = semitonesToRatio(xd * cents * 0.01f);
            const float angle = (clampf(x * width, -1.0f, 1.0f) + 1.0f) * (kPi * 0.25f);
            gainL[j] = std::cos(angle) * 1.41421356f * norm;
            gainR[j] = std::sin(angle) * 1.41421356f * norm;
        }
        maxRatio = semitonesToRatio(cents * 0.01f);

        // Per-warp constants.
        const float a = amount;
        bend = 0.96f * a;
        squeeze = 0.96f * a;
        syncRatio = 1.0f + 7.0f * a;
        windowAmt = std::min(1.0f, 4.0f * a);
        foldGain = 1.0f + 5.0f * a;
        selfFm = 0.18f * a;
        quantSteps = std::round(std::exp2(9.0f - 7.0f * a));
    }

    /** -1..1 position of copy j across the stack. */
    float position(int j) const { return voices > 1 ? 2.0f * (float) j / (float) (voices - 1) - 1.0f : 0.0f; }
    int count() const { return voices; }

    /** How much the warp widens the spectrum (to choose the mip). */
    float bandwidthFactor() const
    {
        float f = 1.0f;
        switch (warp)
        {
            case WtWarp::BendPlus:
            case WtWarp::BendMinus: f = 1.0f + 2.0f * bend; break;
            case WtWarp::Sync:
            case WtWarp::Formant:   f = syncRatio; break;
            case WtWarp::Mirror:    f = 1.0f + 2.5f * amount; break;
            case WtWarp::Squeeze:   f = 1.0f + 3.0f * squeeze; break;
            case WtWarp::Fold:      f = 1.0f + 2.0f * amount; break;
            case WtWarp::SelfFm:    f = 1.0f + 6.0f * amount; break;
            default: break;
        }
        // The warp itself makes the brightness: reading a mip that much duller
        // keeps its output below Nyquist.
        return std::min(8.0f, f) * maxRatio;
    }

    /** Frames for copy j this block. pairKey identifies the frame pair: while it
        stays the same, the cross-fade glides from its last value. */
    void setFrames(int j, const float* a, const float* b, int size, float mix, int pairKey, bool first)
    {
        fa[j] = a;
        fb[j] = b;
        tableSize[j] = size;
        mixStart[j] = (! first && pairKey == key[j]) ? mixEnd[j] : mix;
        mixEnd[j] = mix;
        key[j] = pairKey;
    }

    /** One sample. t = 0..1 through the control block (cross-fade interpolation);
        sync = the master wrapped (hard sync resets every copy). */
    void process(float inc, float t, bool sync, float& outL, float& outR)
    {
        // One dispatch per sample; the copies' loop is specialised for the warp.
        switch (warp)
        {
            case WtWarp::BendPlus:  run<WtWarp::BendPlus>(inc, t, sync, outL, outR); break;
            case WtWarp::BendMinus: run<WtWarp::BendMinus>(inc, t, sync, outL, outR); break;
            case WtWarp::Sync:      run<WtWarp::Sync>(inc, t, sync, outL, outR); break;
            case WtWarp::Formant:   run<WtWarp::Formant>(inc, t, sync, outL, outR); break;
            case WtWarp::Mirror:    run<WtWarp::Mirror>(inc, t, sync, outL, outR); break;
            case WtWarp::Squeeze:   run<WtWarp::Squeeze>(inc, t, sync, outL, outR); break;
            case WtWarp::Fold:      run<WtWarp::Fold>(inc, t, sync, outL, outR); break;
            case WtWarp::SelfFm:    run<WtWarp::SelfFm>(inc, t, sync, outL, outR); break;
            case WtWarp::Quantize:  run<WtWarp::Quantize>(inc, t, sync, outL, outR); break;
            default:                run<WtWarp::Off>(inc, t, sync, outL, outR); break;
        }
    }

    void endBlock()
    {
        for (int j = 0; j < voices; ++j)
            mixStart[j] = mixEnd[j];
    }

private:
    float lookup(int j, float p, float mix) const
    {
        if (fa[j] == nullptr)
            return 0.0f;
        const int n = tableSize[j];
        const float pos = p * (float) n;
        const int i = std::min((int) pos, n - 1);
        const float f = pos - (float) i;
        const float* a = fa[j];
        const float* b = fb[j];
        const float va = a[i] + (a[i + 1] - a[i]) * f;
        const float vb = b[i] + (b[i + 1] - b[i]) * f;
        return va + (vb - va) * mix;
    }

    static float wrap01(float p) { return p - fastFloor(p); }

    template <WtWarp W>
    void run(float inc, float t, bool sync, float& outL, float& outR)
    {
        inc = clampf(inc, 0.0f, 0.45f);
        float l = 0.0f, r = 0.0f;
        for (int j = 0; j < voices; ++j)
        {
            float p = phase[j] + std::min(0.45f, inc * ratio[j]);
            p -= (float) (int) p; // p >= 0
            if (sync)
                p = 0.0f;
            phase[j] = p;
            const float y = read<W>(j, p, mixStart[j] + (mixEnd[j] - mixStart[j]) * t);
            l += y * gainL[j];
            r += y * gainR[j];
        }
        outL = l;
        outR = r;
    }

    template <WtWarp W>
    float read(int j, float p, float mix)
    {
        if constexpr (W == WtWarp::BendPlus)  // CZ-style phase distortion: the cycle's start squeezed, its end stretched
            return lookup(j, wrap01(p + bend * fastSin(p) * kInv2Pi), mix);
        else if constexpr (W == WtWarp::BendMinus)
            return lookup(j, wrap01(p - bend * fastSin(p) * kInv2Pi), mix);
        else if constexpr (W == WtWarp::Sync)
            return lookup(j, wrap01(p * syncRatio), mix);
        else if constexpr (W == WtWarp::Formant)
        {
            // Sync with the cycle windowed (raised cosine): the formants move,
            // the pitch stays, and there is no reset click.
            const float w = 1.0f - windowAmt * (0.5f + 0.5f * fastSin(p + 0.25f));
            return lookup(j, wrap01(p * syncRatio), mix) * w;
        }
        else if constexpr (W == WtWarp::Mirror)
        {
            // Forwards through the wave and back again in one cycle (a raised-cosine sweep).
            const float q = 0.5f - 0.5f * fastSin(p + 0.25f);
            const float y = lookup(j, p, mix);
            return y + (lookup(j, std::min(q, 0.999999f), mix) - y) * amount;
        }
        else if constexpr (W == WtWarp::Squeeze)   // both half-cycles narrowed, like PWM on any wave
            return lookup(j, wrap01(p - squeeze * fastSin(2.0f * p) * kInv4Pi), mix);
        else if constexpr (W == WtWarp::Fold)
            return fastSin(lookup(j, p, mix) * foldGain * 0.25f);
        else if constexpr (W == WtWarp::SelfFm)
        {
            // Phase modulation by its own (averaged) output, DX7 style.
            const float y = lookup(j, wrap01(p + selfFm * 0.5f * (fb1[j] + fb2[j])), mix);
            fb2[j] = fb1[j];
            fb1[j] = y;
            return y;
        }
        else if constexpr (W == WtWarp::Quantize)
            return lookup(j, std::floor(p * quantSteps) / quantSteps, mix);
        else
            return lookup(j, p, mix);
    }

    int voices = 1;
    WtWarp warp = WtWarp::Off;
    float amount = 0.0f;
    float phase[kMax] {}, ratio[kMax] {}, gainL[kMax] {}, gainR[kMax] {};
    float fb1[kMax] {}, fb2[kMax] {};
    const float* fa[kMax] {};
    const float* fb[kMax] {};
    int tableSize[kMax] {};
    float mixStart[kMax] {}, mixEnd[kMax] {};
    int key[kMax] {};
    float maxRatio = 1.0f;
    float bend = 0.0f, syncRatio = 1.0f, windowAmt = 0.0f, squeeze = 0.0f, foldGain = 1.0f, selfFm = 0.0f, quantSteps = 512.0f;
    static constexpr float kInv2Pi = 0.15915494f, kInv4Pi = 0.07957747f;
};

} // namespace zsynth::dsp
