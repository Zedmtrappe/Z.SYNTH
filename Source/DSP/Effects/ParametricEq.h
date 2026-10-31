#pragma once

// Four-band parametric EQ (low shelf, two bells, high shelf) for the reverb's
// wet signal. RBJ biquads, transposed direct form II; coefficients are only
// recomputed when a setting changes. The same design functions drive the
// on-screen curve, so what is drawn is exactly what is heard.

#include "../Common/DspCommon.h"

#include <array>
#include <complex>

namespace zsynth::dsp
{

struct EqSettings
{
    float lowFreq = 120.0f, lowGain = 0.0f;
    float bell1Freq = 400.0f, bell1Gain = 0.0f, bell1Q = 1.0f;
    float bell2Freq = 2500.0f, bell2Gain = 0.0f, bell2Q = 1.0f;
    float highFreq = 8000.0f, highGain = 0.0f;

    bool isFlat() const
    {
        return std::abs(lowGain) < 0.01f && std::abs(bell1Gain) < 0.01f && std::abs(bell2Gain) < 0.01f && std::abs(highGain) < 0.01f;
    }
    bool operator==(const EqSettings& o) const
    {
        return lowFreq == o.lowFreq && lowGain == o.lowGain && bell1Freq == o.bell1Freq && bell1Gain == o.bell1Gain
            && bell1Q == o.bell1Q && bell2Freq == o.bell2Freq && bell2Gain == o.bell2Gain && bell2Q == o.bell2Q
            && highFreq == o.highFreq && highGain == o.highGain;
    }
};

class ParametricEq
{
public:
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    enum class Shape { LowShelf, Bell, HighShelf };

    static Coeffs design(Shape shape, double freq, double gainDb, double q, double fs)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * std::min(freq, 0.45 * fs) / fs;
        const double cw = std::cos(w0), sw = std::sin(w0);
        Coeffs c;
        double a0;
        if (shape == Shape::Bell)
        {
            const double alpha = sw / (2.0 * q);
            c.b0 = 1.0 + alpha * A; c.b1 = -2.0 * cw; c.b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A; c.a1 = -2.0 * cw; c.a2 = 1.0 - alpha / A;
        }
        else
        {
            // Shelf slope 0.8: a gentle, musical shelf.
            const double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / 0.8 - 1.0) + 2.0);
            const double s = shape == Shape::LowShelf ? -1.0 : 1.0;
            const double k = 2.0 * std::sqrt(A) * alpha;
            c.b0 = A * ((A + 1) + s * (A - 1) * cw + k);
            c.b1 = -2.0 * s * A * ((A - 1) + s * (A + 1) * cw);
            c.b2 = A * ((A + 1) + s * (A - 1) * cw - k);
            a0 = (A + 1) - s * (A - 1) * cw + k;
            c.a1 = 2.0 * s * ((A - 1) - s * (A + 1) * cw);
            c.a2 = (A + 1) - s * (A - 1) * cw - k;
        }
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0; c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    static std::array<Coeffs, 4> designAll(const EqSettings& s, double fs)
    {
        return { design(Shape::LowShelf, s.lowFreq, s.lowGain, 0.7, fs),
                 design(Shape::Bell, s.bell1Freq, s.bell1Gain, s.bell1Q, fs),
                 design(Shape::Bell, s.bell2Freq, s.bell2Gain, s.bell2Q, fs),
                 design(Shape::HighShelf, s.highFreq, s.highGain, 0.7, fs) };
    }

    /** Response of the whole EQ at `freq`, in dB (used by the curve display). */
    static double magnitudeDb(const EqSettings& s, double freq, double fs)
    {
        const double w = 2.0 * 3.14159265358979323846 * freq / fs;
        const std::complex<double> z1 = std::polar(1.0, -w), z2 = z1 * z1;
        double db = 0.0;
        for (const auto& c : designAll(s, fs))
        {
            const auto h = (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
            db += 20.0 * std::log10(std::max(1.0e-9, std::abs(h)));
        }
        return db;
    }

    void prepare(float sampleRate) { fs = sampleRate; reset(); valid = false; }
    void reset() { for (auto& st : state) st = {}; }

    /** Call once per block; cheap when nothing changed. */
    void setSettings(const EqSettings& s)
    {
        if (valid && s == current)
            return;
        current = s;
        valid = true;
        flat = s.isFlat();
        coeffs = designAll(s, fs);
    }

    bool isFlat() const { return flat; }

    void process(float& l, float& r)
    {
        if (flat)
            return;
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            double x = *io[c];
            for (int b = 0; b < 4; ++b)
            {
                auto& st = state[(size_t) (c * 4 + b)];
                const auto& k = coeffs[(size_t) b];
                const double y = k.b0 * x + st.z1;
                st.z1 = k.b1 * x - k.a1 * y + st.z2;
                st.z2 = k.b2 * x - k.a2 * y;
                x = y;
            }
            *io[c] = (float) x;
        }
    }

private:
    struct State { double z1 = 0, z2 = 0; };
    float fs = 48000.0f;
    EqSettings current;
    bool valid = false, flat = true;
    std::array<Coeffs, 4> coeffs {};
    std::array<State, 8> state {};
};

} // namespace zsynth::dsp
