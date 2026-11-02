#pragma once

// Classic hardware EQs for the output EQ slot (the PRISTINE parametric is
// OutputEq). Each model reproduces its unit's control set (stepped switches
// where the hardware has them), the curve shapes of its filter network and the
// colour of its amplifier and transformers:
//
//   PULTEC EQP-1A  Passive LC network + valve make-up amp. LOW boost is a
//                  resonant shelf concentrated below the selected frequency;
//                  LOW atten a broad first-order shelf reaching higher, so
//                  boost + atten together give the famous sub lift with a
//                  low-mid dip. HIGH boost: resonant peak, BANDWIDTH sharp ->
//                  broad. HIGH atten: shelf at 5 / 10 / 20 kHz. Gentle, mostly
//                  2nd-harmonic valve colour.
//   NEVE 1073      HPF 50/80/160/300 Hz at 18 dB/oct; LOW shelf 35-220 Hz with
//                  the inductor's slight overshoot; MID 360 Hz - 7.2 kHz bell
//                  (fixed, fairly broad); HIGH 12 kHz gentle shelf. Class-A
//                  amp + transformers: 2nd and 3rd harmonics, thickening lows.
//   API 550A/550B  Stepped "proportional Q" bands (narrower as you boost or
//                  cut more), PEAK/SHELF on the outer bands; 550A has the
//                  BAND-PASS filter (50 Hz - 15 kHz). 2520 op-amp + output
//                  transformer: clean until pushed, then punchy odd harmonics.
//   API 560        10-band graphic, 31 Hz - 16 kHz, proportional Q.
//
// Everything runs at twice the host rate (half-band up/down) so curves near
// 20 kHz are not cramped and the saturation does not alias. The curve display
// uses magnitudeDb(), the same designs that are heard.

#include "../Common/HalfBand.h"
#include "../../Parameters/ParamChoices.h"
#include "ParametricEq.h"

#include <array>
#include <cmath>
#include <complex>

namespace zsynth::dsp
{

struct VintageEqSettings
{
    EqModel model = EqModel::Pristine;
    // Pultec EQP-1A (knobs 0..10, frequency indices into the choice lists).
    int pulLowFreq = 2, pulHighFreq = 3, pulAttenFreq = 1;
    float pulLowBoost = 0, pulLowAtten = 0, pulHighBoost = 0, pulBandwidth = 5, pulHighAtten = 0;
    // Neve 1073.
    int neveHpf = 0, neveLowFreq = 2, neveMidFreq = 2;
    float neveLowGain = 0, neveMidGain = 0, neveHighGain = 0;
    // API 550A: bands low / mid / high; gains in dB (already stepped).
    int a550Freq[3] { 1, 2, 2 };
    float a550Gain[3] {};
    bool a550Shelf[2] {}, a550Bandpass = false;
    // API 550B: four bands.
    int b550Freq[4] { 2, 4, 2, 3 };
    float b550Gain[4] {};
    bool b550Shelf[2] {};
    // API 560.
    float g560[10] {};
    // Colour and level.
    float drive = 0.0f;    // 0 = nominal level .. 1 = +24 dB into the amplifier (level-compensated)
    float outputDb = 0.0f;

    bool operator==(const VintageEqSettings& o) const
    {
        auto same = [](const auto& a, const auto& b) { return std::equal(std::begin(a), std::end(a), std::begin(b)); };
        return model == o.model && pulLowFreq == o.pulLowFreq && pulHighFreq == o.pulHighFreq && pulAttenFreq == o.pulAttenFreq
            && pulLowBoost == o.pulLowBoost && pulLowAtten == o.pulLowAtten && pulHighBoost == o.pulHighBoost
            && pulBandwidth == o.pulBandwidth && pulHighAtten == o.pulHighAtten && neveHpf == o.neveHpf
            && neveLowFreq == o.neveLowFreq && neveMidFreq == o.neveMidFreq && neveLowGain == o.neveLowGain
            && neveMidGain == o.neveMidGain && neveHighGain == o.neveHighGain && same(a550Freq, o.a550Freq)
            && same(a550Gain, o.a550Gain) && same(a550Shelf, o.a550Shelf) && a550Bandpass == o.a550Bandpass
            && same(b550Freq, o.b550Freq) && same(b550Gain, o.b550Gain) && same(b550Shelf, o.b550Shelf)
            && same(g560, o.g560) && drive == o.drive && outputDb == o.outputDb;
    }
};

class VintageEq
{
public:
    using Coeffs = ParametricEq::Coeffs;
    static constexpr int kMaxStages = 12;

    // Hardware switch positions.
    static constexpr float kPulLow[4] = { 20, 30, 60, 100 };
    static constexpr float kPulHigh[7] = { 3000, 4000, 5000, 8000, 10000, 12000, 16000 };
    static constexpr float kPulAtten[3] = { 5000, 10000, 20000 };
    static constexpr float kNeveHpf[5] = { 0, 50, 80, 160, 300 };
    static constexpr float kNeveLow[5] = { 0, 35, 60, 110, 220 };
    static constexpr float kNeveMid[6] = { 360, 700, 1600, 3200, 4800, 7200 };
    static constexpr float kA550[3][5] = { { 50, 100, 200, 300, 400 }, { 400, 800, 1500, 3000, 5000 }, { 2500, 5000, 7500, 10000, 12500 } };
    static constexpr float kB550[4][7] = { { 30, 40, 50, 100, 200, 300, 400 }, { 75, 150, 180, 240, 500, 700, 1000 },
                                           { 800, 1500, 3000, 5000, 8000, 10000, 12500 }, { 2500, 5000, 7000, 10000, 12500, 15000, 20000 } };
    static constexpr float k560[10] = { 31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    static constexpr float kA550Steps[11] = { -12, -9, -6, -4, -2, 0, 2, 4, 6, 9, 12 };
    static constexpr float kB550Steps[15] = { -15, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, 15 };

    // ---------------------------------------------------------- filter design
    static Coeffs bell(double f, double gainDb, double q, double fs)
    {
        return ParametricEq::design(ParametricEq::Shape::Bell, f, gainDb, q, fs);
    }

    /** Second-order shelf with an explicit Q (Q > 0.707 = the overshoot of inductor shelves). */
    static Coeffs shelf(bool high, double f, double gainDb, double q, double fs)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * std::min(f, 0.45 * fs) / fs;
        const double cw = std::cos(w0), alpha = std::sin(w0) / (2.0 * q);
        const double s = high ? 1.0 : -1.0, k = 2.0 * std::sqrt(A) * alpha;
        Coeffs c;
        c.b0 = A * ((A + 1) + s * (A - 1) * cw + k);
        c.b1 = -2.0 * s * A * ((A - 1) + s * (A + 1) * cw);
        c.b2 = A * ((A + 1) + s * (A - 1) * cw - k);
        const double a0 = (A + 1) - s * (A - 1) * cw + k;
        c.a1 = 2.0 * s * ((A - 1) - s * (A + 1) * cw);
        c.a2 = (A + 1) - s * (A - 1) * cw - k;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0; c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    /** First-order shelf (a broad, gentle passive slope). `f` = the half-gain point. */
    static Coeffs shelf1(bool high, double f, double gainDb, double fs)
    {
        const double G = std::pow(10.0, gainDb / 20.0);
        const double sq = std::sqrt(G);
        // Analogue H(s) = (s + w*sq) / (s + w/sq) for a low shelf of gain G (high: mirrored), bilinear.
        const double K = std::tan(3.14159265358979323846 * std::min(f, 0.45 * fs) / fs);
        Coeffs c;
        if (! high)
        {
            const double a0 = 1.0 + K / sq;
            c.b0 = (1.0 + K * sq) / a0; c.b1 = (K * sq - 1.0) / a0; c.a1 = (K / sq - 1.0) / a0;
        }
        else
        {
            const double a0 = K + 1.0 / sq;
            c.b0 = (K + sq) / a0; c.b1 = (K - sq) / a0; c.a1 = (K - 1.0 / sq) / a0;
        }
        c.b2 = 0.0; c.a2 = 0.0;
        return c;
    }

    static Coeffs pass(bool high, double f, double q, double fs)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * std::min(f, 0.45 * fs) / fs;
        const double cw = std::cos(w0), alpha = std::sin(w0) / (2.0 * q), sign = high ? -1.0 : 1.0;
        Coeffs c;
        c.b0 = (1.0 - sign * cw) * 0.5; c.b1 = sign * (1.0 - sign * cw); c.b2 = c.b0;
        const double a0 = 1.0 + alpha;
        c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        return c;
    }

    static Coeffs pass1(bool high, double f, double fs)
    {
        const double K = std::tan(3.14159265358979323846 * std::min(f, 0.45 * fs) / fs);
        Coeffs c;
        const double a0 = K + 1.0;
        if (high) { c.b0 = 1.0 / a0; c.b1 = -1.0 / a0; }
        else      { c.b0 = K / a0;  c.b1 = K / a0; }
        c.a1 = (K - 1.0) / a0; c.b2 = c.a2 = 0.0;
        return c;
    }

    /** API proportional Q: broad at small settings, narrower as the gain grows. */
    static double apiQ(double gainDb) { return 0.45 + 0.11 * std::abs(gainDb); }
    static double apiShelfQ(double gainDb) { return 0.55 + 0.035 * std::abs(gainDb); }
    /** Pultec knob (0..10) to dB: the pots are not linear in dB. */
    static double pulDb(double knob, double maxDb) { return maxDb * std::pow(std::clamp(knob, 0.0, 10.0) / 10.0, 1.25); }

    /** The filter network for the settings at rate `fs`; returns the number of stages. */
    static int design(const VintageEqSettings& s, double fs, Coeffs* c)
    {
        int n = 0;
        auto add = [&](const Coeffs& k) { if (n < kMaxStages) c[n++] = k; };
        switch (s.model)
        {
            case EqModel::Pultec:
            {
                const double lf = kPulLow[std::clamp(s.pulLowFreq, 0, 3)];
                if (s.pulLowBoost > 0.0f)
                    add(shelf(false, lf * 1.1, pulDb(s.pulLowBoost, 13.5), 1.05, fs));
                if (s.pulLowAtten > 0.0f)
                {
                    // The attenuation reaches higher and more gently than the boost: a
                    // broad first-order shelf plus a wide dip in the low mids.
                    const double d = pulDb(s.pulLowAtten, 17.5);
                    add(shelf1(false, lf * 1.6, -0.7 * d, fs));
                    add(bell(lf * 3.2, -0.35 * d, 0.6, fs));
                }
                if (s.pulHighBoost > 0.0f)
                {
                    const double q = 2.2 * std::pow(0.15, std::clamp((double) s.pulBandwidth, 0.0, 10.0) / 10.0); // sharp 2.2 .. broad 0.33
                    add(bell(kPulHigh[std::clamp(s.pulHighFreq, 0, 6)], pulDb(s.pulHighBoost, 16.0), q, fs));
                }
                if (s.pulHighAtten > 0.0f)
                    add(shelf(true, kPulAtten[std::clamp(s.pulAttenFreq, 0, 2)] * 0.7, -pulDb(s.pulHighAtten, 16.0), 0.55, fs));
                // Transformers and the amp's response: a touch of LF and HF droop.
                add(pass1(true, 8.0, fs));
                add(pass1(false, 42000.0, fs));
                break;
            }
            case EqModel::Neve1073:
            {
                const int hp = std::clamp(s.neveHpf, 0, 4);
                if (hp > 0)
                {
                    // 18 dB/oct: one first-order and one second-order (Q 1) section (3rd-order Butterworth).
                    add(pass1(true, kNeveHpf[hp], fs));
                    add(pass(true, kNeveHpf[hp], 1.0, fs));
                }
                const int lo = std::clamp(s.neveLowFreq, 0, 4);
                if (lo > 0 && std::abs(s.neveLowGain) > 0.01f)
                    add(shelf(false, kNeveLow[lo], s.neveLowGain, 0.95, fs));
                if (std::abs(s.neveMidGain) > 0.01f)
                    add(bell(kNeveMid[std::clamp(s.neveMidFreq, 0, 5)], s.neveMidGain, 1.0, fs));
                if (std::abs(s.neveHighGain) > 0.01f)
                    add(shelf(true, 12000.0, s.neveHighGain, 0.5, fs));
                add(pass1(true, 12.0, fs));
                add(pass1(false, 38000.0, fs));
                break;
            }
            case EqModel::Api550A:
            {
                for (int b = 0; b < 3; ++b)
                {
                    const double g = s.a550Gain[b];
                    if (std::abs(g) < 0.01)
                        continue;
                    const double f = kA550[b][std::clamp(s.a550Freq[b], 0, 4)];
                    const bool isShelf = (b == 0 && s.a550Shelf[0]) || (b == 2 && s.a550Shelf[1]);
                    add(isShelf ? shelf(b == 2, f, g, apiShelfQ(g), fs) : bell(f, g, apiQ(g), fs));
                }
                if (s.a550Bandpass)
                {
                    add(pass(true, 50.0, 0.707, fs));
                    add(pass(false, 15000.0, 0.707, fs));
                }
                add(pass1(true, 6.0, fs));
                break;
            }
            case EqModel::Api550B:
            {
                for (int b = 0; b < 4; ++b)
                {
                    const double g = s.b550Gain[b];
                    if (std::abs(g) < 0.01)
                        continue;
                    const double f = kB550[b][std::clamp(s.b550Freq[b], 0, 6)];
                    const bool isShelf = (b == 0 && s.b550Shelf[0]) || (b == 3 && s.b550Shelf[1]);
                    add(isShelf ? shelf(b == 3, f, g, apiShelfQ(g), fs) : bell(f, g, apiQ(g), fs));
                }
                add(pass1(true, 6.0, fs));
                break;
            }
            case EqModel::Api560:
            {
                // Ten bands; the gentle LF/HF trims share the last two stages.
                for (int b = 0; b < 10; ++b)
                    if (std::abs(s.g560[b]) > 0.01f)
                        add(bell(k560[b], s.g560[b], 0.5 + 0.12 * std::abs(s.g560[b]), fs));
                break;
            }
            default:
                break;
        }
        return n;
    }

    /** Response at `freq` in dB (filters only, without drive or trim). `fs` = the host rate. */
    static double magnitudeDb(const VintageEqSettings& s, double freq, double fs)
    {
        std::array<Coeffs, kMaxStages> c {};
        const int n = design(s, fs * 2.0, c.data());
        const double w = 2.0 * 3.14159265358979323846 * freq / (fs * 2.0);
        const std::complex<double> z1 = std::polar(1.0, -w), z2 = z1 * z1;
        double db = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto& k = c[(size_t) i];
            const auto h = (k.b0 + k.b1 * z1 + k.b2 * z2) / (1.0 + k.a1 * z1 + k.a2 * z2);
            db += 20.0 * std::log10(std::max(1.0e-9, std::abs(h)));
        }
        return db;
    }

    // -------------------------------------------------------------- amplifier
    struct Amp { float headroom, bias, hardness; };
    /** headroom: input-referred clip level at drive 0; bias: even harmonics; hardness: knee (0 soft .. 1 hard). */
    static Amp ampFor(EqModel m)
    {
        switch (m)
        {
            case EqModel::Pultec:   return { 6.0f, 0.17f, 0.0f };  // 12AX7 / 12AU7 make-up amp: soft, 2nd harmonic
            case EqModel::Neve1073: return { 3.0f, 0.15f, 0.25f }; // BA283 class A: 2nd + 3rd, thick
            default:                return { 3.5f, 0.03f, 0.75f }; // 2520 op-amp: clean, then hard and odd
        }
    }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        valid = false;
        reset();
    }

    void reset()
    {
        for (auto& st : state)
            st = {};
        for (int c = 0; c < 2; ++c)
        {
            up[c].reset();
            down[c].reset();
        }
    }

    void process(float* L, float* R, int n, const VintageEqSettings& s)
    {
        if (! valid || ! (s == current))
        {
            if (! valid || s.model != current.model)
                reset();
            current = s;
            valid = true;
            stages = design(s, fs * 2.0, coeffs.data());
            amp = ampFor(s.model);
            gain = std::pow(10.0f, s.outputDb / 20.0f);
            drive = std::pow(10.0f, 24.0f * std::clamp(s.drive, 0.0f, 1.0f) / 20.0f);
            p = 2.0f + 6.0f * amp.hardness;
            biasOut = sat(amp.bias);
            // Unity small-signal gain through the amplifier.
            slope = (sat(amp.bias + 1.0e-3f) - biasOut) / 1.0e-3f;
        }
        float* io[2] = { L, R };
        const float pre = drive / amp.headroom, post = amp.headroom / (drive * slope);
        for (int c = 0; c < 2; ++c)
        {
            auto* st = &state[(size_t) (c * kMaxStages)];
            for (int i = 0; i < n; ++i)
            {
                float a, b;
                up[c].process(io[c][i], a, b);
                a = tick(a, st, pre, post);
                b = tick(b, st, pre, post);
                io[c][i] = down[c].process(a, b) * gain;
            }
        }
    }

private:
    struct State { double z1 = 0, z2 = 0; };

    float sat(float u) const
    {
        // Soft (algebraic) to hard knee; slope 1 at 0.
        return u / std::pow(1.0f + std::pow(std::abs(u), p), 1.0f / p);
    }

    float tick(float x, State* st, float pre, float post)
    {
        double y = x;
        for (int k = 0; k < stages; ++k)
        {
            const auto& q = coeffs[(size_t) k];
            const double o = q.b0 * y + st[k].z1;
            st[k].z1 = q.b1 * y - q.a1 * o + st[k].z2;
            st[k].z2 = q.b2 * y - q.a2 * o;
            y = o;
        }
        // The amplifier after the network (make-up gain stage), level-compensated.
        return (sat((float) y * pre + amp.bias) - biasOut) * post;
    }

    float fs = 48000.0f;
    VintageEqSettings current;
    bool valid = false;
    int stages = 0;
    std::array<Coeffs, kMaxStages> coeffs {};
    std::array<State, 2 * kMaxStages> state {};
    HalfBandInterpolator up[2];
    HalfBandDecimator down[2];
    Amp amp { 3.5f, 0.0f, 0.5f };
    float gain = 1.0f, drive = 1.0f, p = 2.0f, biasOut = 0.0f, slope = 1.0f;
};

} // namespace zsynth::dsp
