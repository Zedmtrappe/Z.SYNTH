#pragma once

// Small, allocation-free DSP helpers shared by the whole engine. JUCE-free.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
 #include <xmmintrin.h>
 #define ZS_HAS_SSE 1
#endif

namespace zsynth::dsp
{

constexpr float kPi     = 3.14159265358979323846f;
constexpr float kTwoPi  = 6.28318530717958647692f;
constexpr float kLn2    = 0.69314718055994530942f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

/** floor() without the libm call that baseline x86-64 builds emit for std::floor.
    Valid for |x| < 2^31. */
inline float fastFloor(float x)
{
    const float t = (float) (int32_t) x;
    return t > x ? t - 1.0f : t;
}

/** Wraps a phase into [0, 1). */
inline float wrapPhase(float x) { return x - fastFloor(x); }
inline float lerp(float a, float b, float t)     { return a + (b - a) * t; }

/** Odd 7/6 Padé approximation of tanh, accurate to ~1e-4 and exactly bounded. */
inline float fastTanh(float x)
{
    x = clampf(x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float a  = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b  = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return clampf(a / b, -1.0f, 1.0f);
}

/** tanh(x)/x, the "transconductance" factor used by the linearised nonlinear
    filters. Same Pade approximant as fastTanh with the x cancelled, so it costs
    one division; beyond the approximant's range tanh(x) = 1 to float precision. */
inline float tanhRatio(float x)
{
    const float ax = std::abs(x);
    if (ax >= 4.97f)
        return 1.0f / ax;
    const float x2 = x * x;
    const float a = 135135.0f + x2 * (17325.0f + x2 * (378.0f + x2));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

/** Smooth cubic soft clipper; |output| <= 2/3 * limit. Cheap and alias-friendly. */
inline float softClipCubic(float x)
{
    x = clampf(x, -1.0f, 1.0f);
    return x - (x * x * x) * (1.0f / 3.0f);
}

/** 2^x with ~1e-6 relative error; valid for |x| < 126. */
inline float fastExp2(float x)
{
    x = clampf(x, -126.0f, 126.0f);
    const float fi = fastFloor(x);
    const float f  = x - fi;
    const float p  = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
    const int32_t e = ((int32_t) fi + 127) << 23;
    float scale;
    std::memcpy(&scale, &e, sizeof(scale));
    return p * scale;
}

inline float semitonesToRatio(float semis) { return fastExp2(semis * (1.0f / 12.0f)); }
inline float midiNoteToHz(float note)      { return 440.0f * fastExp2((note - 69.0f) * (1.0f / 12.0f)); }
inline float dbToGain(float db)            { return std::pow(10.0f, db * 0.05f); }

/** Flushes values that would otherwise become denormal in feedback paths. */
inline float undenormalise(float x) { return std::abs(x) < 1.0e-15f ? 0.0f : x; }

inline bool isFiniteSample(float x) { return std::isfinite(x); }

/** RAII guard enabling flush-to-zero / denormals-are-zero for the current thread. */
class ScopedNoDenormals
{
public:
    ScopedNoDenormals()
    {
#if ZS_HAS_SSE
        saved = _mm_getcsr();
        _mm_setcsr(saved | 0x8040); // FTZ | DAZ
#endif
    }
    ~ScopedNoDenormals()
    {
#if ZS_HAS_SSE
        _mm_setcsr(saved);
#endif
    }
private:
    unsigned int saved = 0;
};

// -----------------------------------------------------------------------------
/** Deterministic fast PRNG (xorshift32). Each voice owns one. */
class Random
{
public:
    explicit Random(uint32_t seed = 0x1234567u) { setSeed(seed); }
    void setSeed(uint32_t seed) { state = seed != 0 ? seed : 0x9E3779B9u; }

    uint32_t next()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return state = x;
    }

    /** Uniform in [0, 1). */
    float unipolar() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    /** Uniform in [-1, 1). */
    float bipolar()  { return unipolar() * 2.0f - 1.0f; }

private:
    uint32_t state;
};

// -----------------------------------------------------------------------------
/** 4096-point sine lookup with linear interpolation. Phase in cycles. */
class SineTable
{
public:
    static constexpr int kSize = 4096;

    static const SineTable& get()
    {
        static const SineTable table;
        return table;
    }

    /** phase in cycles (any real value). */
    float lookup(float phase) const
    {
        phase -= fastFloor(phase);
        const float pos = phase * (float) kSize;
        const int i = (int) pos;
        const float f = pos - (float) i;
        return data[i] + (data[i + 1] - data[i]) * f;
    }

private:
    SineTable()
    {
        for (int i = 0; i <= kSize; ++i)
            data[i] = (float) std::sin(2.0 * 3.14159265358979323846 * (double) i / kSize);
    }

    float data[kSize + 1];
};

inline float fastSin(float phaseCycles) { return SineTable::get().lookup(phaseCycles); }

// -----------------------------------------------------------------------------
/** One-pole parameter smoother (exponential approach). */
class Smoother
{
public:
    void setTime(float seconds, float updateRate)
    {
        coeff = seconds <= 0.0f ? 0.0f : std::exp(-1.0f / (seconds * updateRate));
    }
    void reset(float v) { current = target = v; }
    void setTarget(float v) { target = v; }
    float next() { current = target + (current - target) * coeff; return current; }
    float value() const { return current; }

private:
    float current = 0.0f, target = 0.0f, coeff = 0.0f;
};

// -----------------------------------------------------------------------------
/** Beat lengths of the shared tempo division list (ZS_CH_DIVISION order). */
inline float divisionBeats(int index)
{
    static constexpr float beats[] = {
        16.0f, 8.0f, 4.0f, 3.0f, 2.0f, 4.0f / 3.0f, 1.5f, 1.0f, 2.0f / 3.0f,
        0.75f, 0.5f, 1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f, 0.125f
    };
    return beats[std::clamp(index, 0, 15)];
}

} // namespace zsynth::dsp
