#pragma once

// Parametric output EQ: 12 dB/oct low cut, five bell bands with variable Q,
// 12 dB/oct high cut and an output trim. Same RBJ designs as the reverb EQ
// (ParametricEq), so the on-screen curve is exactly what is heard. With every
// band at 0 dB, both cuts off and the trim at 0 dB it is bypassed exactly.

#include "ParametricEq.h"

namespace zsynth::dsp
{

struct OutputEqSettings
{
    static constexpr int kBands = 5;
    float hpf = 20.0f, lpf = 20000.0f;
    float freq[kBands] { 80.0f, 250.0f, 1000.0f, 3500.0f, 10000.0f };
    float gainDb[kBands] {};
    float q[kBands] { 0.9f, 0.9f, 0.9f, 0.9f, 0.9f };
    float outputDb = 0.0f;

    bool hpfOn() const { return hpf > 20.5f; }
    bool lpfOn() const { return lpf < 19900.0f; }
    bool isFlat() const
    {
        for (float g : gainDb)
            if (std::abs(g) >= 0.01f)
                return false;
        return ! hpfOn() && ! lpfOn() && std::abs(outputDb) < 0.01f;
    }
    bool operator==(const OutputEqSettings& o) const
    {
        for (int b = 0; b < kBands; ++b)
            if (freq[b] != o.freq[b] || gainDb[b] != o.gainDb[b] || q[b] != o.q[b])
                return false;
        return hpf == o.hpf && lpf == o.lpf && outputDb == o.outputDb;
    }
};

class OutputEq
{
public:
    static constexpr int kStages = OutputEqSettings::kBands + 2;
    using Coeffs = ParametricEq::Coeffs;
    using Shape = ParametricEq::Shape;

    static std::array<Coeffs, kStages> designAll(const OutputEqSettings& s, double fs)
    {
        std::array<Coeffs, kStages> c {};
        c[0] = s.hpfOn() ? ParametricEq::design(Shape::HighPass, s.hpf, 0.0, 0.707, fs) : Coeffs {};
        for (int b = 0; b < OutputEqSettings::kBands; ++b)
            c[(size_t) b + 1] = ParametricEq::design(Shape::Bell, s.freq[b], s.gainDb[b], s.q[b], fs);
        c[kStages - 1] = s.lpfOn() ? ParametricEq::design(Shape::LowPass, s.lpf, 0.0, 0.707, fs) : Coeffs {};
        return c;
    }

    /** Response at `freq` in dB (without the output trim), for the curve display. */
    static double magnitudeDb(const OutputEqSettings& s, double freq, double fs)
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

    void process(float* L, float* R, int n, const OutputEqSettings& s)
    {
        if (! valid || ! (s == current))
        {
            current = s;
            valid = true;
            flat = s.isFlat();
            coeffs = designAll(s, fs);
            gain = std::pow(10.0f, s.outputDb / 20.0f);
        }
        if (flat)
            return;
        float* io[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                double x = io[c][i];
                for (int b = 0; b < kStages; ++b)
                {
                    auto& st = state[(size_t) (c * kStages + b)];
                    const auto& k = coeffs[(size_t) b];
                    const double y = k.b0 * x + st.z1;
                    st.z1 = k.b1 * x - k.a1 * y + st.z2;
                    st.z2 = k.b2 * x - k.a2 * y;
                    x = y;
                }
                io[c][i] = (float) x * gain;
            }
    }

private:
    struct State { double z1 = 0, z2 = 0; };
    float fs = 48000.0f;
    OutputEqSettings current;
    bool valid = false, flat = true;
    float gain = 1.0f;
    std::array<Coeffs, kStages> coeffs {};
    std::array<State, 2 * kStages> state {};
};

} // namespace zsynth::dsp
