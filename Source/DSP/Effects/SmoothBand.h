#pragma once

// SMOOTH: a fixed-band dynamic compressor centred on 2.2 kHz, the presence
// region where synths turn harsh. A state-variable band-pass isolates the
// band; when it rises above the threshold, a matching portion of it is
// subtracted, which is exactly a dynamic bell cut (x - (1 - g) * band).
// Fixed 4:1 ratio, fast attack, musical release. AMOUNT lowers the threshold;
// at 0 the stage is bypassed exactly.

#include "../Common/DspCommon.h"

#include <atomic>

namespace zsynth::dsp
{

class SmoothBand
{
public:
    static constexpr float kCentreHz = 2200.0f;
    static constexpr float kQ = 1.0f;
    static constexpr float kRatio = 4.0f;
    static constexpr float kMaxReductionDb = 18.0f;

    /** Threshold for an AMOUNT of 0..1: from -6 dBFS (gentle) to -36 dBFS. */
    static float thresholdDb(float amount) { return -6.0f - 30.0f * amount; }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        const float g = std::tan(kPi * std::min(kCentreHz, 0.45f * fs) / fs);
        k = 1.0f / kQ;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        attack = 1.0f - std::exp(-1.0f / (0.002f * fs));
        release = 1.0f - std::exp(-1.0f / (0.12f * fs));
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
            ic1[c] = ic2[c] = 0.0f;
        env = 0.0f;
        maxReduction.store(0.0f, std::memory_order_relaxed);
    }

    /** Largest gain reduction (positive dB) since the last call. */
    float takeGainReduction() { return maxReduction.exchange(0.0f, std::memory_order_relaxed); }

    void process(float* L, float* R, int n, float amount)
    {
        if (amount <= 0.001f)
        {
            if (env != 0.0f)
                reset();
            return;
        }
        const float threshold = thresholdDb(clampf(amount, 0.0f, 1.0f));
        float peak = 0.0f;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            float band[2];
            for (int c = 0; c < 2; ++c)
            {
                // TPT state-variable filter; k * bp has unity gain at the centre.
                const float v0 = io[c][i];
                const float v3 = v0 - ic2[c];
                const float v1 = a1 * ic1[c] + a2 * v3;
                const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = undenormalise(2.0f * v1 - ic1[c]);
                ic2[c] = undenormalise(2.0f * v2 - ic2[c]);
                band[c] = k * v1;
            }
            const float level = std::max(std::abs(band[0]), std::abs(band[1]));
            env += (level > env ? attack : release) * (level - env);
            const float over = 20.0f * std::log10(env + 1.0e-9f) - threshold;
            const float reduction = over > 0.0f ? std::min(kMaxReductionDb, over * (1.0f - 1.0f / kRatio)) : 0.0f;
            peak = std::max(peak, reduction);
            const float cut = 1.0f - fastExp2(-reduction * 0.16609640474f);
            for (int c = 0; c < 2; ++c)
                io[c][i] -= cut * band[c];
        }
        if (peak > maxReduction.load(std::memory_order_relaxed))
            maxReduction.store(peak, std::memory_order_relaxed);
    }

private:
    float fs = 48000.0f;
    float k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1[2] {}, ic2[2] {};
    float env = 0.0f, attack = 0.0f, release = 0.0f;
    std::atomic<float> maxReduction { 0.0f };
};

} // namespace zsynth::dsp
