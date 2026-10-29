#pragma once

// Minimal in-place radix-2 complex FFT. Used only off the audio thread
// (wavetable generation and tests), so it favours clarity over speed.

#include <cmath>
#include <complex>
#include <vector>

namespace zsynth::dsp
{

/** In-place FFT of a power-of-two length buffer. inverse=true computes the
    unnormalised inverse transform (caller divides by N if required). */
inline void fftInPlace(std::vector<std::complex<double>>& x, bool inverse)
{
    const size_t n = x.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(x[i], x[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = 2.0 * 3.14159265358979323846 / (double) len * (inverse ? 1.0 : -1.0);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = x[i + k];
                const auto v = x[i + k + len / 2] * w;
                x[i + k] = u + v;
                x[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

/** Magnitude spectrum (first N/2 bins) of a real signal, 4-term
    Blackman-Harris window (-92 dB sidelobes, main lobe +/-4 bins), so that
    measurements of aliasing are not limited by window leakage. */
inline std::vector<double> magnitudeSpectrum(const float* data, size_t n)
{
    std::vector<std::complex<double>> buf(n);
    for (size_t i = 0; i < n; ++i)
    {
        const double x = 2.0 * 3.14159265358979323846 * (double) i / (double) (n - 1);
        const double w = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2.0 * x) - 0.01168 * std::cos(3.0 * x);
        buf[i] = { data[i] * w, 0.0 };
    }
    fftInPlace(buf, false);
    std::vector<double> mag(n / 2);
    for (size_t i = 0; i < n / 2; ++i)
        mag[i] = std::abs(buf[i]) / (double) n;
    return mag;
}

} // namespace zsynth::dsp
