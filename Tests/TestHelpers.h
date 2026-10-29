#pragma once

// Shared helpers for rendering the engine and analysing its output.

#include "DSP/Common/FFT.h"
#include "DSP/Engine/SynthEngine.h"
#include "Parameters/ParamTable.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace zstest
{

using namespace zsynth;
using namespace zsynth::dsp;

/** An engine plus a parameter array and a MIDI event queue. */
struct EngineRig
{
    explicit EngineRig(double sr = 48000.0, int block = 256) : sampleRate(sr), blockSize(block)
    {
        fillDefaults(params);
        // Neutral settings for measurement unless a test overrides them.
        params[P::Drift] = 0.0f;
        params[P::Vintage] = 0.0f;
        params[P::MixDrive] = 0.0f;
        params[P::Character] = 0.0f;
        engine.prepare(sr, block);
    }

    void set(int index, float value) { params[index] = value; }

    void noteOn(double atSeconds, int note, int vel = 100, int channel = 0)
    {
        queue.push_back({ (long long) (atSeconds * sampleRate), (uint8_t) (0x90 | channel), (uint8_t) note, (uint8_t) vel });
    }
    void noteOff(double atSeconds, int note, int channel = 0)
    {
        queue.push_back({ (long long) (atSeconds * sampleRate), (uint8_t) (0x80 | channel), (uint8_t) note, 0 });
    }
    void cc(double atSeconds, int number, int value, int channel = 0)
    {
        queue.push_back({ (long long) (atSeconds * sampleRate), (uint8_t) (0xB0 | channel), (uint8_t) number, (uint8_t) value });
    }
    void bend(double atSeconds, int value14, int channel = 0)
    {
        queue.push_back({ (long long) (atSeconds * sampleRate), (uint8_t) (0xE0 | channel), (uint8_t) (value14 & 0x7F), (uint8_t) ((value14 >> 7) & 0x7F) });
    }

    /** Renders `seconds` of audio, returning left (and filling right). */
    std::vector<float> render(double seconds, std::vector<float>* rightOut = nullptr)
    {
        const long long total = (long long) (seconds * sampleRate);
        std::vector<float> left((size_t) total), right((size_t) total);
        std::sort(queue.begin(), queue.end(), [](const Queued& a, const Queued& b) { return a.time < b.time; });

        std::vector<MidiEvent> events;
        long long pos = 0;
        while (pos < total)
        {
            const int n = (int) std::min<long long>(blockSize, total - pos);
            events.clear();
            for (const auto& q : queue)
                if (q.time >= renderedSamples + pos && q.time < renderedSamples + pos + n)
                    events.push_back({ (int) (q.time - renderedSamples - pos), q.status, q.d1, q.d2 });
            engine.process(left.data() + pos, right.data() + pos, n, params, events.data(), (int) events.size(), transport);
            pos += n;
        }
        renderedSamples += total;
        if (rightOut != nullptr)
            *rightOut = right;
        return left;
    }

    struct Queued { long long time; uint8_t status, d1, d2; };

    double sampleRate;
    int blockSize;
    float params[kNumParams];
    SynthEngine engine;
    TransportInfo transport;
    std::vector<Queued> queue;
    long long renderedSamples = 0;
};

inline float peakOf(const std::vector<float>& x, size_t from = 0, size_t to = (size_t) -1)
{
    float p = 0.0f;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; ++i)
        p = std::max(p, std::abs(x[i]));
    return p;
}

inline double rmsOf(const std::vector<float>& x, size_t from = 0, size_t to = (size_t) -1)
{
    double s = 0.0;
    to = std::min(to, x.size());
    if (to <= from)
        return 0.0;
    for (size_t i = from; i < to; ++i)
        s += (double) x[i] * x[i];
    return std::sqrt(s / (double) (to - from));
}

inline bool allFinite(const std::vector<float>& x)
{
    for (float v : x)
        if (! std::isfinite(v))
            return false;
    return true;
}

/** Estimated fundamental from the strongest spectral peak (parabolic interpolation). */
inline double dominantFrequency(const float* data, size_t n, double sampleRate)
{
    const auto mag = magnitudeSpectrum(data, n);
    size_t best = 1;
    for (size_t i = 2; i + 1 < mag.size(); ++i)
        if (mag[i] > mag[best])
            best = i;
    const double a = std::log(mag[best - 1] + 1e-20), b = std::log(mag[best] + 1e-20), c = std::log(mag[best + 1] + 1e-20);
    const double offset = 0.5 * (a - c) / (a - 2.0 * b + c);
    return ((double) best + offset) * sampleRate / (double) n;
}

/** Ratio (dB) of energy away from the harmonics of f0 to energy on them, 20 Hz..maxHz. */
inline double inharmonicRatioDb(const float* data, size_t n, double sampleRate, double f0, double maxHz = 20000.0)
{
    const auto mag = magnitudeSpectrum(data, n);
    const double binHz = sampleRate / (double) n;
    double harmonic = 0.0, other = 0.0;
    for (size_t i = 1; i < mag.size(); ++i)
    {
        const double f = (double) i * binHz;
        if (f < 20.0 || f > maxHz)
            continue;
        const double h = f / f0;
        const double dist = std::abs(h - std::round(h)) * f0 / binHz; // distance in bins
        const double e = mag[i] * mag[i];
        if (dist <= 6.0 && std::round(h) >= 1.0)
            harmonic += e;
        else
            other += e;
    }
    return 10.0 * std::log10((other + 1e-30) / (harmonic + 1e-30));
}

/** Largest sample-to-sample jump. */
inline float maxStep(const std::vector<float>& x, size_t from = 1)
{
    float m = 0.0f;
    for (size_t i = std::max<size_t>(from, 1); i < x.size(); ++i)
        m = std::max(m, std::abs(x[i] - x[i - 1]));
    return m;
}

struct Stopwatch
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    double seconds() const { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); }
};

} // namespace zstest
