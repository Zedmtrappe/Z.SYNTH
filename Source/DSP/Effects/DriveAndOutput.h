#pragma once

// Drive effect and the final output stage.

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"
#include "../Common/HalfBand.h"

namespace zsynth::dsp
{

/** Overdrive / distortion / wavefolder, processed at 2x with half-band
    resampling so the shaper's harmonics do not fold back into the audio band. */
class Drive
{
public:
    void prepare(float sampleRate) { fs = sampleRate; reset(); }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            up[c].reset();
            down[c].reset();
            tone[c] = 0.0f;
            dc[c] = 0.0f;
        }
    }

    void process(float* L, float* R, int n, DriveType type, float amount, float toneAmt, float mix)
    {
        const float gain = dbToGain(amount * 36.0f);
        // Rough loudness compensation so turning drive up does not just get louder.
        const float makeup = 1.0f / std::sqrt(gain);
        const float toneHz = 700.0f * fastExp2(toneAmt * 4.6f);
        const float aTone = 1.0f - std::exp(-kTwoPi * std::min(toneHz, 0.45f * fs) / fs);
        const float aDc = 1.0f - std::exp(-kTwoPi * 8.0f / fs);
        float* ch[2] = { L, R };

        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                float a, b;
                up[c].process(x * gain, a, b);
                const float y = down[c].process(shape(type, a, amount), shape(type, b, amount)) * makeup;

                tone[c] += aTone * (y - tone[c]);
                dc[c] += aDc * (tone[c] - dc[c]);
                const float wet = tone[c] - dc[c];
                ch[c][i] = x + (wet - x) * mix;
            }
        }
    }

private:
    static float shape(DriveType type, float x, float amount)
    {
        switch (type)
        {
            case DriveType::Overdrive:
            {
                // Asymmetric soft clipping: adds even harmonics like a driven tube stage.
                constexpr float bias = 0.25f;
                return fastTanh(x + bias) - fastTanh(bias);
            }
            case DriveType::Distortion:
            {
                // Harder knee: cubic soft clip that flattens fully at |x| = 1.25.
                return 1.5f * softClipCubic(clampf(x * 0.8f, -1.0f, 1.0f));
            }
            case DriveType::Fold:
            {
                // Sine wavefolder: folds back repeatedly as level increases.
                const float folds = 0.5f + amount * 2.5f;
                return fastSin(x * folds * 0.25f);
            }
            default:
                return x;
        }
    }

    float fs = 48000.0f;
    HalfBandInterpolator up[2];
    HalfBandDecimator down[2];
    float tone[2] {}, dc[2] {};
};

// -----------------------------------------------------------------------------
/** DC blocking, stereo width, "character" saturation, master gain, safety
    limiter, NaN protection and peak metering. */
class OutputStage
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        aDc = 1.0f - std::exp(-kTwoPi * 5.0f / fs);
        limiterRelease = std::exp(-1.0f / (0.12f * fs));
        meterDecay = std::exp(-1.0f / (0.3f * fs));
        reset();
    }

    void reset()
    {
        dc[0] = dc[1] = 0.0f;
        limiterGain = 1.0f;
        gainSmoothed = -1.0f;
        peak[0] = peak[1] = 0.0f;
    }

    /** Returns true if non-finite samples were found (and removed). */
    bool process(float* L, float* R, int n, float width, float character, float masterGain, bool limiter)
    {
        if (gainSmoothed < 0.0f)
            gainSmoothed = masterGain;
        const float gainCoeff = 1.0f - std::exp(-1.0f / (0.02f * fs));
        constexpr float ceiling = 0.966f; // -0.3 dBFS
        bool foundBad = false;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (! isFiniteSample(l) || ! isFiniteSample(r))
            {
                l = r = 0.0f;
                foundBad = true;
            }

            dc[0] += aDc * (l - dc[0]);
            dc[1] += aDc * (r - dc[1]);
            l -= dc[0];
            r -= dc[1];

            const float mid = 0.5f * (l + r);
            const float side = 0.5f * (l - r) * width;
            l = mid + side;
            r = mid - side;

            if (character > 0.0f)
            {
                // Blend toward a unity-small-signal-gain soft saturator.
                l += (fastTanh(l * 1.5f) * (1.0f / 1.5f) - l) * character;
                r += (fastTanh(r * 1.5f) * (1.0f / 1.5f) - r) * character;
            }

            gainSmoothed += (masterGain - gainSmoothed) * gainCoeff;
            l *= gainSmoothed;
            r *= gainSmoothed;

            if (limiter)
            {
                const float p = std::max(std::abs(l), std::abs(r));
                const float needed = p > ceiling ? ceiling / p : 1.0f;
                limiterGain = needed < limiterGain ? needed : needed + (limiterGain - needed) * limiterRelease;
                l = clampf(l * limiterGain, -1.0f, 1.0f);
                r = clampf(r * limiterGain, -1.0f, 1.0f);
            }

            peak[0] = std::max(std::abs(l), peak[0] * meterDecay);
            peak[1] = std::max(std::abs(r), peak[1] * meterDecay);
            L[i] = l;
            R[i] = r;
        }

        if (foundBad)
            dc[0] = dc[1] = 0.0f;
        return foundBad;
    }

    float getPeak(int channel) const { return peak[channel]; }
    float getLimiterGain() const { return limiterGain; }

private:
    float fs = 48000.0f;
    float aDc = 0.0f;
    float dc[2] {};
    float limiterGain = 1.0f, limiterRelease = 0.999f;
    float gainSmoothed = -1.0f;
    float peak[2] {};
    float meterDecay = 0.999f;
};

} // namespace zsynth::dsp
