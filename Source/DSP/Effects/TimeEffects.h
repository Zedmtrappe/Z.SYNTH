#pragma once

// Phaser, stereo delay and reverb.

#include "../../Parameters/ParamChoices.h"
#include "DelayLine.h"

#include <array>

namespace zsynth::dsp
{

// Dual-linear wet/dry law for send-style effects: the dry signal stays at full
// level up to 50 % mix and the wet signal reaches full level at 50 %, so the
// middle of the knob is "both at full" and the ends are pure dry / pure wet.
inline float wetDryDry(float mix) { return std::min(1.0f, 2.0f * (1.0f - mix)); }
inline float wetDryWet(float mix) { return std::min(1.0f, 2.0f * mix); }

// -----------------------------------------------------------------------------
/** Six-stage stereo phaser. Each stage is a TPT first-order all-pass whose
    break frequency is swept exponentially by a sine LFO (90 degrees apart on
    left and right). Feedback around the chain sharpens the notches. */
class Phaser
{
public:
    void prepare(float sampleRate) { fs = sampleRate; reset(); }
    void reset() { for (auto& c : state) c.fill(0.0f); phase = 0.0f; fb[0] = fb[1] = 0.0f; }

    void process(float* L, float* R, int n, float rateHz, float depth, float feedback, float mix)
    {
        const float inc = rateHz / fs;
        const float minHz = 120.0f;
        const float octaves = 1.0f + depth * 6.0f;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            phase = wrapPhase(phase);
            // The sweep is slow (<= 8 Hz): refresh the all-pass coefficient every 8 samples.
            if ((counter++ & 7) == 0)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = 0.5f + 0.5f * fastSin(phase + (c == 0 ? 0.0f : 0.25f));
                    const float hz = std::min(minHz * fastExp2(lfo * octaves), 0.45f * fs);
                    const float g = std::tan(kPi * hz / fs);
                    coeff[c] = g / (1.0f + g);
                }
            }
            for (int c = 0; c < 2; ++c)
            {
                const float G = coeff[c];
                const float x = ch[c][i];
                float y = x + fb[c] * feedback;
                for (int s = 0; s < 6; ++s)
                {
                    // All-pass from a one-pole: ap = 2 * lp - x.
                    const float v = (y - state[c][(size_t) s]) * G;
                    const float lp = v + state[c][(size_t) s];
                    state[c][(size_t) s] = undenormalise(lp + v);
                    y = 2.0f * lp - y;
                }
                fb[c] = fastTanh(y);
                ch[c][i] = x + 0.5f * mix * (y - x); // mix 1 = classic 50/50 notch blend
            }
        }
    }

private:
    float fs = 48000.0f;
    std::array<float, 6> state[2] {};
    float phase = 0.0f;
    float fb[2] {};
    float coeff[2] { 0.1f, 0.1f };
    uint32_t counter = 0;
};

// -----------------------------------------------------------------------------
/** Stereo delay with six characters. Time changes are smoothed (they repitch
    like tape instead of clicking).

      Digital      clean repeats; TONE shapes the feedback only (the original ZYNTH delay)
      Tape RE-201  Space Echo style: tape saturation, head-gap top-end loss,
                   wow and flutter, slight hiss
      Tape EP-3    Echoplex style: warmer and darker, stronger wow, more drive
      Analog DM-2  bucket-brigade: dark two-pole roll-off, gentle compander
                   saturation, slow chorus-like clock wobble, clock hiss
      Diffuse      every repeat passes through all-pass diffusers and smears
      Lo-Fi        sample-rate and bit reduction, telephone band-pass

    WOW / FLUTTER / AGE scale each type's natural amount (0.5 = as voiced). */
struct DelaySettings
{
    float seconds = 0.35f, feedback = 0.35f, tone = 0.6f, mix = 0.25f;
    bool pingPong = true;
    DelayType type = DelayType::Digital;
    float wow = 0.5f, flutter = 0.5f, age = 0.5f, spread = 0.0f, duck = 0.0f;
};

class StereoDelay
{
public:
    struct Voicing
    {
        float wowMs, wowHz;         // slow drift depth (ms) and rate
        float flutterMs, flutterHz; // fast wobble
        float topHz;                // repeat bandwidth at natural age
        float drive;                // feedback-path saturation
        float hiss;                 // noise floor added per pass (linear)
        bool twoPole;               // BBD-style steeper roll-off
        float lowHz;                // head-gap / coupling low cut of each repeat
    };

    static const Voicing& voicing(DelayType t)
    {
        static const Voicing v[(int) DelayType::Count] = {
            // Each type is voiced to be unmistakable at its natural settings
            // (WOW / FLUTTER / AGE at 0.5): every repeat passes these stages again,
            // so the character builds up audibly from the first echo onwards.
            { 0.0f, 0.5f, 0.0f, 6.0f, 20000.0f, 0.0f, 0.0f, false, 20.0f },       // Digital
            { 2.2f, 0.5f, 0.35f, 6.8f, 4200.0f, 1.8f, 0.0012f, false, 140.0f },   // Tape RE-201: thin, dark, wobbly
            { 3.6f, 0.33f, 0.22f, 4.6f, 3100.0f, 2.8f, 0.0008f, false, 90.0f },   // Tape EP-3: darker, crunchier, seasick
            { 1.4f, 0.45f, 0.0f, 6.0f, 2600.0f, 1.2f, 0.0015f, true, 70.0f },     // Analog DM-2: murky, chorused
            { 0.6f, 0.3f, 0.0f, 6.0f, 7000.0f, 0.4f, 0.0f, false, 60.0f },        // Diffuse
            { 0.3f, 0.7f, 0.05f, 9.0f, 3400.0f, 1.0f, 0.0008f, true, 20.0f },     // Lo-Fi
        };
        return v[std::clamp((int) t, 0, (int) DelayType::Count - 1)];
    }

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : lines)
            d.prepare((int) (2.1f * 1.25f * fs + 0.02f * fs));
        static constexpr float diffMs[2][3] = { { 4.7f, 7.9f, 13.1f }, { 5.3f, 8.9f, 12.1f } };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k)
            {
                diffLen[c][k] = std::max(1, (int) (diffMs[c][k] * 0.001f * fs));
                diffusers[c][k].prepare(diffLen[c][k] + 4);
            }
        aDuckAttack = 1.0f - std::exp(-1.0f / (0.005f * fs));
        aDuckRelease = 1.0f - std::exp(-1.0f / (0.25f * fs));
        aDrift = 1.0f - std::exp(-kTwoPi * 0.3f / fs);
        reset();
    }

    void reset()
    {
        for (auto& d : lines)
            d.reset();
        for (auto& row : diffusers)
            for (auto& d : row)
                d.reset();
        for (int c = 0; c < 2; ++c)
            lp[c] = hp[c] = low[c] = top[c] = top2[c] = held[c] = holdPhase[c] = 0.0f;
        smoothedDelay = -1.0f;
        wowPhase = flutterPhase = 0.0f;
        drift = driftTarget = 0.0f;
        duckEnv = 0.0f;
    }

    void process(float* L, float* R, int n, const DelaySettings& s)
    {
        const Voicing& v = voicing(s.type);
        const bool digital = s.type == DelayType::Digital;
        const float target = clampf(s.seconds * fs, 1.0f, 2.0f * fs);
        if (smoothedDelay < 0.0f)
            smoothedDelay = target;
        const float smoothCoeff = 1.0f - std::exp(-1.0f / (0.08f * fs));

        // TONE: below 0.5 darkens the repeats, above 0.5 thins them.
        const float lpHz = s.tone < 0.5f ? 900.0f * fastExp2(s.tone * 2.0f * 4.4f) : 19000.0f;
        const float hpHz = s.tone > 0.5f ? 20.0f * fastExp2((s.tone - 0.5f) * 2.0f * 5.6f) : (s.type == DelayType::LoFi ? 300.0f : 20.0f);
        const float aLp = coeffFor(lpHz), aHp = coeffFor(hpHz);

        const float age = 2.0f * clampf(s.age, 0.0f, 1.0f);           // 1 = as voiced
        const float aTop = coeffFor(v.topHz * fastExp2(-(age - 1.0f) * 1.2f));
        aLow = coeffFor(v.lowHz);
        const float drive = v.drive * age;
        const float driveNorm = 1.0f / (1.0f + drive); // unity small-signal gain: never raises the loop gain
        const float hiss = v.hiss * age;
        const float wowSamples = v.wowMs * 0.001f * fs * 2.0f * clampf(s.wow, 0.0f, 1.0f);
        const float flutterSamples = v.flutterMs * 0.001f * fs * 2.0f * clampf(s.flutter, 0.0f, 1.0f);
        const float wowInc = v.wowHz / fs, flutterInc = v.flutterHz / fs;
        const float spread = 1.0f + 0.25f * clampf(s.spread, 0.0f, 1.0f);
        // Lo-fi: sample-and-hold rate falls from 16 kHz to 5 kHz with age.
        const float holdStep = s.type == DelayType::LoFi ? std::min(1.0f, (16000.0f * fastExp2(-age * 0.85f)) / fs) : 1.0f;
        const float bits = s.type == DelayType::LoFi ? 12.0f - 3.0f * age : 0.0f;
        const float quant = bits > 0.0f ? fastExp2(bits - 1.0f) : 0.0f;
        const float dryGain = wetDryDry(s.mix), wetGain = wetDryWet(s.mix);

        for (int i = 0; i < n; ++i)
        {
            smoothedDelay += (target - smoothedDelay) * smoothCoeff;
            wowPhase = wrapPhase(wowPhase + wowInc);
            flutterPhase = wrapPhase(flutterPhase + flutterInc);
            if ((counter++ & 1023) == 0)
                driftTarget = rng.bipolar();
            drift += aDrift * (driftTarget - drift);
            const float mod = wowSamples * (0.75f * fastSin(wowPhase) + 0.25f * drift)
                            + flutterSamples * fastSin(flutterPhase);
            // The DM-2's clock wobble runs in opposite phase on each side: a gentle chorus.
            const float modR = s.type == DelayType::AnalogDM2 ? -mod : mod;

            float outL = lines[0].read(smoothedDelay + wowSamples + flutterSamples + mod + 2.0f);
            float outR = lines[1].read(smoothedDelay * spread + wowSamples + flutterSamples + modR + 2.0f);

            float fbL, fbR;
            if (digital)
            {
                // The original clean delay: tone shapes the feedback only.
                fbL = toneFilter(0, outL, aLp, aHp) * s.feedback;
                fbR = toneFilter(1, outR, aLp, aHp) * s.feedback;
            }
            else
            {
                outL = colour(0, outL, s.type, v, aLp, aHp, aTop, drive, driveNorm, hiss, holdStep, quant);
                outR = colour(1, outR, s.type, v, aLp, aHp, aTop, drive, driveNorm, hiss, holdStep, quant);
                fbL = outL * s.feedback;
                fbR = outR * s.feedback;
            }

            float inL, inR;
            if (s.pingPong)
            {
                inL = 0.5f * (L[i] + R[i]) + fbR;
                inR = fbL;
            }
            else
            {
                inL = L[i] + fbL;
                inR = R[i] + fbR;
            }
            lines[0].push(fastTanh(inL));
            lines[1].push(fastTanh(inR));

            // Ducking: the echoes step back while the input is busy.
            const float level = std::max(std::abs(L[i]), std::abs(R[i]));
            duckEnv += (level > duckEnv ? aDuckAttack : aDuckRelease) * (level - duckEnv);
            const float duckGain = 1.0f / (1.0f + s.duck * 12.0f * duckEnv);

            L[i] = L[i] * dryGain + outL * wetGain * duckGain;
            R[i] = R[i] * dryGain + outR * wetGain * duckGain;
        }
    }

private:
    float coeffFor(float hz) const { return 1.0f - std::exp(-kTwoPi * std::min(hz, 0.45f * fs) / fs); }

    float toneFilter(int c, float x, float aLp, float aHp)
    {
        lp[c] += aLp * (x - lp[c]);
        hp[c] += aHp * (lp[c] - hp[c]);
        return undenormalise(lp[c] - hp[c]);
    }

    float colour(int c, float x, DelayType type, const Voicing& v, float aLp, float aHp, float aTop,
                 float drive, float driveNorm, float hiss, float holdStep, float quant)
    {
        if (type == DelayType::LoFi)
        {
            // Sample-and-hold decimation and bit reduction.
            holdPhase[c] += holdStep;
            if (holdPhase[c] >= 1.0f)
            {
                holdPhase[c] -= 1.0f;
                held[c] = quant > 0.0f ? std::round(x * quant) / quant : x;
            }
            x = held[c];
        }
        if (type == DelayType::Diffuse)
        {
            constexpr float g = 0.7f;
            for (int k = 0; k < 3; ++k)
            {
                const float d = diffusers[c][k].readInt(diffLen[c][k]);
                const float w = x + d * g;
                diffusers[c][k].push(w);
                x = d - w * g;
            }
        }
        if (drive > 0.0f)
            x = fastTanh(x * (1.0f + drive)) * driveNorm;
        top[c] += aTop * (x - top[c]);
        x = top[c];
        low[c] += aLow * (x - low[c]);
        x -= low[c];
        if (v.twoPole)
        {
            top2[c] += aTop * (x - top2[c]);
            x = top2[c];
        }
        if (hiss > 0.0f)
            x += hiss * rng.bipolar();
        return toneFilter(c, x, aLp, aHp);
    }

    float fs = 48000.0f;
    DelayLine lines[2];
    DelayLine diffusers[2][3];
    int diffLen[2][3] {};
    float aLow = 0.0f;
    float lp[2] {}, hp[2] {}, low[2] {}, top[2] {}, top2[2] {}, held[2] {}, holdPhase[2] {};
    float smoothedDelay = -1.0f;
    float wowPhase = 0.0f, flutterPhase = 0.0f, drift = 0.0f, driftTarget = 0.0f, aDrift = 0.0f;
    float duckEnv = 0.0f, aDuckAttack = 0.0f, aDuckRelease = 0.0f;
    uint32_t counter = 0;
    Random rng { 0x5eed1234u };
};

// -----------------------------------------------------------------------------
/** Feedback-delay-network reverb with nine voicings.

    Core: pre-delay, early-reflection taps, four series all-pass diffusers, then
    eight modulated delay lines mixed by a Hadamard matrix with per-line damping
    and decay gains derived from the requested RT60. Each type sets the tank
    size, diffusion, modulation, bandwidth, damping and early reflections:

      Hall          the original ZYNTH hall (no early reflections)
      Room          small tank, strong early reflections, short decay
      EMT 140 Plate no early reflections, very dense and bright, fast build-up
      Echo Chamber  medium tank, darker, clear early reflections
      Spring        long dispersive all-pass chain (the "boing"), narrow band
      Lexicon 224   12 kHz bandwidth, heavy chorused modulation, grainier
                    diffusion and 12-bit-style quantisation of the tail
      Lexicon 480L  larger and smoother than the 224, rich modulation, 15 kHz
      Bricasti M7   dense, natural: high diffusion, subtle modulation,
                    realistic early reflections, slightly longer low end
      Shimmer       hall with an octave-up pitch shifter in the feedback

    These are original algorithms voiced after those classics, not emulations. */
struct ReverbSettings
{
    ReverbType type = ReverbType::Hall;
    float size = 0.6f, damping = 0.4f, predelaySec = 0.02f, mix = 0.25f;
    float decay = 0.5f, lowCutHz = 20.0f, diffusion = 0.5f, modulation = 0.5f, early = 0.5f, width = 0.5f;
};

class Reverb
{
public:
    struct Voicing
    {
        float scale;        // tank size (delay-line length multiplier)
        float rtMul;        // decay multiplier
        float diffusion;    // input diffuser gain at DIFFUSION 0.5
        float mod;          // modulation depth multiplier at MOD 0.5
        float modHz;
        float bandwidthHz;  // input/output band limit
        float dampMul;      // > 1 darker tail
        float earlyLevel, earlyScale;
        float lowCutHz;     // fixed band-limit at the bottom
        bool allLinesMod, spring, shimmer, grainy;
    };

    static const Voicing& voicing(ReverbType t)
    {
        static const Voicing v[(int) ReverbType::Count] = {
            { 1.00f, 1.0f, 0.62f, 1.0f, 0.7f, 20000.0f, 1.0f, 0.0f, 1.0f, 0.0f, false, false, false, false },   // Hall
            { 0.32f, 0.45f, 0.70f, 0.6f, 0.9f, 18000.0f, 1.0f, 0.9f, 0.45f, 0.0f, false, false, false, false }, // Room
            { 0.55f, 0.9f, 0.78f, 0.8f, 1.1f, 20000.0f, 0.55f, 0.0f, 1.0f, 60.0f, false, false, false, false },// EMT 140 Plate
            { 0.50f, 0.7f, 0.66f, 0.6f, 0.8f, 12000.0f, 1.3f, 0.7f, 0.7f, 0.0f, false, false, false, false },  // Echo Chamber
            { 0.28f, 0.6f, 0.50f, 0.4f, 1.3f, 4800.0f, 1.0f, 0.0f, 1.0f, 150.0f, false, true, false, false },  // Spring
            { 1.10f, 1.1f, 0.50f, 0.85f, 0.75f, 12000.0f, 0.9f, 0.3f, 1.0f, 0.0f, true, false, false, true },  // Lexicon 224
            { 1.25f, 1.1f, 0.66f, 0.9f, 0.6f, 15000.0f, 0.8f, 0.35f, 1.1f, 0.0f, true, false, false, false },  // Lexicon 480L
            { 1.20f, 1.0f, 0.74f, 0.5f, 0.5f, 20000.0f, 0.75f, 0.6f, 1.2f, 0.0f, false, false, false, false }, // Bricasti M7
            { 1.20f, 1.4f, 0.70f, 0.9f, 0.6f, 16000.0f, 0.7f, 0.0f, 1.0f, 0.0f, false, false, true, false },   // Shimmer
        };
        return v[std::clamp((int) t, 0, (int) ReverbType::Count - 1)];
    }

    static constexpr float kMaxScale = 1.25f;

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        const float scale = fs / 44100.0f;
        static constexpr int diffSamples[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 8; ++i)
        {
            const float maxLen = kLineMs[i] * kMaxScale * 0.001f * fs;
            lines[i].prepare((int) (maxLen + 0.015f * fs + 8));
        }
        for (int i = 0; i < 4; ++i)
        {
            diffLen[i] = std::max(1, (int) ((float) diffSamples[i] * scale));
            diffusers[i].prepare(diffLen[i] + 4);
        }
        predelay.prepare((int) (0.36f * fs));
        shimmerLine.prepare(kShimmerWindow * 2 + 8);
        aDrift = 1.0f - std::exp(-kTwoPi * 0.5f / fs);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& d : diffusers) d.reset();
        predelay.reset();
        shimmerLine.reset();
        for (auto& d : damp) d = 0.0f;
        for (auto& a : springState) a = 0.0f;
        bandIn = bandL = bandR = lowL = lowR = 0.0f;
        modPhase = 0.0f;
        shimmerPhase = 0.0f;
        shimmerOut = 0.0f;
        lengthsValid = false;
    }

    void process(float* L, float* R, int n, const ReverbSettings& s)
    {
        const Voicing& v = voicing(s.type);
        const float rt60 = (0.35f + s.size * s.size * 11.0f) * v.rtMul * fastExp2((clampf(s.decay, 0.0f, 1.0f) - 0.5f) * 3.0f);
        float target[8], gains[8];
        for (int i = 0; i < 8; ++i)
        {
            target[i] = kLineMs[i] * v.scale * 0.001f * fs;
            gains[i] = std::pow(10.0f, -3.0f * target[i] / (rt60 * fs));
        }
        if (! lengthsValid)
        {
            std::copy(target, target + 8, lengths);
            lengthsValid = true;
        }
        const float lenSmooth = 1.0f - std::exp(-1.0f / (0.05f * fs));
        const float dampHz = 16000.0f * fastExp2(-s.damping * 4.5f) / v.dampMul;
        const float aDamp = coeffFor(dampHz);
        const float aBand = coeffFor(v.bandwidthHz);
        const float aLow = coeffFor(std::max(s.lowCutHz, v.lowCutHz));
        const float pre = std::max(1.0f, s.predelaySec * fs);
        const float modInc = v.modHz / fs;
        const float modDepth = 0.0006f * fs * v.mod * 2.0f * clampf(s.modulation, 0.0f, 1.0f);
        const float diffGain = clampf(v.diffusion * (0.7f + 0.6f * clampf(s.diffusion, 0.0f, 1.0f)), 0.0f, 0.82f);
        const float e = clampf(s.early, 0.0f, 1.0f);
        const float earlyGain = v.earlyLevel * std::min(1.0f, 2.0f * e);
        const float lateGain = std::min(1.0f, 2.0f * (1.0f - e));
        const float width = 2.0f * clampf(s.width, 0.0f, 1.0f);
        const float erScale = v.earlyScale * (0.5f + s.size) * 0.001f * fs;
        const float dryGain = wetDryDry(s.mix), wetGain = wetDryWet(s.mix);
        const bool band = v.bandwidthHz < 19000.0f;
        // Shimmer feedback shrinks as the tank's own decay lengthens, so the
        // octave-up loop never sustains itself.
        float gMean = 0.0f;
        for (float g : gains)
            gMean += g * 0.125f;
        const float shimmerGain = clampf(2.0f * (1.0f - gMean), 0.0f, 0.3f);

        for (int i = 0; i < n; ++i)
        {
            predelay.push(0.5f * (L[i] + R[i]));
            float x = predelay.read(pre);

            // Early reflections: eight taps after the pre-delay, alternating sides.
            float erL = 0.0f, erR = 0.0f;
            if (earlyGain > 0.0f)
                for (int k = 0; k < 8; ++k)
                {
                    const float tap = predelay.read(pre + kEarlyMs[k] * erScale) * kEarlyGain[k];
                    ((k & 1) ? erR : erL) += tap;
                }

            if (band)
            {
                bandIn += aBand * (x - bandIn);
                x = bandIn;
            }
            if (v.spring)
            {
                // Dispersion: a long chain of identical first-order all-passes
                // delays high frequencies more than low ones, giving the chirp.
                constexpr float a = 0.62f;
                for (auto& z : springState)
                {
                    const float y = -a * x + z;
                    z = undenormalise(x + a * y);
                    x = y;
                }
            }
            if (v.shimmer)
                x += shimmerOut;

            for (int d = 0; d < 4; ++d)
            {
                const float delayed = diffusers[d].readInt(diffLen[d]);
                const float w = x + delayed * diffGain;
                diffusers[d].push(w);
                x = delayed - w * diffGain;
            }

            modPhase = wrapPhase(modPhase + modInc);
            if (v.allLinesMod && (counter++ & 511) == 0)
                driftTarget = rng.bipolar();
            drift += aDrift * (driftTarget - drift);

            float y[8];
            for (int k = 0; k < 8; ++k)
            {
                lengths[k] += (target[k] - lengths[k]) * lenSmooth;
                float len = lengths[k];
                if (k < 4 || v.allLinesMod)
                    len += modDepth * (fastSin(modPhase + (float) k * 0.125f) + (v.allLinesMod ? 0.3f * drift : 0.0f));
                y[k] = lines[k].read(len + modDepth * 1.6f + 2.0f);
                damp[k] += aDamp * (y[k] - damp[k]);
                y[k] = undenormalise(damp[k]) * gains[k];
            }

            hadamard8(y);

            for (int k = 0; k < 8; ++k)
                lines[k].push(y[k] + x * 0.35f);

            float wl = (y[0] - y[1] + y[2] - y[3] + y[4] - y[5]) * 0.5f;
            float wr = (y[1] + y[2] - y[4] - y[6] + y[7] - y[3]) * 0.5f;

            if (v.shimmer)
                shimmerOut = shimmerGain * fastTanh(pitchUpOctave(0.5f * (wl + wr)));
            if (v.grainy)
            {
                // 12-bit-style converters: a dithered noise floor under the tail
                // (dither keeps the decay smooth instead of stepping/gating).
                constexpr float q = 4096.0f;
                wl = std::round(wl * q + 0.5f * (rng.unipolar() - rng.unipolar())) / q;
                wr = std::round(wr * q + 0.5f * (rng.unipolar() - rng.unipolar())) / q;
            }

            wl = wl * lateGain + erL * earlyGain;
            wr = wr * lateGain + erR * earlyGain;
            if (band)
            {
                bandL += aBand * (wl - bandL);
                bandR += aBand * (wr - bandR);
                wl = bandL;
                wr = bandR;
            }
            lowL += aLow * (wl - lowL);
            lowR += aLow * (wr - lowR);
            wl -= lowL;
            wr -= lowR;

            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
            L[i] = L[i] * dryGain + (mid + side) * wetGain;
            R[i] = R[i] * dryGain + (mid - side) * wetGain;
        }
        lowL = undenormalise(lowL);
        lowR = undenormalise(lowR);
    }

private:
    static constexpr float kLineMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
    static constexpr float kEarlyMs[8] = { 4.3f, 7.9f, 11.7f, 17.3f, 23.1f, 29.9f, 37.7f, 46.1f };
    static constexpr float kEarlyGain[8] = { 0.8f, 0.72f, 0.62f, 0.55f, 0.46f, 0.4f, 0.33f, 0.28f };
    static constexpr int kShimmerWindow = 2048;

    float coeffFor(float hz) const { return 1.0f - std::exp(-kTwoPi * std::min(hz, 0.45f * fs) / fs); }

    /** Two-tap delay-line pitch shifter (+12 semitones) with triangular crossfades. */
    float pitchUpOctave(float x)
    {
        shimmerLine.push(x);
        shimmerPhase += 1.0f / (float) kShimmerWindow;
        if (shimmerPhase >= 1.0f)
            shimmerPhase -= 1.0f;
        float out = 0.0f;
        for (int t = 0; t < 2; ++t)
        {
            float ph = shimmerPhase + 0.5f * (float) t;
            if (ph >= 1.0f)
                ph -= 1.0f;
            // Reading speed 2: the delay shrinks by one sample per sample.
            const float delay = 2.0f + (1.0f - ph) * (float) kShimmerWindow;
            const float w = 1.0f - std::abs(2.0f * ph - 1.0f);
            out += shimmerLine.read(delay) * w;
        }
        return out;
    }

    static void hadamard8(float* v)
    {
        for (int len = 1; len < 8; len <<= 1)
            for (int i = 0; i < 8; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float a = v[j], b = v[j + len];
                    v[j] = a + b;
                    v[j + len] = a - b;
                }
        constexpr float norm = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < 8; ++i)
            v[i] *= norm;
    }

    float fs = 48000.0f;
    DelayLine lines[8];
    float lengths[8] {};
    bool lengthsValid = false;
    DelayLine diffusers[4];
    int diffLen[4] {};
    DelayLine predelay;
    DelayLine shimmerLine;
    float damp[8] {};
    float springState[24] {};
    float bandIn = 0.0f, bandL = 0.0f, bandR = 0.0f, lowL = 0.0f, lowR = 0.0f;
    float modPhase = 0.0f, drift = 0.0f, driftTarget = 0.0f, aDrift = 0.0f;
    float shimmerPhase = 0.0f, shimmerOut = 0.0f;
    uint32_t counter = 0;
    Random rng { 0x2eb1u };
};

} // namespace zsynth::dsp
