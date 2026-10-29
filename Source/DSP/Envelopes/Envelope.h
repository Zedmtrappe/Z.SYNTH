#pragma once

// Analogue-style ADSR envelope.
//
// Attack charges towards an overshoot target (1.3) so the curve is the convex
// RC shape of hardware envelopes and reaches 1.0 in the set time. Decay and
// release are exponential; their time is defined as the time to fall to
// -60 dB of the distance to the target, matching how hardware "time" knobs
// feel. Retriggers start from the current level, so there is never a jump.

#include "../Common/DspCommon.h"

namespace zsynth::dsp
{

/** Per-sample-rate coefficients for one ADSR, computed once per control block
    by the engine and shared by every voice. */
struct EnvelopeCoeffs
{
    float attackCoeff = 0.0f, attackBase = 0.0f;
    float decayCoeff = 0.0f, releaseCoeff = 0.0f;
    float sustain = 1.0f;

    static constexpr float kAttackTarget = 1.3f;

    void set(float attackSec, float decaySec, float sustainLevel, float releaseSec, float sampleRate)
    {
        // Attack: y = target - (target - y) * c reaches 1.0 from 0 after attackSec.
        const float attackSamples = std::max(1.0f, attackSec * sampleRate);
        attackCoeff = std::exp(-std::log(kAttackTarget / (kAttackTarget - 1.0f)) / attackSamples);
        attackBase = kAttackTarget * (1.0f - attackCoeff);

        constexpr float ln1000 = 6.90775527898f;
        decayCoeff   = std::exp(-ln1000 / std::max(1.0f, decaySec * sampleRate));
        releaseCoeff = std::exp(-ln1000 / std::max(1.0f, releaseSec * sampleRate));
        sustain = clampf(sustainLevel, 0.0f, 1.0f);
    }
};

class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void reset() { stage = Stage::Idle; level = 0.0f; }

    void noteOn(bool retriggerFromZero = false)
    {
        if (retriggerFromZero)
            level = 0.0f;
        stage = Stage::Attack;
    }

    void noteOff()
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    /** Ramps to zero quickly (used when a voice is stolen). */
    void forceFastRelease() { stage = Stage::Release; fastRelease = true; }

    bool isActive() const { return stage != Stage::Idle; }
    bool isReleasing() const { return stage == Stage::Release; }
    Stage getStage() const { return stage; }
    float getLevel() const { return level; }

    float process(const EnvelopeCoeffs& c)
    {
        switch (stage)
        {
            case Stage::Attack:
                level = c.attackBase + level * c.attackCoeff;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                level = c.sustain + (level - c.sustain) * c.decayCoeff;
                if (std::abs(level - c.sustain) < 1.0e-5f)
                {
                    level = c.sustain;
                    stage = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                // Follow sustain changes smoothly.
                level = c.sustain + (level - c.sustain) * 0.999f;
                break;

            case Stage::Release:
                level *= fastRelease ? kFastReleaseCoeff : c.releaseCoeff;
                if (level < 1.0e-5f)
                {
                    level = 0.0f;
                    stage = Stage::Idle;
                    fastRelease = false;
                }
                break;

            case Stage::Idle:
            default:
                break;
        }
        return level;
    }

    /** Skips ahead n samples (used for the control-rate copies of envelopes). */
    float processBlock(const EnvelopeCoeffs& c, int n)
    {
        for (int i = 0; i < n; ++i)
            process(c);
        return level;
    }

    void setFastReleaseCoeff(float sampleRate)
    {
        // ~3 ms to -100 dB: inaudible as a click, quick enough for voice stealing.
        kFastReleaseCoeff = std::exp(-11.5f / (0.003f * sampleRate));
    }

private:
    Stage stage = Stage::Idle;
    float level = 0.0f;
    bool fastRelease = false;
    float kFastReleaseCoeff = 0.99f;
};

} // namespace zsynth::dsp
