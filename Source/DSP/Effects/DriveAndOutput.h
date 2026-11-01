#pragma once

// Drive effect and the final output stage.

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"
#include "../Common/HalfBand.h"
#include "ParametricEq.h"

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
/** Valve output stage, run at 2x with half-band resampling, with power-supply
    sag and an output-transformer stage (WARMTH: top roll-off, low weight).
    Each valve type has its own transfer curve, harmonic signature and fixed
    tone voicing:

      12AX7   high-mu preamp triode. A biased tanh: one side of the swing
              compresses well before the other, so the 2nd harmonic leads
              (sweet, singing); the most gain, so it is the first to fuzz.
              Voicing: tight bass, a little presence.
      12AU7   low-mu triode. A very soft algebraic curve with little bias:
              smooth, low-order, mostly 2nd-harmonic thickening and lots of
              headroom (the least gain). Voicing: warm low mids, rounded top.
      EL84    class-AB push-pull pentodes. A hard-kneed pentode curve, the pair
              cancels most even harmonics (a slight imbalance leaves a trace):
              3rd and 5th lead, gritty "chime" that compresses early (most
              sag). Voicing: forward upper mids, leaner bass.
      6L6     big push-pull beam tetrodes. A firmer knee but the most clean
              headroom (low drive range), mild imbalance: stays clean, then
              compresses solidly. Voicing: deep lows, scooped mids, sparkle.

    Every curve is scaled to unity small-signal gain, so DRIVE changes colour
    (and peak compression) rather than loudness. */
class ValveStage
{
public:
    enum class Curve { BiasedTanh, SoftAlgebraic, PushPull };

    struct Model
    {
        float minGainDb, maxGainDb; // input gain across the DRIVE range
        Curve curve;
        float bias;                 // operating-point offset at mid BIAS
        float knee;                 // push-pull: knee hardness (p-norm)
        float imbalance;            // push-pull: gain mismatch of the second valve
        float sagDepth;             // how much the supply sags on loud signals
        float transformerHz;        // top-end roll-off at full WARMTH
        float lowBoostDb;           // low-end weight at full WARMTH
        // Fixed voicing: low shelf, mid bell, high shelf.
        float lowHz, lowDb, midHz, midDb, midQ, highHz, highDb;
    };

    static const Model& model(ValveType t)
    {
        static const Model models[(int) ValveType::Count] = {
            // gain range     curve                 bias  knee imbal sag   xfmr    low   voicing
            { -17.0f, 18.0f, Curve::BiasedTanh,    0.20f, 0.0f, 0.0f,  0.4f, 9000.0f, 2.0f, 90.0f, -1.5f, 2800.0f, 1.5f, 0.8f, 9000.0f, 0.5f },  // 12AX7
            { -15.0f, 10.0f, Curve::SoftAlgebraic, 0.08f, 0.0f, 0.0f,  0.2f, 12000.0f, 2.0f, 250.0f, 1.2f, 1200.0f, 0.0f, 0.7f, 7000.0f, -2.0f }, // 12AU7
            { -7.0f, 14.0f, Curve::PushPull,       0.15f, 4.5f, 0.15f, 0.9f, 7500.0f, 2.0f, 90.0f, -2.0f, 1800.0f, 2.5f, 0.9f, 8000.0f, -1.0f },  // EL84
            { -14.0f, 12.0f, Curve::PushPull,      0.25f, 2.5f, 0.03f, 0.5f, 6500.0f, 3.5f, 110.0f, 2.0f, 600.0f, -2.0f, 0.8f, 6000.0f, 1.5f },  // 6L6
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
        voicedType = -1;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            up[c].reset();
            down[c].reset();
            dc[c] = tone[c] = low[c] = 0.0f;
            for (auto& st : eqState[c])
                st = {};
        }
        sagEnv = 0.0f;
        lastGain = -1.0f;
    }

    /** The type's transfer curve, unity slope at zero. b = operating point. */
    static float curve(const Model& m, float x, float b)
    {
        switch (m.curve)
        {
            case Curve::BiasedTanh:
            {
                const float tb = std::tanh(b);
                return (std::tanh(x + b) - tb) / (1.0f - tb * tb);
            }
            case Curve::SoftAlgebraic:
            {
                auto sa = [](float u) { return u / std::sqrt(1.0f + u * u); };
                return (sa(x + b) - sa(b)) * std::pow(1.0f + b * b, 1.5f);
            }
            case Curve::PushPull:
            default:
            {
                const float p = m.knee;
                auto pent = [p](float u) { return u / std::pow(1.0f + std::pow(std::abs(u), p), 1.0f / p); };
                auto slope = [p](float u) { return std::pow(1.0f + std::pow(std::abs(u), p), -1.0f / p - 1.0f); };
                const float k2 = 1.0f - m.imbalance;
                const float y = 0.5f * ((pent(x + b) - pent(b)) - k2 * (pent(-x + b) - pent(b)));
                return y / (0.5f * (1.0f + k2) * slope(b));
            }
        }
    }

    void process(float* L, float* R, int n, ValveType type, float drive, float bias, float sag, float warmth, float mix)
    {
        const Model& m = model(type);
        if ((int) type != voicedType)
        {
            voicedType = (int) type;
            eq[0] = ParametricEq::design(ParametricEq::Shape::LowShelf, m.lowHz, m.lowDb, 0.7, fs);
            eq[1] = ParametricEq::design(ParametricEq::Shape::Bell, m.midHz, m.midDb, m.midQ, fs);
            eq[2] = ParametricEq::design(ParametricEq::Shape::HighShelf, m.highHz, m.highDb, 0.7, fs);
        }
        // DRIVE taper: most of the travel is warmth and soft peak compression;
        // the gain rises steeply only near the top, so obvious distortion
        // starts at around 90 %.
        const float d = clampf(drive, 0.0f, 1.0f);
        const float gain = dbToGain(m.minGainDb + std::pow(d, 5.0f) * (m.maxGainDb - m.minGainDb));
        if (lastGain < 0.0f)
            lastGain = gain;
        const float gainStep = (gain - lastGain) / (float) std::max(1, n);

        // BIAS moves the operating point: cold = cleaner, more symmetric;
        // hot = more even harmonics (triodes) or more crossover grit (pairs).
        const float b = m.bias * (0.3f + 1.4f * clampf(bias, 0.0f, 1.0f));
        const float sagAmt = clampf(sag, 0.0f, 1.0f) * m.sagDepth * 1.2f;

        const float w = clampf(warmth, 0.0f, 1.0f);
        const float lpHz = std::min(20000.0f * std::pow(m.transformerHz / 20000.0f, w), 0.45f * fs);
        const float aTone = 1.0f - std::exp(-kTwoPi * lpHz / fs);
        const float lowBoost = dbToGain(w * m.lowBoostDb) - 1.0f;

        float g = lastGain;
        float env = sagEnv;
        for (int i = 0; i < n; ++i)
        {
            g += gainStep;
            // Sag: the effective drive drops as the supply is loaded, then recovers.
            const float gEff = g / (1.0f + sagAmt * env);
            // Above unity drive, part of the level lost to saturation is made up.
            const float makeup = gEff > 1.0f ? std::pow(gEff, -0.8f) : 1.0f / std::max(1.0e-4f, gEff);
            float* ch[2] = { L + i, R + i };
            float peak = 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float x = *ch[c];
                float a0, a1;
                up[c].process(x * gEff, a0, a1);
                float y = down[c].process(curve(m, a0, b), curve(m, a1, b)) * makeup;
                dc[c] += aDc * (y - dc[c]);
                y -= dc[c];
                // Fixed voicing of the valve type.
                double v = y;
                for (int k = 0; k < 3; ++k)
                {
                    auto& st = eqState[c][k];
                    const auto& q = eq[k];
                    const double o = q.b0 * v + st.z1;
                    st.z1 = q.b1 * v - q.a1 * o + st.z2;
                    st.z2 = q.b2 * v - q.a2 * o;
                    v = o;
                }
                y = (float) v;
                // Output transformer (WARMTH).
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
    struct EqState { double z1 = 0.0, z2 = 0.0; };
    float fs = 48000.0f;
    float aDc = 0.0f, aLow = 0.0f, sagAttack = 0.0f, sagRelease = 0.0f;
    HalfBandInterpolator up[2];
    HalfBandDecimator down[2];
    float dc[2] {}, tone[2] {}, low[2] {};
    ParametricEq::Coeffs eq[3];
    EqState eqState[2][3];
    int voicedType = -1;
    float sagEnv = 0.0f;
    float lastGain = -1.0f;
};

// -----------------------------------------------------------------------------
/** Audio output transformer, modelled from its physics:

      v_out = v_in - R_s * i_m(phi),   d(phi)/dt = v_out

    phi is the core flux (the integral of the voltage, so it grows as
    frequency falls) and i_m the magnetising current drawn through the source
    resistance. i_m has a linear part (the low-frequency roll-off), a cubic
    part that bends over at the core's saturation flux (low-frequency
    distortion that rises steeply as frequency falls and level rises, mostly
    3rd harmonic, and the "tightening" of the bass when pushed), a small even
    part (remanence: some 2nd harmonic) and a hysteresis loop (the low-level
    distortion and slight bass phase shift of steel cores). Leakage
    inductance and winding capacitance give each its top-end roll-off.

    Level reference: -6 dBFS = +12 dBu. The voicings follow published
    datasheet figures and documented behaviour (LF / HF corners, distortion
    at low frequencies and high level, core material) of each transformer;
    they are models of their measured character, not circuit-exact copies. */
class OutputTransformer
{
public:
    struct Model
    {
        float lfHz;        // -3 dB low-frequency corner (magnetising inductance)
        float thd40;       // THD (%) at 40 Hz, +12 dBu (AMOUNT 50 %)
        float satFlux;     // flux (40 Hz-peak units) where the core saturates hard
        float even;        // 2nd-harmonic share (remanence / DC)
        float hyst;        // hysteresis depth (steel > nickel)
        float top10k, top20k; // response (dB) at 10 and 20 kHz (leakage L / winding C)
        // +HOT voicing (exaggerated): low-frequency resonance with the
        // source / load, a midrange tilt and a stronger top-end tilt.
        float bumpHz, bumpDb, bumpQ, midHz, midDb, hotTop10k, hotTop20k;
    };

    static const Model& model(XfmrType t)
    {
        static const Model models[(int) XfmrType::Count] = {
            //                                                   +HOT: bump Hz/dB/Q      mid Hz/dB      10k/20k
            { 8.0f,  0.30f, 2.2f, 0.15f, 0.00035f, -0.2f, -0.8f, 80.0f, 1.5f, 0.8f,  700.0f, 0.6f,  -0.6f, -2.0f }, // Neve LO1166 (Marinair, steel)
            { 5.0f,  0.15f, 2.8f, 0.05f, 0.00020f, 0.0f,  -0.2f, 120.0f, 1.0f, 0.9f, 3000.0f, 0.9f, -0.2f, -0.8f }, // API 2503
            { 18.0f, 0.50f, 1.8f, 0.10f, 0.00045f, -0.6f, -2.0f, 100.0f, 2.0f, 0.7f, 1500.0f, -0.6f, -1.2f, -4.0f }, // UTC A-24 (tube era)
            { 14.0f, 0.25f, 2.2f, 0.22f, 0.00035f, -0.3f, -1.2f, 60.0f, 2.0f, 0.7f,  400.0f, 0.8f,  -0.8f, -2.8f }, // Haufe V76
            { 1.0f,  0.02f, 7.0f, 0.02f, 0.00004f, 0.0f,  -0.1f, 50.0f, 0.0f, 0.7f, 1000.0f, 0.0f,  0.0f, -0.3f }, // Jensen JT-11 (nickel)
        };
        return models[std::clamp((int) t, 0, (int) XfmrType::Count - 1)];
    }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        lastType = -1;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            flux[c] = play[c] = 0.0f;
            for (auto& st : top[c])
                st = {};
        }
    }

    /** The magnetising-current voltage drop D(phi) and its slope. */
    struct Core
    {
        float lin, a2, a3, cs, satFlux;
        bool soft = false; // +HOT: a 5th-order knee (gradual) instead of 7th (real cores bend late and hard)
        float drop(float phi) const
        {
            const float u = phi / satFlux, u2 = u * u;
            const float sat = soft ? u2 * u2 * u : u2 * u2 * u2 * u;
            return lin * phi + a2 * phi * phi + a3 * phi * phi * phi + cs * satFlux * sat;
        }
        float slope(float phi) const
        {
            const float u = phi / satFlux, u2 = u * u;
            const float sat = soft ? 5.0f * u2 * u2 : 7.0f * u2 * u2 * u2;
            return lin + 2.0f * a2 * phi + 3.0f * a3 * phi * phi + cs * sat;
        }
    };

    void process(float* L, float* R, int n, XfmrType type, float amount, bool hot = false)
    {
        const Model& m = model(type);
        const int key = (int) type * 2 + (hot ? 1 : 0);
        if (key != lastType)
        {
            lastType = key;
            // Top end: two gentle shelves fitted to the response at 10 and 20 kHz;
            // +HOT adds the exaggerated low bump and midrange tilt.
            const float t10 = hot ? m.hotTop10k : m.top10k, t20 = hot ? m.hotTop20k : m.top20k;
            topCoeffs[0] = ParametricEq::design(ParametricEq::Shape::HighShelf, 9000.0, t10, 0.7, fs);
            topCoeffs[1] = ParametricEq::design(ParametricEq::Shape::HighShelf, 17000.0, t20 - t10, 0.7, fs);
            topCoeffs[2] = ParametricEq::design(ParametricEq::Shape::Bell, m.bumpHz, hot ? m.bumpDb : 0.0, m.bumpQ, fs);
            topCoeffs[3] = ParametricEq::design(ParametricEq::Shape::Bell, m.midHz, hot ? m.midDb : 0.0, 0.7, fs);
            stages = hot ? 4 : 2;
        }
        // Flux in units where a 40 Hz sine of amplitude A peaks at A.
        const float k = kTwoPi * 40.0f / fs;
        Core core;
        core.lin = m.lfHz / 40.0f;
        // Cubic term calibrated to the model's THD at 40 Hz, +12 dBu (A = 0.5):
        // THD ~ a3 A^2 / 2 (the flux integrator adds the factor of 2).
        core.a3 = 2.0f * (m.thd40 * 0.01f) / (0.5f * 0.5f);
        core.a2 = m.even * core.a3;
        core.cs = hot ? 0.05f : 0.02f; // saturation around satFlux
        core.soft = hot;
        core.satFlux = m.satFlux;
        const float hw = 0.02f;   // hysteresis loop half-width (flux units)
        const float hk = m.hyst / hw;
        // Real: -12 .. +18 dB around nominal (+12 dBu at -6 dBFS).
        // +HOT: nominal .. +36 dB, where bass and low mids saturate and compress.
        const float a = clampf(amount, 0.0f, 1.0f);
        const float drive = dbToGain(hot ? 36.0f * std::pow(a, 1.1f) : -12.0f + 30.0f * a);
        const float makeup = 1.0f / drive;

        float* io[2] = { L, R };
        for (int c = 0; c < 2; ++c)
        {
            float phi = flux[c], p = play[c];
            for (int i = 0; i < n; ++i)
            {
                const float x = io[c][i] * drive;
                // Backward-Euler step for the flux (one Newton iteration): stable
                // however hard the core is driven.
                p = std::clamp(p, phi - hw, phi + hw); // play-operator hysteresis
                const float hystDrop = hk * (phi - p);
                const float f = k * (core.drop(phi) + hystDrop - x);
                const float next = phi - f / (1.0f + k * core.slope(phi));
                float y = (next - phi) / k;
                phi = undenormalise(next);
                for (int s2 = 0; s2 < stages; ++s2)
                {
                    auto& st = top[c][s2];
                    const auto& q = topCoeffs[s2];
                    const double o = q.b0 * y + st.z1;
                    st.z1 = q.b1 * y - q.a1 * o + st.z2;
                    st.z2 = q.b2 * y - q.a2 * o;
                    y = (float) o;
                }
                io[c][i] = y * makeup;
            }
            flux[c] = phi;
            play[c] = p;
        }
    }

private:
    struct State { double z1 = 0.0, z2 = 0.0; };
    float fs = 48000.0f;
    int lastType = -1;
    ParametricEq::Coeffs topCoeffs[4];
    int stages = 2;
    float flux[2] {}, play[2] {};
    State top[2][4];
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
