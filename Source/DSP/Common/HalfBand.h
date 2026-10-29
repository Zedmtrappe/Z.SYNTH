#pragma once

// Linear-phase half-band FIR for 2x resampling.
//
// 63 taps, Kaiser window (beta 8): ~-80 dB stopband from 0.29 fs_os, passband
// to ~0.21 fs_os (i.e. 0.42 x the base rate: 18.5 kHz at 44.1 kHz). Every
// second tap except the centre is zero, so decimation costs 16 MACs per
// output sample. Coefficients are designed at construction (no tables to
// maintain); Tests check the response.

#include <array>
#include <cmath>

namespace zsynth::dsp
{

class HalfBandCoefficients
{
public:
    static constexpr int kTaps = 63;
    static constexpr int kCentre = kTaps / 2; // 31 (odd -> centre tap 0.5)

    static const HalfBandCoefficients& get()
    {
        static const HalfBandCoefficients c;
        return c;
    }

    std::array<float, kTaps> h {};

private:
    static double besselI0(double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    HalfBandCoefficients()
    {
        constexpr double beta = 8.0;
        const double denom = besselI0(beta);
        double sum = 0.0;
        std::array<double, kTaps> d {};
        for (int n = 0; n < kTaps; ++n)
        {
            const int m = n - kCentre;
            const double sinc = m == 0 ? 0.5 : std::sin(0.5 * 3.14159265358979323846 * m) / (3.14159265358979323846 * m);
            const double r = (double) m / kCentre;
            const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / denom;
            d[(size_t) n] = (m != 0 && (m % 2) == 0) ? 0.0 : sinc * w;
            sum += d[(size_t) n];
        }
        for (int n = 0; n < kTaps; ++n)
            h[(size_t) n] = (float) (d[(size_t) n] / sum); // unity DC gain
    }
};

/** FIR over a doubled history buffer so the taps read contiguous memory;
    only the 17 non-zero half-band taps are evaluated. */
class HalfBandFir
{
public:
    static constexpr int N = HalfBandCoefficients::kTaps;

    HalfBandFir()
    {
        const auto& h = HalfBandCoefficients::get().h;
        for (int n = 0; n < N; ++n)
        {
            if (h[(size_t) n] != 0.0f)
            {
                // Window position 0 is the oldest sample, so tap n multiplies
                // the sample that is (N - 1 - n) old.
                offsets[(size_t) count] = N - 1 - n;
                coeffs[(size_t) count] = h[(size_t) n];
                ++count;
            }
        }
    }

    void reset() { history.fill(0.0f); pos = 0; }

    void push(float x)
    {
        history[(size_t) pos] = x;
        history[(size_t) (pos + N)] = x;
        pos = pos + 1 == N ? 0 : pos + 1;
    }

    /** Output for the most recently pushed sample. */
    float output() const
    {
        const float* w = history.data() + pos; // oldest .. newest
        float acc = 0.0f;
        for (int k = 0; k < count; ++k)
            acc += coeffs[(size_t) k] * w[offsets[(size_t) k]];
        return acc;
    }

private:
    std::array<float, 2 * N> history {};
    std::array<int, N> offsets {};
    std::array<float, N> coeffs {};
    int count = 0;
    int pos = 0;
};

/** Takes two oversampled input samples, returns one base-rate sample. */
class HalfBandDecimator
{
public:
    void reset() { fir.reset(); }

    float process(float a, float b)
    {
        fir.push(a);
        fir.push(b);
        return fir.output();
    }

private:
    HalfBandFir fir;
};

/** Produces two oversampled samples from one base-rate sample. */
class HalfBandInterpolator
{
public:
    void reset() { fir.reset(); }

    void process(float x, float& outA, float& outB)
    {
        fir.push(x * 2.0f);
        outA = fir.output();
        fir.push(0.0f);
        outB = fir.output();
    }

private:
    HalfBandFir fir;
};

} // namespace zsynth::dsp
