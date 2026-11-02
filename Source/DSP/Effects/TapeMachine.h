#pragma once

// Tape machine: record -> tape -> playback, per channel at twice the host rate.
//
//   record EQ   pre-emphasis (treble lifted before the tape), so when driven
//               the highs saturate first, as on real tape; de-emphasis after.
//   tape        saturation with a small hysteresis loop (play operator); BIAS
//               trades distortion and treble: under-biased = brighter and
//               dirtier, over-biased = cleaner and darker.
//   playback    head bump (a low-frequency resonance whose frequency rises
//               with tape speed), treble loss (falls with speed), LF roll-off.
//   transport   wow (slow) and flutter (fast) as a pitch-modulated delay,
//               the same on both channels (the tape moves as one); hiss.
//
// Values per machine follow published specs and documented character (head
// bump frequency and size, HF response, wow & flutter, noise, headroom); they
// are models of each machine's character, not circuit-exact copies.

#include "../Common/HalfBand.h"
#include "../../Parameters/ParamChoices.h"
#include "ParametricEq.h"

#include <array>
#include <cmath>
#include <vector>

namespace zsynth::dsp
{

struct TapeSettings
{
    TapeModel model = TapeModel::StuderA800;
    int speed = 1;          // 0 / 1 / 2 = 7.5 / 15 / 30 ips (cassette: 1 7/8 / 3 3/4 / 3 3/4)
    float driveDb = 0.0f;   // into the tape (level-compensated)
    float bias = 0.5f;      // 0 = under, 0.5 = calibrated, 1 = over
    float wow = 0.5f;       // 0 = off, 0.5 = the machine as specified, 1 = worn (x3)
    float hiss = 0.0f;      // 0 = off, 0.5 = the machine's noise floor, 1 = +6 dB
    float outputDb = 0.0f;
};

class TapeMachine
{
public:
    struct Model
    {
        float nominalIps;   // the speed the figures below are for
        float headroom;     // input-referred saturation level (0 dBFS peak = 1)
        float hysteresis;   // loop width (fraction of the headroom)
        float even;         // asymmetry: 2nd-harmonic share
        float bumpHz, bumpDb, bumpQ;
        float hfHz;         // playback -3 dB point at the nominal speed
        float emphasisDb;   // record pre-emphasis above ~3 kHz
        float wowPct, flutterPct; // weighted peak deviation (%)
        float hissDb;       // noise floor at the nominal speed (dBFS)
        float colourDb;     // electronics tilt above 8 kHz (output)
    };

    static const Model& model(TapeModel m)
    {
        static const Model models[(int) TapeModel::Count] = {
            // ips   head  hyst  even  bump Hz/dB/Q      HF      emph  wow    flut   hiss  colour
            { 15.0f, 1.00f, 0.012f, 0.04f, 55.0f, 2.0f, 1.3f, 18000.0f, 6.0f, 0.03f, 0.04f, -66.0f, 0.0f },  // Studer A800: punchy, big lows
            { 15.0f, 1.40f, 0.008f, 0.05f, 48.0f, 1.2f, 1.1f, 22000.0f, 5.0f, 0.02f, 0.025f, -70.0f, 0.2f }, // Ampex ATR-102: clean, extended
            { 15.0f, 0.85f, 0.018f, 0.12f, 70.0f, 2.5f, 1.2f, 13000.0f, 7.0f, 0.08f, 0.06f, -60.0f, -0.8f }, // Studer J37: valve, warm, soft top
            { 15.0f, 1.10f, 0.010f, 0.03f, 60.0f, 1.6f, 1.4f, 20000.0f, 6.0f, 0.035f, 0.04f, -66.0f, 0.6f }, // Otari MTR-90: tight, a touch bright
            { 1.875f, 0.70f, 0.024f, 0.08f, 50.0f, 1.5f, 1.0f, 10000.0f, 9.0f, 0.12f, 0.10f, -54.0f, -0.5f },  // Cassette 4-track
        };
        return models[std::clamp((int) m, 0, (int) TapeModel::Count - 1)];
    }

    static float ipsFor(TapeModel m, int speed)
    {
        static const float reel[3] = { 7.5f, 15.0f, 30.0f };
        static const float cassette[3] = { 1.875f, 3.75f, 3.75f };
        const int i = std::clamp(speed, 0, 2);
        return m == TapeModel::Cassette ? cassette[i] : reel[i];
    }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        delay.assign((size_t) (0.04f * fs) + 8, 0.0f);
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
            play[c] = 0.0f;
        }
        std::fill(delay.begin(), delay.end(), 0.0f);
        writePos = 0;
        phases[0] = 0.0; phases[1] = 0.37; phases[2] = 0.11; phases[3] = 0.73;
        drift = driftTarget = 0.0f;
        noiseSeed = 0x2545F491u;
        hissLp[0] = hissLp[1] = hissHp[0] = hissHp[1] = 0.0f;
    }

    /** The playback/record EQ at rate `rate` (for tests and the display). */
    static int design(const TapeSettings& s, double rate, ParametricEq::Coeffs* c, float* emphasisOut = nullptr)
    {
        const auto& m = model(s.model);
        const double scale = ipsFor(s.model, s.speed) / m.nominalIps;
        const double bias = std::clamp((double) s.bias, 0.0, 1.0);
        int n = 0;
        using PE = ParametricEq;
        // 0, 1: pre-emphasis and de-emphasis (de-emphasis is applied after the tape).
        c[n++] = PE::design(PE::Shape::HighShelf, 3000.0 * std::sqrt(scale), m.emphasisDb, 0.7, rate);
        c[n++] = PE::design(PE::Shape::HighShelf, 3000.0 * std::sqrt(scale), -m.emphasisDb, 0.7, rate);
        // Playback: head bump (and the dip an octave above it), treble loss, LF roll-off, electronics tilt.
        const double bump = m.bumpHz * scale;
        c[n++] = PE::design(PE::Shape::Bell, bump, m.bumpDb, m.bumpQ, rate);
        c[n++] = PE::design(PE::Shape::Bell, bump * 2.2, -0.45 * m.bumpDb, 1.4, rate);
        const double hf = std::min(0.45 * rate, m.hfHz * std::pow(scale, 0.85) * (1.2 - 0.4 * bias));
        c[n++] = PE::design(PE::Shape::LowPass, hf, 0.0, 0.707, rate);
        c[n++] = PE::design(PE::Shape::HighPass, 18.0 * std::pow(scale, 0.3), 0.0, 0.707, rate);
        c[n++] = PE::design(PE::Shape::HighShelf, 8000.0, m.colourDb, 0.7, rate);
        if (emphasisOut != nullptr)
            *emphasisOut = m.emphasisDb;
        return n;
    }

    void process(float* L, float* R, int n, const TapeSettings& s)
    {
        if (! valid || s.model != current.model || s.speed != current.speed || s.bias != current.bias)
        {
            if (valid && s.model != current.model)
                reset();
            stages = design(s, fs * 2.0, coeffs.data());
            const auto& m = model(s.model);
            const float bias = std::clamp(s.bias, 0.0f, 1.0f);
            headroom = m.headroom * (0.7f + 0.6f * bias);
            hystW = m.hysteresis * (1.6f - 1.2f * bias) * headroom;
            even = m.even;
            const float scale = ipsFor(s.model, s.speed) / m.nominalIps;
            const float wf = 1.0f / std::sqrt(std::max(0.1f, scale));
            wowDev = m.wowPct * 0.01f * wf;
            flutterDev = m.flutterPct * 0.01f * wf;
            hissLevel = std::pow(10.0f, (m.hissDb - 6.0f * std::log2(std::max(0.1f, scale))) / 20.0f);
            const float hf = std::min(0.45f * fs, m.hfHz * std::pow(scale, 0.85f));
            hissLpA = 1.0f - std::exp(-2.0f * 3.14159265f * hf / fs);
            hissHpA = std::exp(-2.0f * 3.14159265f * 800.0f / fs);
            current = s;
            valid = true;
        }
        current.driveDb = s.driveDb;
        current.wow = s.wow;
        current.hiss = s.hiss;
        current.outputDb = s.outputDb;

        const float in = std::pow(10.0f, s.driveDb / 20.0f);
        const float out = std::pow(10.0f, s.outputDb / 20.0f) / in;
        const float biasOut = shape(even);
        const float wowScale = s.wow <= 0.5f ? s.wow * 2.0f : 1.0f + (s.wow - 0.5f) * 4.0f; // 0 .. 1 (as specified) .. 3 (worn)
        const float hissGain = s.hiss <= 0.0f ? 0.0f : hissLevel * 2.0f * s.hiss;

        float* io[2] = { L, R };
        const size_t len = delay.size() / 2;
        for (int i = 0; i < n; ++i)
        {
            // Wow (two slow sines and a random drift) and flutter (two fast sines):
            // the delay that gives pitch deviation dev at rate f has depth dev / (2 pi f).
            advance(0, 0.53f); advance(1, 0.87f); advance(2, 6.8f); advance(3, 11.3f);
            if (++driftCount >= (int) (fs * 0.25f))
            {
                driftCount = 0;
                driftTarget = rand() * 0.7f;
            }
            drift += (driftTarget - drift) * (1.0f / (fs * 0.4f));
            const float tw = 2.0f * 3.14159265f;
            const float wowD = wowScale * wowDev * (std::sin(tw * (float) phases[0]) / (tw * 0.53f) * 0.6f
                                                    + std::sin(tw * (float) phases[1]) / (tw * 0.87f) * 0.4f + drift / (tw * 0.3f) * 0.3f);
            const float flutD = wowScale * flutterDev * (std::sin(tw * (float) phases[2]) / (tw * 6.8f) * 0.7f
                                                         + std::sin(tw * (float) phases[3]) / (tw * 11.3f) * 0.3f);
            const float delaySamples = baseDelay * fs + (wowD + flutD) * fs;

            for (int c = 0; c < 2; ++c)
            {
                float a, b;
                up[c].process(io[c][i] * in, a, b);
                a = tapeTick(a, c, biasOut);
                b = tapeTick(b, c, biasOut);
                float y = down[c].process(a, b);

                // Transport: the delay line (cubic interpolation).
                delay[(size_t) c * len + writePos] = y;
                const float rp = (float) writePos - delaySamples + (float) len * 2.0f;
                const int i1 = (int) rp;
                const float fr = rp - (float) i1;
                auto at = [&](int k) { return delay[(size_t) c * len + (size_t) ((i1 + k) % (int) len)]; };
                const float y0 = at(-1 + (int) len), y1 = at(0), y2 = at(1), y3 = at(2);
                y = y1 + 0.5f * fr * (y2 - y0 + fr * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + fr * (3.0f * (y1 - y2) + y3 - y0)));

                if (hissGain > 0.0f)
                {
                    // Hiss: white noise, high-passed (tape noise is treble-heavy) and band-limited by the head.
                    const float w = rand();
                    hissHp[c] = hissHpA * (hissHp[c] + w - hissPrev[c]);
                    hissPrev[c] = w;
                    hissLp[c] += hissLpA * (hissHp[c] - hissLp[c]);
                    y += hissLp[c] * hissGain * 1.7f;
                }
                io[c][i] = y * out;
            }
            writePos = (writePos + 1) % len;
        }
    }

    /** Total delay of the transport (for reference). */
    static constexpr float baseDelay = 0.006f;

private:
    struct State { double z1 = 0, z2 = 0; };

    float shape(float u) const { return std::tanh(u); }

    void advance(int k, float hz) { phases[k] += hz / fs; if (phases[k] >= 1.0) phases[k] -= 1.0; }

    float rand()
    {
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        return (float) (noiseSeed >> 8) / 8388608.0f - 1.0f;
    }

    float biquad(float x, int c, int k)
    {
        auto& st = state[(size_t) (c * kStages + k)];
        const auto& q = coeffs[(size_t) k];
        const double y = q.b0 * x + st.z1;
        st.z1 = q.b1 * x - q.a1 * y + st.z2;
        st.z2 = q.b2 * x - q.a2 * y;
        return (float) y;
    }

    float tapeTick(float x, int c, float biasOut)
    {
        float y = biquad(x, c, 0); // pre-emphasis
        // Magnetisation: a play operator gives the loop (lag at low levels), then the soft limit of the tape.
        play[c] = std::clamp(play[c], y - hystW, y + hystW);
        const float m = 0.85f * y + 0.15f * play[c];
        y = (shape(m / headroom + even) - biasOut) * headroom;
        y = biquad(y, c, 1); // de-emphasis
        for (int k = 2; k < stages; ++k)
            y = biquad(y, c, k);
        return y;
    }

    static constexpr int kStages = 8;
    float fs = 48000.0f;
    TapeSettings current;
    bool valid = false;
    int stages = 0;
    std::array<ParametricEq::Coeffs, kStages> coeffs {};
    std::array<State, 2 * kStages> state {};
    HalfBandInterpolator up[2];
    HalfBandDecimator down[2];
    float play[2] {};
    float headroom = 1.0f, hystW = 0.05f, even = 0.1f;
    float wowDev = 0.0f, flutterDev = 0.0f;
    float hissLevel = 0.0f, hissLpA = 0.5f, hissHpA = 0.9f;
    float hissLp[2] {}, hissHp[2] {}, hissPrev[2] {};
    std::vector<float> delay;
    size_t writePos = 0;
    double phases[4] {};
    float drift = 0.0f, driftTarget = 0.0f;
    int driftCount = 0;
    uint32_t noiseSeed = 0x2545F491u;
};

} // namespace zsynth::dsp
