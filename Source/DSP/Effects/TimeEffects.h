#pragma once

// Phaser, stereo delay and reverb.

#include "DelayLine.h"

#include <array>

namespace zsynth::dsp
{

// Dual-linear wet/dry law for send-style effects: the dry signal stays at full
// level up to 50 % mix and the wet signal reaches full level at 50 %, so the
// middle of the knob is "both at full" and the ends are pure dry / pure wet.
inline float wetDryDry(float mix) { return std::min(1.0f, 2.0f * (1.0f - mix)); }
inline float wetDryWet(float mix) { return std::min(1.0f, 2.0f * mix); }

// -----------------------------------------------------------------------------
/** Six-stage stereo phaser. Each stage is a TPT first-order all-pass whose
    break frequency is swept exponentially by a sine LFO (90 degrees apart on
    left and right). Feedback around the chain sharpens the notches. */
class Phaser
{
public:
    void prepare(float sampleRate) { fs = sampleRate; reset(); }
    void reset() { for (auto& c : state) c.fill(0.0f); phase = 0.0f; fb[0] = fb[1] = 0.0f; }

    void process(float* L, float* R, int n, float rateHz, float depth, float feedback, float mix)
    {
        const float inc = rateHz / fs;
        const float minHz = 120.0f;
        const float octaves = 1.0f + depth * 6.0f;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            phase = wrapPhase(phase);
            // The sweep is slow (<= 8 Hz): refresh the all-pass coefficient every 8 samples.
            if ((counter++ & 7) == 0)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = 0.5f + 0.5f * fastSin(phase + (c == 0 ? 0.0f : 0.25f));
                    const float hz = std::min(minHz * fastExp2(lfo * octaves), 0.45f * fs);
                    const float g = std::tan(kPi * hz / fs);
                    coeff[c] = g / (1.0f + g);
                }
            }
            for (int c = 0; c < 2; ++c)
            {
                const float G = coeff[c];
                const float x = ch[c][i];
                float y = x + fb[c] * feedback;
                for (int s = 0; s < 6; ++s)
                {
                    // All-pass from a one-pole: ap = 2 * lp - x.
                    const float v = (y - state[c][(size_t) s]) * G;
                    const float lp = v + state[c][(size_t) s];
                    state[c][(size_t) s] = undenormalise(lp + v);
                    y = 2.0f * lp - y;
                }
                fb[c] = fastTanh(y);
                ch[c][i] = x + 0.5f * mix * (y - x); // mix 1 = classic 50/50 notch blend
            }
        }
    }

private:
    float fs = 48000.0f;
    std::array<float, 6> state[2] {};
    float phase = 0.0f;
    float fb[2] {};
    float coeff[2] { 0.1f, 0.1f };
    uint32_t counter = 0;
};

// -----------------------------------------------------------------------------
/** Stereo delay with optional ping-pong, tone-shaped and soft-saturated
    feedback, and smoothed time changes (repitches like tape instead of clicking). */
class StereoDelay
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : lines)
            d.prepare((int) (2.1f * fs));
        reset();
    }

    void reset()
    {
        for (auto& d : lines)
            d.reset();
        for (int c = 0; c < 2; ++c)
            lp[c] = hp[c] = 0.0f;
        smoothedDelay = -1.0f;
    }

    void process(float* L, float* R, int n, float delaySeconds, float feedback, float tone, bool pingPong, float mix)
    {
        const float target = clampf(delaySeconds * fs, 1.0f, (float) lines[0].capacity() - 4.0f);
        if (smoothedDelay < 0.0f)
            smoothedDelay = target;
        const float smoothCoeff = 1.0f - std::exp(-1.0f / (0.08f * fs));

        // Tone: below 0.5 darkens the repeats, above 0.5 thins them.
        const float lpHz = tone < 0.5f ? 900.0f * fastExp2(tone * 2.0f * 4.4f) : 19000.0f;
        const float hpHz = tone > 0.5f ? 20.0f * fastExp2((tone - 0.5f) * 2.0f * 5.6f) : 20.0f;
        const float aLp = 1.0f - std::exp(-kTwoPi * std::min(lpHz, 0.45f * fs) / fs);
        const float aHp = 1.0f - std::exp(-kTwoPi * hpHz / fs);
        const float dryGain = wetDryDry(mix), wetGain = wetDryWet(mix);

        for (int i = 0; i < n; ++i)
        {
            smoothedDelay += (target - smoothedDelay) * smoothCoeff;
            float outL = lines[0].read(smoothedDelay);
            float outR = lines[1].read(smoothedDelay);

            float fbL = shape(0, outL, aLp, aHp) * feedback;
            float fbR = shape(1, outR, aLp, aHp) * feedback;

            float inL, inR;
            if (pingPong)
            {
                inL = 0.5f * (L[i] + R[i]) + fbR;
                inR = fbL;
            }
            else
            {
                inL = L[i] + fbL;
                inR = R[i] + fbR;
            }
            lines[0].push(fastTanh(inL));
            lines[1].push(fastTanh(inR));

            L[i] = L[i] * dryGain + outL * wetGain;
            R[i] = R[i] * dryGain + outR * wetGain;
        }
    }

private:
    float shape(int c, float x, float aLp, float aHp)
    {
        lp[c] += aLp * (x - lp[c]);
        hp[c] += aHp * (lp[c] - hp[c]);
        return undenormalise(lp[c] - hp[c]);
    }

    float fs = 48000.0f;
    DelayLine lines[2];
    float lp[2] {}, hp[2] {};
    float smoothedDelay = -1.0f;
};

// -----------------------------------------------------------------------------
/** Feedback-delay-network reverb: pre-delay, four series all-pass diffusers,
    then eight modulated delay lines mixed by a Hadamard matrix with per-line
    damping and decay gains derived from the requested RT60. */
class Reverb
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        const float scale = fs / 44100.0f;
        static constexpr float lineMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        static constexpr int diffSamples[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 8; ++i)
        {
            lengths[i] = lineMs[i] * 0.001f * fs;
            lines[i].prepare((int) (lengths[i] + 0.002f * fs + 8));
        }
        for (int i = 0; i < 4; ++i)
        {
            diffLen[i] = std::max(1, (int) ((float) diffSamples[i] * scale));
            diffusers[i].prepare(diffLen[i] + 4);
        }
        predelay.prepare((int) (0.21f * fs));
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& d : diffusers) d.reset();
        predelay.reset();
        for (auto& d : damp) d = 0.0f;
        modPhase = 0.0f;
    }

    void process(float* L, float* R, int n, float size, float damping, float predelaySec, float mix)
    {
        const float rt60 = 0.35f + size * size * 11.0f;
        float gains[8];
        for (int i = 0; i < 8; ++i)
            gains[i] = std::pow(10.0f, -3.0f * lengths[i] / (rt60 * fs));
        const float dampHz = 16000.0f * fastExp2(-damping * 4.5f);
        const float aDamp = 1.0f - std::exp(-kTwoPi * std::min(dampHz, 0.45f * fs) / fs);
        const float pre = std::max(1.0f, predelaySec * fs);
        const float modInc = 0.7f / fs;
        const float modDepth = 0.0006f * fs;
        constexpr float diffGain = 0.62f;
        const float dryGain = wetDryDry(mix), wetGain = wetDryWet(mix);

        for (int i = 0; i < n; ++i)
        {
            predelay.push(0.5f * (L[i] + R[i]));
            float x = predelay.read(pre);

            for (int d = 0; d < 4; ++d)
            {
                const float delayed = diffusers[d].readInt(diffLen[d]);
                const float v = x + delayed * diffGain;
                diffusers[d].push(v);
                x = delayed - v * diffGain;
            }

            modPhase += modInc;
            modPhase = wrapPhase(modPhase);

            float y[8];
            for (int k = 0; k < 8; ++k)
            {
                float len = lengths[k];
                if (k < 4)
                    len += modDepth * fastSin(modPhase + (float) k * 0.25f);
                y[k] = lines[k].read(len);
                damp[k] += aDamp * (y[k] - damp[k]);
                y[k] = undenormalise(damp[k]) * gains[k];
            }

            hadamard8(y);

            for (int k = 0; k < 8; ++k)
                lines[k].push(y[k] + x * 0.35f);

            const float wl = (y[0] - y[1] + y[2] - y[3] + y[4] - y[5]) * 0.5f;
            const float wr = (y[1] + y[2] - y[4] - y[6] + y[7] - y[3]) * 0.5f;
            L[i] = L[i] * dryGain + wl * wetGain;
            R[i] = R[i] * dryGain + wr * wetGain;
        }
    }

private:
    static void hadamard8(float* v)
    {
        for (int len = 1; len < 8; len <<= 1)
            for (int i = 0; i < 8; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float a = v[j], b = v[j + len];
                    v[j] = a + b;
                    v[j + len] = a - b;
                }
        constexpr float norm = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < 8; ++i)
            v[i] *= norm;
    }

    float fs = 48000.0f;
    DelayLine lines[8];
    float lengths[8] {};
    DelayLine diffusers[4];
    int diffLen[4] {};
    DelayLine predelay;
    float damp[8] {};
    float modPhase = 0.0f;
};

} // namespace zsynth::dsp
