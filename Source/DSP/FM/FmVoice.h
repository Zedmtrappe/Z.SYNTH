#pragma once

// Six-operator phase-modulation ("FM") voice.
//
// Each operator is a table sine with its own ADSR. Modulator outputs are
// scaled to a modulation index in *cycles*: an operator at level 1 with
// global depth 1 deviates the phase of its target by up to 2.5 cycles
// (~15.7 rad), enough for bright brass and metallic spectra. Operator level
// is squared for a perceptual taper. Velocity scales modulator indices only
// (timbre), not carriers (the amp envelope handles loudness).

#include "../Common/DspCommon.h"
#include "../Envelopes/Envelope.h"
#include "FmAlgorithms.h"

namespace zsynth::dsp
{

/** Per-control-block FM settings shared by all voices (computed by the engine). */
struct FmShared
{
    int algorithm = 0;
    float ratio[6] { 1, 1, 1, 1, 1, 1 };
    float level[6] { 1, 1, 1, 1, 1, 1 };
    EnvelopeCoeffs env[6];
    float depth = 0.5f;     // global index scaling 0..1 (may be modulated per voice)
    float feedback = 0.0f;  // 0..1
    float velocitySens = 0.5f;
};

class FmVoice
{
public:
    static constexpr float kMaxIndexCycles = 2.5f;
    static constexpr float kMaxFeedbackCycles = 0.35f;

    void setFastRelease(float sampleRate) { for (auto& e : env) e.setFastReleaseCoeff(sampleRate); }

    /** resetPhases gives DX-style key sync (all operators start at phase 0),
        which makes attacks consistent; legato notes keep running phases. */
    void noteOn(float vel, bool resetPhases)
    {
        velocity = vel;
        for (int i = 0; i < 6; ++i)
        {
            env[i].noteOn();
            if (resetPhases)
                phase[i] = 0.0f;
        }
        if (resetPhases)
            fb1 = fb2 = 0.0f;
    }

    void noteOff() { for (auto& e : env) e.noteOff(); }
    void kill()    { for (auto& e : env) e.reset(); fb1 = fb2 = 0.0f; }
    void forceFastRelease() { for (auto& e : env) e.forceFastRelease(); }

    bool isActive() const
    {
        for (const auto& e : env)
            if (e.isActive())
                return true;
        return false;
    }

    /** Called once per control block: resolves per-voice indices. */
    void prepareBlock(const FmShared& s, float depthMod, float feedbackMod)
    {
        const FmAlgorithm& alg = fmAlgorithm(s.algorithm);
        algorithm = &alg;
        const float depth = clampf(s.depth + depthMod, 0.0f, 1.0f);
        const float velScale = 1.0f - s.velocitySens + s.velocitySens * velocity;
        for (int i = 0; i < 6; ++i)
        {
            const float lvl = s.level[i] * s.level[i];
            if ((alg.carriers >> i) & 1u)
                outGain[i] = lvl;
            else
                outGain[i] = lvl * depth * kMaxIndexCycles * velScale;
            isCarrier[i] = ((alg.carriers >> i) & 1u) != 0;
        }
        int carriers = 0;
        for (int i = 0; i < 6; ++i)
            carriers += isCarrier[i] ? 1 : 0;
        carrierNorm = 1.0f / std::sqrt((float) std::max(1, carriers));
        feedback = clampf(s.feedback + feedbackMod, 0.0f, 1.0f) * kMaxFeedbackCycles;
        shared = &s;
    }

    /** baseInc: fundamental increment in cycles/sample. */
    float process(float baseInc)
    {
        float out[6];
        float sum = 0.0f;
        for (int op = 5; op >= 0; --op)
        {
            float pm = 0.0f;
            const uint8_t mods = algorithm->modulators[op];
            if (mods != 0)
                for (int m = op + 1; m < 6; ++m)
                    if ((mods >> m) & 1u)
                        pm += out[m];
            if (op == 5)
                pm += feedback * (fb1 + fb2) * 0.5f;

            const float e = env[op].process(shared->env[op]);
            const float s = fastSin(phase[op] + pm) * e;
            out[op] = s * outGain[op];

            if (op == 5)
            {
                fb2 = fb1;
                fb1 = s;
            }
            if (isCarrier[op])
                sum += out[op];

            phase[op] += baseInc * shared->ratio[op];
            phase[op] = wrapPhase(phase[op]);
        }
        return sum * carrierNorm;
    }

private:
    const FmShared* shared = nullptr;
    const FmAlgorithm* algorithm = &fmAlgorithm(0);
    Envelope env[6];
    float phase[6] {};
    float outGain[6] {};
    bool isCarrier[6] {};
    float carrierNorm = 1.0f;
    float feedback = 0.0f;
    float fb1 = 0.0f, fb2 = 0.0f;
    float velocity = 1.0f;
};

} // namespace zsynth::dsp
