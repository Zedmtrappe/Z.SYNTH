#pragma once

// Chorus, string ensemble and flanger: modulated short delays.

#include "DelayLine.h"

namespace zsynth::dsp
{

/** One-pole low-pass used to emulate the band-limited wet path of
    bucket-brigade delays. */
struct WetLowpass
{
    float s = 0.0f, a = 0.0f;
    void setCutoff(float hz, float fs) { a = 1.0f - std::exp(-kTwoPi * hz / fs); }
    float process(float x) { s += a * (x - s); return s; }
};

// -----------------------------------------------------------------------------
/** Stereo chorus in the classic two-phase BBD style: one delay per channel,
    swept by a triangle LFO in opposite phase on left and right. */
class Chorus
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : lines)
            d.prepare((int) (0.03f * fs));
        for (auto& w : wet)
            w.setCutoff(9000.0f, fs);
        reset();
    }

    void reset() { for (auto& d : lines) d.reset(); phase = 0.0f; }

    void process(float* L, float* R, int n, float rateHz, float depth, float mix)
    {
        const float inc = rateHz / fs;
        const float base = 0.007f * fs;
        const float range = 0.0045f * fs * depth;
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            phase = wrapPhase(phase);
            const float tri = 4.0f * std::abs(phase - 0.5f) - 1.0f;

            lines[0].push(L[i]);
            lines[1].push(R[i]);
            const float wl = wet[0].process(lines[0].read(base + range * tri));
            const float wr = wet[1].process(lines[1].read(base - range * tri));
            L[i] += (wl - L[i]) * mix;
            R[i] += (wr - R[i]) * mix;
        }
    }

private:
    float fs = 48000.0f;
    DelayLine lines[2];
    WetLowpass wet[2];
    float phase = 0.0f;
};

// -----------------------------------------------------------------------------
/** String-machine ensemble: three delay taps per channel, each swept by a slow
    and a fast sine at 120-degree offsets, giving the dense, shimmering
    "three-phase" motion. Right channel taps run 60 degrees away from the left. */
class Ensemble
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : lines)
            d.prepare((int) (0.04f * fs));
        for (auto& w : wet)
            w.setCutoff(10000.0f, fs);
        reset();
    }

    void reset() { for (auto& d : lines) d.reset(); slow = fast = 0.0f; }

    void process(float* L, float* R, int n, float rateHz, float depth, float mix)
    {
        const float slowInc = rateHz / fs;
        const float fastInc = rateHz * 9.6f / fs;
        const float base = 0.012f * fs;
        const float slowRange = 0.0035f * fs * depth;
        const float fastRange = 0.0004f * fs * depth;
        for (int i = 0; i < n; ++i)
        {
            slow += slowInc;  slow = wrapPhase(slow);
            fast += fastInc;  fast = wrapPhase(fast);
            lines[0].push(L[i]);
            lines[1].push(R[i]);

            float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                const float off = (float) k / 3.0f;
                const float dl = base + slowRange * fastSin(slow + off) + fastRange * fastSin(fast + off);
                const float dr = base + slowRange * fastSin(slow + off + 1.0f / 6.0f) + fastRange * fastSin(fast + off + 1.0f / 6.0f);
                wl += lines[0].read(dl);
                wr += lines[1].read(dr);
            }
            wl = wet[0].process(wl * (1.0f / 3.0f));
            wr = wet[1].process(wr * (1.0f / 3.0f));
            L[i] += (wl - L[i]) * mix;
            R[i] += (wr - R[i]) * mix;
        }
    }

private:
    float fs = 48000.0f;
    DelayLine lines[2];
    WetLowpass wet[2];
    float slow = 0.0f, fast = 0.0f;
};

// -----------------------------------------------------------------------------
/** Stereo flanger: 0.3-7 ms swept delay with bipolar feedback (negative
    feedback gives the hollow "through-zero-like" colour). */
class Flanger
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : lines)
            d.prepare((int) (0.02f * fs));
        reset();
    }

    void reset() { for (auto& d : lines) d.reset(); phase = 0.0f; fb[0] = fb[1] = 0.0f; }

    void process(float* L, float* R, int n, float rateHz, float depth, float feedback, float mix)
    {
        const float inc = rateHz / fs;
        const float minDelay = 0.0003f * fs;
        const float maxDelay = (0.0003f + 0.0067f * depth) * fs;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            phase = wrapPhase(phase);
            for (int c = 0; c < 2; ++c)
            {
                const float p = phase + (c == 0 ? 0.0f : 0.25f);
                const float tri = 1.0f - 2.0f * std::abs(wrapPhase(p) - 0.5f) * 2.0f; // -1..1
                const float delay = minDelay + (maxDelay - minDelay) * (0.5f + 0.5f * tri);
                const float x = ch[c][i];
                lines[c].push(x + fastTanh(fb[c] * feedback));
                const float w = lines[c].read(std::max(1.0f, delay));
                fb[c] = undenormalise(w);
                // mix = 1 is the classic equal blend of dry and delayed signal
                // (deepest comb notches); 0 is dry.
                ch[c][i] = x + 0.5f * mix * (w - x);
            }
        }
    }

private:
    float fs = 48000.0f;
    DelayLine lines[2];
    float phase = 0.0f;
    float fb[2] {};
};

} // namespace zsynth::dsp
