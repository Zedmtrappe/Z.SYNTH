#pragma once

// Vintage 10-band graphic EQ for the output, voiced after the API 560:
// 31 Hz ... 16 kHz octave bands of +/-12 dB with "proportional Q" (each band
// narrows as it is pushed harder, so small moves are broad and gentle and big
// moves are focused), followed by an output transformer stage (soft
// saturation, a little low-end weight, a slightly softened top).
//
// FLAT = DRY: with every band at 0 dB the stage is bypassed exactly, so the
// transformer colour only comes in once the EQ is actually used.

#include "ParametricEq.h"

namespace zsynth::dsp
{

class VintageGraphicEq
{
public:
    static constexpr int kBands = 10;
    static constexpr float kFreqs[kBands] = { 31.0f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };

    struct Settings
    {
        float gainDb[kBands] {};
        float colour = 0.3f;  // transformer saturation, 0..1
        float outputDb = 0.0f;

        bool isFlat() const
        {
            for (float g : gainDb)
                if (std::abs(g) >= 0.05f)
                    return false;
            return true;
        }
    };

    /** Proportional Q: broad for gentle settings, narrower as gain rises. */
    static double qFor(float gainDb) { return 0.6 + 0.12 * std::abs(gainDb); }

    static ParametricEq::Coeffs band(int b, float gainDb, double fs)
    {
        return ParametricEq::design(ParametricEq::Shape::Bell, std::min((double) kFreqs[b], 0.42 * fs), gainDb, qFor(gainDb), fs);
    }

    /** Response of the ten bands at `freq` in dB (transformer colour excluded). */
    static double magnitudeDb(const Settings& s, double freq, double fs)
    {
        if (s.isFlat())
            return 0.0;
        const double w = 2.0 * 3.14159265358979323846 * freq / fs;
        const std::complex<double> z1 = std::polar(1.0, -w), z2 = z1 * z1;
        double db = s.outputDb;
        for (int b = 0; b < kBands; ++b)
        {
            const auto c = band(b, s.gainDb[b], fs);
            const auto h = (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
            db += 20.0 * std::log10(std::max(1.0e-9, std::abs(h)));
        }
        return db;
    }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        aBump = 1.0f - std::exp(-kTwoPi * 90.0f / fs);
        aTop = 1.0f - std::exp(-kTwoPi * std::min(17000.0f, 0.45f * fs) / fs);
        reset();
        valid = false;
    }

    void reset()
    {
        for (auto& st : state)
            st = {};
        for (int c = 0; c < 2; ++c)
            bump[c] = top[c] = dc[c] = 0.0f;
    }

    void process(float* L, float* R, int n, const Settings& s)
    {
        if (s.isFlat())
        {
            // Flat = dry, except for the output trim if one is set.
            if (std::abs(s.outputDb) > 0.01f)
            {
                const float g = dbToGain(s.outputDb);
                for (int i = 0; i < n; ++i)
                {
                    L[i] *= g;
                    R[i] *= g;
                }
            }
            return;
        }
        bool changed = ! valid;
        for (int b = 0; b < kBands; ++b)
            changed = changed || s.gainDb[b] != lastGain[b];
        if (changed)
        {
            for (int b = 0; b < kBands; ++b)
            {
                coeffs[(size_t) b] = band(b, s.gainDb[b], fs);
                lastGain[b] = s.gainDb[b];
            }
            valid = true;
        }

        const float drive = 1.0f + 2.0f * clampf(s.colour, 0.0f, 1.0f);
        const float bumpAmount = 0.12f * clampf(s.colour, 0.0f, 1.0f); // ~ +1 dB low-end weight at full colour
        const float out = dbToGain(s.outputDb);
        float* io[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                double x = io[c][i];
                for (int b = 0; b < kBands; ++b)
                {
                    auto& st = state[(size_t) (c * kBands + b)];
                    const auto& k = coeffs[(size_t) b];
                    const double y = k.b0 * x + st.z1;
                    st.z1 = k.b1 * x - k.a1 * y + st.z2;
                    st.z2 = k.b2 * x - k.a2 * y;
                    x = y;
                }
                // Output transformer: slightly asymmetric soft saturation with
                // unity small-signal gain, a low-frequency bump and a gentle top.
                float y = (float) x;
                const float a = y * drive;
                y = (a >= 0.0f ? fastTanh(a) : fastTanh(a * 1.15f) / 1.15f) / drive;
                dc[c] += 0.0005f * (y - dc[c]);
                y -= dc[c];
                bump[c] += aBump * (y - bump[c]);
                y += bump[c] * bumpAmount;
                top[c] += aTop * (y - top[c]);
                io[c][i] = undenormalise(top[c]) * out;
            }
    }

private:
    struct State { double z1 = 0, z2 = 0; };
    float fs = 48000.0f;
    std::array<ParametricEq::Coeffs, kBands> coeffs {};
    std::array<State, 2 * kBands> state {};
    float lastGain[kBands] {};
    bool valid = false;
    float aBump = 0.0f, aTop = 0.0f;
    float bump[2] {}, top[2] {}, dc[2] {};
};

} // namespace zsynth::dsp
