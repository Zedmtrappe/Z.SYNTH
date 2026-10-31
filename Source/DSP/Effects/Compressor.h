#pragma once

// Output compressor with four character models, the last stage before the
// master section. Stereo-linked; gain computed in decibels.
//
//   LA-2A Opto     optical leveller: RMS-like detection, ~10 ms attack and the
//                  T4 cell's two-stage release (fast to about half, then slow,
//                  longer after sustained compression); soft knee; COMPRESS
//                  (about 3:1) or LIMIT (ratio >= 8:1); HF emphasis in the
//                  side-chain; valve output stage colour.
//   SSL Bus        VCA bus compressor: peak detection, hard-ish knee, ratio,
//                  attack (0.1..30 ms) and release (0.1..1.2 s or AUTO,
//                  program dependent), side-chain high-pass; very clean.
//   1176 FET       fast FET limiter: 20..800 us attack, 50..1100 ms release,
//                  4/8/12/20:1 and ALL (all buttons in: harder, overshooting,
//                  grittier); FET colour (odd harmonics).
//   Fairchild 670  variable-mu valve: very soft knee whose ratio rises with
//                  level, smooth time constants, valve colour.
//
// These are original models voiced after those units, not circuit emulations.

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"

#include <atomic>

namespace zsynth::dsp
{

struct CompSettings
{
    CompModel model = CompModel::SSLBus;
    float thresholdDb = -12.0f;
    float makeupDb = 0.0f;
    float mix = 1.0f;
    int ratioIndex = 3;      // ZS_CH_COMP_RATIO
    float attackMs = 10.0f;
    float releaseMs = 300.0f;
    bool autoRelease = false;
    float knee = 0.3f;       // 0..1
    float scHpfHz = 20.0f;
    float emphasis = 0.0f;   // 0..1, side-chain HF emphasis
    float colour = 0.5f;     // 0..1, analogue colour of the model
};

class OutputCompressor
{
public:
    static float ratioFor(int index)
    {
        static constexpr float r[] = { 1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 12.0f, 20.0f, 20.0f };
        return r[juce_clamp(index)];
    }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        reset();
    }

    void reset()
    {
        envFast = envSlow = 0.0f;
        rms = 0.0f;
        for (int c = 0; c < 2; ++c)
            scHp[c] = scLp[c] = 0.0f;
        maxReduction.store(0.0f, std::memory_order_relaxed);
    }

    /** Largest gain reduction (positive dB) since the last call; for the meter. */
    float takeGainReduction() { return maxReduction.exchange(0.0f, std::memory_order_relaxed); }

    void process(float* L, float* R, int n, const CompSettings& s)
    {
        // ---- model-specific behaviour
        float ratio = ratioFor(s.ratioIndex);
        float kneeDb = 1.0f + s.knee * 12.0f;
        float attackMs = s.attackMs, releaseMs = s.releaseMs;
        float slowAttackMs = 0.0f, slowReleaseMs = 0.0f; // program-dependent second stage (0 = off)
        bool rmsDetect = false, fairchild = false;
        float threshold = s.thresholdDb;
        float satAmount = 0.0f, satAsym = 0.0f;
        float overshoot = 1.0f;
        switch (s.model)
        {
            case CompModel::LA2A:
                ratio = s.ratioIndex >= 4 ? 20.0f : 3.0f; // COMPRESS / LIMIT
                kneeDb = 12.0f;
                attackMs = 10.0f;
                releaseMs = 60.0f;            // the cell's fast stage...
                slowAttackMs = 400.0f;        // ...charges a slow memory while compressing
                slowReleaseMs = 2500.0f;
                rmsDetect = true;
                satAmount = 0.5f;
                satAsym = 0.25f;              // valve: even harmonics
                break;
            case CompModel::SSLBus:
                attackMs = clampf(attackMs, 0.1f, 30.0f);
                releaseMs = clampf(releaseMs, 100.0f, 1200.0f);
                kneeDb = 1.0f + s.knee * 6.0f;
                if (s.autoRelease)
                {
                    releaseMs = 120.0f;
                    slowAttackMs = 150.0f;
                    slowReleaseMs = 1200.0f;
                }
                satAmount = 0.08f;            // nearly clean VCA
                break;
            case CompModel::FET1176:
                attackMs = clampf(attackMs, 0.02f, 0.8f);
                releaseMs = clampf(releaseMs, 50.0f, 1100.0f);
                kneeDb = 2.0f;
                if (s.ratioIndex >= 7)
                {
                    // All buttons in: lower effective threshold, a hard knee,
                    // gain-reduction overshoot and a lot more grit.
                    threshold -= 4.0f;
                    kneeDb = 0.5f;
                    overshoot = 1.3f;
                    satAmount = 1.0f;
                }
                else
                {
                    satAmount = 0.45f;
                }
                break;
            case CompModel::Fairchild:
            default:
                fairchild = true;
                kneeDb = 18.0f;
                attackMs = clampf(attackMs, 0.2f, 2.0f);
                releaseMs = clampf(releaseMs, 300.0f, 3000.0f);
                rmsDetect = true;
                satAmount = 0.55f;
                satAsym = 0.1f;
                break;
        }
        satAmount *= 2.0f * clampf(s.colour, 0.0f, 1.0f);

        const float aAtt = timeCoeff(attackMs), aRel = timeCoeff(releaseMs);
        const float aSlowAtt = slowAttackMs > 0.0f ? timeCoeff(slowAttackMs) : 0.0f;
        const float aSlowRel = slowReleaseMs > 0.0f ? timeCoeff(slowReleaseMs) : 0.0f;
        const float aRms = timeCoeff(8.0f);
        const float aHp = 1.0f - std::exp(-kTwoPi * clampf(s.scHpfHz, 10.0f, 2000.0f) / fs);
        const float aEmph = 1.0f - std::exp(-kTwoPi * 2000.0f / fs);
        const bool hp = s.scHpfHz > 21.0f;
        const float emph = s.model == CompModel::LA2A ? clampf(s.emphasis, 0.0f, 1.0f) * 3.0f : 0.0f;
        const float makeup = dbToGain(s.makeupDb);
        const float mix = clampf(s.mix, 0.0f, 1.0f);
        float peakReduction = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // ---- side-chain: optional high-pass and HF emphasis, linked stereo
            float sc[2] = { L[i], R[i] };
            for (int c = 0; c < 2; ++c)
            {
                if (hp)
                {
                    scHp[c] += aHp * (sc[c] - scHp[c]);
                    sc[c] -= scHp[c];
                }
                if (emph > 0.0f)
                {
                    scLp[c] += aEmph * (sc[c] - scLp[c]);
                    sc[c] += emph * (sc[c] - scLp[c]);
                }
            }
            float levelDb;
            if (rmsDetect)
            {
                const float p = 0.5f * (sc[0] * sc[0] + sc[1] * sc[1]);
                rms += aRms * (p - rms);
                levelDb = 10.0f * std::log10(rms + 1.0e-12f) + 3.0f; // sine RMS -> peak-like scale
            }
            else
            {
                levelDb = 20.0f * std::log10(std::max(std::abs(sc[0]), std::abs(sc[1])) + 1.0e-9f);
            }

            // ---- static curve (negative dB of gain change)
            const float over = levelDb - threshold;
            float r = ratio;
            if (fairchild)
                r = 1.0f + clampf(over, 0.0f, 30.0f) / 4.0f; // variable-mu: ratio grows with level
            float target;
            if (2.0f * over < -kneeDb)
                target = 0.0f;
            else if (2.0f * std::abs(over) <= kneeDb)
                target = (1.0f / r - 1.0f) * (over + 0.5f * kneeDb) * (over + 0.5f * kneeDb) / (2.0f * kneeDb);
            else
                target = (1.0f / r - 1.0f) * over;
            target *= overshoot;

            // ---- ballistics in dB (more negative = more reduction)
            envFast += (target < envFast ? aAtt : aRel) * (target - envFast);
            float applied = envFast;
            if (aSlowAtt > 0.0f)
            {
                envSlow += (target < envSlow ? aSlowAtt : aSlowRel) * (target - envSlow);
                applied = std::min(envFast, envSlow * 0.85f);
            }
            envFast = undenormalise(envFast);
            envSlow = undenormalise(envSlow);
            peakReduction = std::max(peakReduction, -applied);
            const float g = fastExp2(applied * 0.16609640474f); // dB -> gain

            // ---- gain, model colour, makeup, parallel mix
            float io[2] = { L[i], R[i] };
            for (int c = 0; c < 2; ++c)
            {
                float y = io[c] * g;
                if (satAmount > 0.0f)
                {
                    const float k = 1.0f + satAmount;
                    const float a = y * k;
                    const float shaped = a >= 0.0f ? fastTanh(a) : fastTanh(a * (1.0f + satAsym)) / (1.0f + satAsym);
                    y = shaped / k; // unity small-signal gain: colour, not level
                }
                y *= makeup;
                io[c] = io[c] + (y - io[c]) * mix;
            }
            L[i] = io[0];
            R[i] = io[1];
        }
        if (peakReduction > maxReduction.load(std::memory_order_relaxed))
            maxReduction.store(peakReduction, std::memory_order_relaxed);
    }

private:
    static int juce_clamp(int i) { return i < 0 ? 0 : (i > 7 ? 7 : i); }
    float timeCoeff(float ms) const { return 1.0f - std::exp(-1.0f / (std::max(0.01f, ms) * 0.001f * fs)); }

    float fs = 48000.0f;
    float envFast = 0.0f, envSlow = 0.0f, rms = 0.0f;
    float scHp[2] {}, scLp[2] {};
    std::atomic<float> maxReduction { 0.0f };
};

} // namespace zsynth::dsp
