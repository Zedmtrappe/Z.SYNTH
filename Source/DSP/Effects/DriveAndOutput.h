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
/** Valve output stage: a triode (or push-pull power pair) transfer curve run at
    2x with half-band resampling, power-supply sag, and an output-transformer
    tone stage (top-end roll-off plus low-end weight).

    Triode curve: soft tanh limiting on the positive (grid-conduction) side and
    an earlier, harder limit on the negative (cut-off) side. The asymmetry is
    what produces the even harmonics of single-ended stages; a push-pull pair
    subtracts two mirrored halves, cancelling even harmonics unless the bias
    unbalances them. The stage has unity gain for small signals, so DRIVE
    changes colour (and peak compression) rather than loudness. */
class ValveStage
{
public:
    struct Model
    {
        float minGainDb, maxGainDb; // input gain across the DRIVE range
        float asym;                 // cut-off side hardness at mid bias
        bool pushPull;
        float sagDepth;             // how much the supply sags on loud signals
        float transformerHz;        // top-end roll-off at full WARMTH
        float lowBoostDb;           // low-end weight at full WARMTH
    };

    static const Model& model(ValveType t)
    {
        static const Model models[(int) ValveType::Count] = {
            // Gains are relative to unity small-signal level: at -12 dB nothing
            // saturates, so low DRIVE settings only warm and gently round peaks.
            { -12.0f, 15.0f, 0.6f, false, 0.5f, 9000.0f, 2.5f },  // 12AX7 preamp triode
            { -12.0f, 11.0f, 0.3f, false, 0.3f, 14000.0f, 2.0f }, // 12AU7 low-mu triode
            { -10.0f, 13.0f, 0.5f, true,  0.8f, 7500.0f, 3.0f },  // EL84 push-pull pentodes
            { -12.0f, 12.0f, 0.4f, true,  0.6f, 6500.0f, 4.0f },  // 6L6 power pair
        };
        return models[std::clamp((int) t, 0, (int) ValveType::Count - 1)];
    }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        aDc = 1.0f - std::exp(-kTwoPi * 10.0f / fs);
        aLow = 1.0f - std::exp(-kTwoPi * 150.0f / fs);
        sagAttack = 1.0f - std::exp(-1.0f / (0.005f * fs));
        sagRelease = 1.0f - std::exp(-1.0f / (0.15f * fs));
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            up[c].reset();
            down[c].reset();
            dc[c] = tone[c] = low[c] = 0.0f;
        }
        sagEnv = 0.0f;
        lastGain = -1.0f;
    }

    static float triode(float x, float asym)
    {
        return x >= 0.0f ? fastTanh(x) : fastTanh(x * (1.0f + asym)) / (1.0f + asym);
    }

    void process(float* L, float* R, int n, ValveType type, float drive, float bias, float sag, float warmth, float mix)
    {
        const Model& m = model(type);
        // DRIVE taper: most of the travel is warmth and soft peak compression;
        // the gain rises steeply only near the top, so obvious distortion
        // starts at around 90 %.
        const float d = clampf(drive, 0.0f, 1.0f);
        const float gain = dbToGain(m.minGainDb + std::pow(d, 5.0f) * (m.maxGainDb - m.minGainDb));
        if (lastGain < 0.0f)
            lastGain = gain;
        const float gainStep = (gain - lastGain) / (float) std::max(1, n);

        const float b = clampf(bias, 0.0f, 1.0f) - 0.5f;
        const float asymA = m.pushPull ? m.asym * (1.0f + 1.6f * b) : m.asym * (1.0f + 3.2f * b);
        const float asymB = m.pushPull ? m.asym * (1.0f - 1.6f * b) : asymA;
        const float sagAmt = clampf(sag, 0.0f, 1.0f) * m.sagDepth * 1.2f;

        const float w = clampf(warmth, 0.0f, 1.0f);
        const float lpHz = std::min(20000.0f * std::pow(m.transformerHz / 20000.0f, w), 0.45f * fs);
        const float aTone = 1.0f - std::exp(-kTwoPi * lpHz / fs);
        const float lowBoost = dbToGain(w * m.lowBoostDb) - 1.0f;

        auto shape = [&](float x) {
            return m.pushPull ? 0.5f * (triode(x, asymA) - triode(-x, asymB)) : triode(x, asymA);
        };

        float g = lastGain;
        float env = sagEnv;
        for (int i = 0; i < n; ++i)
        {
            g += gainStep;
            // Sag: the effective drive drops as the supply is loaded, then recovers.
            const float gEff = g / (1.0f + sagAmt * env);
            // Unity small-signal gain: the stage never just gets louder.
            // Above unity drive, part of the level lost to saturation is made up.
            const float makeup = gEff > 1.0f ? std::pow(gEff, -0.8f) : 1.0f / std::max(1.0e-4f, gEff);
            float* ch[2] = { L + i, R + i };
            float peak = 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float x = *ch[c];
                float a0, a1;
                up[c].process(x * gEff, a0, a1);
                float y = down[c].process(shape(a0), shape(a1)) * makeup;
                dc[c] += aDc * (y - dc[c]);
                y -= dc[c];
                tone[c] += aTone * (y - tone[c]);
                low[c] += aLow * (tone[c] - low[c]);
                y = tone[c] + low[c] * lowBoost;
                peak = std::max(peak, std::abs(y));
                *ch[c] = x + (y - x) * mix;
            }
            env += (peak > env ? sagAttack : sagRelease) * (peak - env);
        }
        lastGain = gain;
        sagEnv = env;
    }

private:
    float fs = 48000.0f;
    float aDc = 0.0f, aLow = 0.0f, sagAttack = 0.0f, sagRelease = 0.0f;
    HalfBandInterpolator up[2];
    HalfBandDecimator down[2];
    float dc[2] {}, tone[2] {}, low[2] {};
    float sagEnv = 0.0f;
    float lastGain = -1.0f;
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
