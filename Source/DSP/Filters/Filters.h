#pragma once

// Z.SYNTH filter models. All are zero-delay-feedback (topology-preserving
// transform) designs, take the prewarped coefficient g = tan(pi * fc / fs) per
// sample (so cutoff can be modulated at audio rate), and contain explicit
// nonlinearities where those shape the character. They are *inspired by*
// classic circuits; none claims to be a validated circuit model.

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"

namespace zsynth::dsp
{

/** Trapezoidal one-pole low-pass state (shared building block). */
struct OnePoleTpt
{
    float s = 0.0f;

    float lowpass(float x, float G)
    {
        const float v = (x - s) * G;
        const float y = v + s;
        s = undenormalise(y + v);
        return y;
    }
    float highpass(float x, float G) { return x - lowpass(x, G); }
};

// -----------------------------------------------------------------------------
/** Four-stage transistor ladder: every stage is a differential pair, modelled as
        dy_i/dt = wc * (tanh(x_i) - tanh(y_i)).
    Each tanh is replaced by v * T(v), T = tanh(v)/v evaluated at the previous
    sample's value, which turns the stage into a linear one-pole solvable in
    closed form together with the global feedback (after Mystran's "cheap
    non-linear zero-delay filters"). Resonance removes bass as on the original
    topology; partial output compensation keeps it musical. Responses other
    than low-pass are obtained by mixing pole outputs (Oberheim Xpander style). */
class LadderFilter
{
public:
    void reset() { for (auto& v : s) v = 0.0f; for (auto& v : est) v = 0.0f; }

    float process(float in, float g, float k, FilterType type, FilterSlope slope)
    {
        const float r0 = tanhRatio(est[0]);
        const float r1 = tanhRatio(est[1]);
        const float r2 = tanhRatio(est[2]);
        const float r3 = tanhRatio(est[3]);
        const float r4 = tanhRatio(est[4]);

        const float d1 = 1.0f / (1.0f + g * r1), d2 = 1.0f / (1.0f + g * r2);
        const float d3 = 1.0f / (1.0f + g * r3), d4 = 1.0f / (1.0f + g * r4);
        const float a1 = g * r0 * d1, a2 = g * r1 * d2, a3 = g * r2 * d3, a4 = g * r3 * d4;
        const float b1 = s[0] * d1, b2 = s[1] * d2, b3 = s[2] * d3, b4 = s[3] * d4;

        const float A = a1 * a2 * a3 * a4;
        const float B = ((b1 * a2 + b2) * a3 + b3) * a4 + b4;

        float y4 = (A * in + B) / (1.0f + k * A);
        const float x1 = in - k * y4;
        const float y1 = a1 * x1 + b1;
        const float y2 = a2 * y1 + b2;
        const float y3 = a3 * y2 + b3;
        y4 = a4 * y3 + b4;

        s[0] = undenormalise(2.0f * y1 - s[0]);
        s[1] = undenormalise(2.0f * y2 - s[1]);
        s[2] = undenormalise(2.0f * y3 - s[2]);
        s[3] = undenormalise(2.0f * y4 - s[3]);
        est[0] = x1; est[1] = y1; est[2] = y2; est[3] = y3; est[4] = y4;

        const float comp = 1.0f + 0.35f * k;
        return comp * mixPoles(x1, y1, y2, y3, y4, type, slope);
    }

    static float mixPoles(float x, float y1, float y2, float y3, float y4, FilterType type, FilterSlope slope)
    {
        const bool four = slope == FilterSlope::Pole4;
        switch (type)
        {
            case FilterType::LowPass:  return four ? y4 : y2;
            case FilterType::BandPass: return four ? 4.0f * (y2 - 2.0f * y3 + y4) : 2.0f * (y1 - y2);
            case FilterType::HighPass: return four ? x - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4 : x - 2.0f * y1 + y2;
            case FilterType::Notch:    return four ? x - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + 2.0f * y4 : x - 2.0f * y1 + 2.0f * y2;
            default: return y4;
        }
    }

private:
    float s[4] {};
    float est[5] {};
};

// -----------------------------------------------------------------------------
/** OTA cascade (in the spirit of the integrated 4-pole chips found in many
    polysynths): four *linear* one-poles, with the nonlinearity concentrated in
    the input stage and the resonance feedback path. Resonance is gain
    compensated so the low end stays solid, which together with the cleaner
    stages gives a brighter, punchier character than the ladder. */
class OtaFilter
{
public:
    void reset() { for (auto& p : poles) p.s = 0.0f; lastY4 = 0.0f; }

    float process(float in, float g, float k, FilterType type, FilterSlope slope)
    {
        const float G = g / (1.0f + g);
        const float invOnePlusG = 1.0f / (1.0f + g);

        // Feedback soft-limiter linearised at the previous output.
        const float kEff = k * tanhRatio(lastY4 * 0.7f);

        const float b1 = poles[0].s * invOnePlusG, b2 = poles[1].s * invOnePlusG;
        const float b3 = poles[2].s * invOnePlusG, b4 = poles[3].s * invOnePlusG;
        const float G2 = G * G;
        const float S = G2 * G * b1 + G2 * b2 + G * b3 + b4;

        const float compensated = in * (1.0f + 0.5f * k);
        float x = (compensated - kEff * S) / (1.0f + kEff * G2 * G2);
        x = fastTanh(x);

        const float y1 = poles[0].lowpass(x, G);
        const float y2 = poles[1].lowpass(y1, G);
        const float y3 = poles[2].lowpass(y2, G);
        const float y4 = poles[3].lowpass(y3, G);
        lastY4 = y4;

        return LadderFilter::mixPoles(x, y1, y2, y3, y4, type, slope);
    }

private:
    OnePoleTpt poles[4];
    float lastY4 = 0.0f;
};

// -----------------------------------------------------------------------------
/** State-variable filter (Simper/Zavalishin TPT form) with a gently saturating
    band-pass integrator, giving the smooth, open, 12 dB multimode character of
    the classic SEM-style module. The 24 dB slope cascades a fixed Butterworth
    section with the resonant one. Resonance stops just short of
    self-oscillation, as on the original design. */
class SemFilter
{
public:
    void reset() { a = {}; b = {}; }

    float process(float in, float g, float res, FilterType type, FilterSlope slope)
    {
        const float k = 2.0f - 1.96f * res;
        if (slope == FilterSlope::Pole4)
        {
            const float first = a.process(in, g, 1.8478f, type, false);
            // Butterworth Q for the second section at zero resonance, rising with res.
            return b.process(first, g, 0.7654f * (1.0f - 0.97f * res), type, true);
        }
        return a.process(in, g, k, type, true);
    }

private:
    struct Section
    {
        float ic1 = 0.0f, ic2 = 0.0f;

        float process(float v0, float g, float k, FilterType type, bool saturate)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = v0 - ic2;
            float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            if (saturate)
                v1 = 1.6f * fastTanh(v1 * 0.625f);
            ic1 = undenormalise(2.0f * v1 - ic1);
            ic2 = undenormalise(2.0f * v2 - ic2);

            switch (type)
            {
                case FilterType::LowPass:  return v2;
                case FilterType::BandPass: return v1 * k;
                case FilterType::HighPass: return v0 - k * v1 - v2;
                case FilterType::Notch:    return v0 - k * v1;
                default: return v2;
            }
        }
    };

    Section a, b;
};

// -----------------------------------------------------------------------------
/** Korg-35-style Sallen-Key filter (after Will Pirkle's TPT formulation) with a
    hard-driven tanh in the feedback loop: the aggressive, screaming 12 dB
    character of the famous semi-modular monosynth. LP and HP are the native
    topologies; BP and notch are derived (LP followed by a one-pole HP, and
    input minus BP). Self-oscillates at maximum resonance. */
class K35Filter
{
public:
    void reset() { lp1 = {}; lp2 = {}; hp3 = {}; hp1 = {}; hp2 = {}; lp3 = {}; bpHp = {}; }

    float process(float in, float g, float res, FilterType type)
    {
        const float K = 0.01f + 2.09f * res; // self-oscillates above K = 2
        const float G = g / (1.0f + g);
        const float alpha0 = 1.0f / (1.0f - K * G + K * G * G);
        const float sat = 1.4f;

        if (type == FilterType::HighPass)
        {
            const float y1 = hp1.highpass(in, G);
            const float s35 = (-G / (1.0f + g)) * hp2.s + (1.0f / (1.0f + g)) * lp3.s;
            float y = K * alpha0 * (y1 + s35);
            y = fastTanh(sat * y) / sat;
            lp3.lowpass(hp2.highpass(y, G), G);
            return y / K;
        }

        const float y1 = lp1.lowpass(in, G);
        const float s35 = (-1.0f / (1.0f + g)) * hp3.s + ((K - K * G) / (1.0f + g)) * lp2.s;
        float u = alpha0 * (y1 + s35);
        u = fastTanh(sat * u) / sat;
        const float y = K * lp2.lowpass(u, G);
        hp3.highpass(y, G);
        const float lp = y / K;

        if (type == FilterType::LowPass)
            return lp;
        const float bp = bpHp.highpass(lp, G) * 2.0f;
        return type == FilterType::BandPass ? bp : in - bp;
    }

private:
    OnePoleTpt lp1, lp2, hp3, hp1, hp2, lp3, bpHp;
};

// -----------------------------------------------------------------------------
/** Non-resonant 12 dB Butterworth high-pass (TPT SVF), used before the main filter. */
class HighPass12
{
public:
    void reset() { ic1 = ic2 = 0.0f; }

    float process(float v0, float g)
    {
        constexpr float k = 1.41421356f;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = undenormalise(2.0f * v1 - ic1);
        ic2 = undenormalise(2.0f * v2 - ic2);
        return v0 - k * v1 - v2;
    }

private:
    float ic1 = 0.0f, ic2 = 0.0f;
};

// -----------------------------------------------------------------------------
/** Per-voice filter: owns one instance of each model and dispatches to the
    selected one. Switching model resets the newly selected model's state. */
class FilterUnit
{
public:
    void reset()
    {
        ladder.reset(); ota.reset(); sem.reset(); k35.reset();
    }

    void setModel(FilterModel m)
    {
        if (m != model)
        {
            model = m;
            reset();
        }
    }

    FilterModel getModel() const { return model; }

    /** g: prewarped cutoff; res 0..1; drive 0..1. */
    float process(float in, float g, float res, float drive, FilterType type, FilterSlope slope)
    {
        const float driveGain = 1.0f + drive * drive * 7.0f;
        const float x = in * driveGain;
        const float makeup = 1.0f / std::sqrt(driveGain);

        float y;
        switch (model)
        {
            case FilterModel::Ladder: y = ladder.process(x, g, res * 4.1f, type, slope); break;
            case FilterModel::Ota:    y = ota.process(x, g, res * 4.25f, type, slope); break;
            case FilterModel::Sem:    y = sem.process(x, g, res, type, slope); break;
            case FilterModel::K35:    y = k35.process(x, g, res, type); break;
            default: y = x; break;
        }
        return y * makeup;
    }

private:
    FilterModel model = FilterModel::Ladder;
    LadderFilter ladder;
    OtaFilter ota;
    SemFilter sem;
    K35Filter k35;
};

/** Prewarped TPT coefficient for a cutoff in Hz at sample rate fs (clamped). */
inline float prewarp(float cutoffHz, float fs)
{
    const float fc = clampf(cutoffHz, 5.0f, 0.47f * fs);
    return std::tan(kPi * fc / fs);
}

} // namespace zsynth::dsp
