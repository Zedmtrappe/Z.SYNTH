#pragma once

// Six-operator phase-modulation ("FM") voice.
//
// Each operator is a table sine with its own ADSR. Modulator outputs are
// scaled to a modulation index in *cycles*: an operator at level 1 with
// global depth 1 deviates the phase of its target by up to 2.5 cycles
// (~15.7 rad), enough for bright brass and metallic spectra. Operator level
// is squared for a perceptual taper. Velocity scales modulator indices only
// (timbre), not carriers (the amp envelope handles loudness).
//
// DX MODE swaps in DX7-style maths: operator levels are logarithmic (the
// level fader is 99 steps of 0.75 dB), envelopes move in decibels at a
// constant rate, the index and feedback ranges follow the DX7, and feedback
// scales with operator 6's level. Per-operator velocity sensitivity and
// keyboard rate/level scaling work in both modes.

#include "../Common/DspCommon.h"
#include "../Envelopes/Envelope.h"
#include "FmAlgorithms.h"

namespace zsynth::dsp
{

/** DX-mode envelope rates: per-sample gain multipliers (the level moves in
    decibels at a constant speed, like the DX7's logarithmic EG). */
struct DxEnvRates
{
    float attackLog = 0.0f;   // ln(gain step) per sample, rising -48 dB -> 0 dB in the attack time
    float decayLog = 0.0f;    // ln(gain step) per sample, falling 60 dB in the decay time
    float releaseLog = 0.0f;  // likewise for release

    static constexpr float kAttackFloor = 0.004f; // -48 dB: where a DX attack starts from silence

    void set(float attackSec, float decaySec, float releaseSec, float sampleRate)
    {
        constexpr float ln1000 = 6.90775527898f;
        attackLog = -std::log(kAttackFloor) / std::max(1.0f, attackSec * sampleRate);
        decayLog = -ln1000 / std::max(1.0f, decaySec * sampleRate);
        releaseLog = -ln1000 / std::max(1.0f, releaseSec * sampleRate);
    }
};

/** Per-control-block FM settings shared by all voices (computed by the engine). */
struct FmShared
{
    int algorithm = 0;
    bool dx = false;        // DX-style level, envelope, index and feedback scaling
    float ratio[6] { 1, 1, 1, 1, 1, 1 };
    float level[6] { 1, 1, 1, 1, 1, 1 };
    float velSens[6] {};    // per-operator velocity sensitivity 0..1
    float rateScale[6] {};  // keyboard rate scaling 0..1
    float levelScale[6] {}; // keyboard level scaling 0..1
    EnvelopeCoeffs env[6];
    DxEnvRates dxEnv[6];
    float depth = 0.5f;     // global index scaling 0..1 (may be modulated per voice)
    float feedback = 0.0f;  // 0..1
    float velocitySens = 0.5f;
};

/** Operator envelope with two behaviours sharing one state, so DX mode can be
    switched while notes sound. Clean: analogue-style ADSR (as Envelope).
    DX: attack, decay and release move at constant dB/s and stop exactly on
    their targets, which gives FM its characteristic "tine" and "bloom". */
class OpEnvelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void reset() { stage = Stage::Idle; level = 0.0f; fastRelease = false; }
    void noteOn() { stage = Stage::Attack; }
    void noteOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void forceFastRelease() { stage = Stage::Release; fastRelease = true; }
    bool isActive() const { return stage != Stage::Idle; }
    void setFastReleaseCoeff(float sampleRate) { fastCoeff = std::exp(-11.5f / (0.003f * sampleRate)); }

    /** Clean: c holds the (key-scaled) coefficients. DX: attackMul etc. are
        per-sample gain multipliers. */
    float process(const EnvelopeCoeffs& c, bool dx, float attackMul, float decayMul, float releaseMul)
    {
        switch (stage)
        {
            case Stage::Attack:
                if (dx)
                    level = std::max(level, DxEnvRates::kAttackFloor) * attackMul;
                else
                    level = c.attackBase + level * c.attackCoeff;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                if (dx)
                {
                    level = level > c.sustain ? level * decayMul : c.sustain;
                    if (level <= c.sustain)
                    {
                        level = c.sustain;
                        stage = Stage::Sustain;
                    }
                }
                else
                {
                    level = c.sustain + (level - c.sustain) * c.decayCoeff;
                    if (std::abs(level - c.sustain) < 1.0e-5f)
                    {
                        level = c.sustain;
                        stage = Stage::Sustain;
                    }
                }
                break;

            case Stage::Sustain:
                level = c.sustain + (level - c.sustain) * 0.999f;
                break;

            case Stage::Release:
                level *= fastRelease ? fastCoeff : (dx ? releaseMul : c.releaseCoeff);
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

private:
    Stage stage = Stage::Idle;
    float level = 0.0f;
    bool fastRelease = false;
    float fastCoeff = 0.99f;
};

class FmVoice
{
public:
    static constexpr float kMaxIndexCycles = 2.5f;
    static constexpr float kMaxFeedbackCycles = 0.35f;
    // DX mode: a modulator at full level deviates its target by 2 cycles (4 pi rad)
    // at the default depth of 0.5; feedback reaches 0.5 cycles and halves per step.
    static constexpr float kDxIndexCycles = 4.0f;
    static constexpr float kDxFeedbackCycles = 0.5f;
    static constexpr float kDxLevelRangeDb = 74.25f; // 99 steps of 0.75 dB
    static constexpr float kVelocityRangeDb = 30.0f;
    static constexpr float kLevelScaleDbPerOctave = 9.0f;

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

    /** Operator output gain for a level parameter 0..1 (before velocity/key scaling). */
    static float levelToGain(float level, bool dx)
    {
        if (dx)
            return level <= 0.0f ? 0.0f : dbToGain(-(1.0f - level) * kDxLevelRangeDb);
        return level * level;
    }

    /** Called once per control block: resolves per-voice indices and envelope rates.
        pitch is the voice's current MIDI note (key scaling is centred on C4 = 60). */
    void prepareBlock(const FmShared& s, float depthMod, float feedbackMod, float pitch)
    {
        const FmAlgorithm& alg = fmAlgorithm(s.algorithm);
        algorithm = &alg;
        dx = s.dx;
        const float depth = clampf(s.depth + depthMod, 0.0f, 1.0f);
        const float velScale = 1.0f - s.velocitySens + s.velocitySens * velocity;
        const float octaves = (pitch - 60.0f) / 12.0f;
        for (int i = 0; i < 6; ++i)
        {
            float gain = levelToGain(s.level[i], dx);
            const float db = s.velSens[i] * (velocity - 1.0f) * kVelocityRangeDb
                           - s.levelScale[i] * kLevelScaleDbPerOctave * std::max(0.0f, octaves);
            if (db < 0.0f)
                gain *= dbToGain(db);
            levelGain[i] = gain;
            isCarrier[i] = ((alg.carriers >> i) & 1u) != 0;
            if (isCarrier[i])
                outGain[i] = gain;
            else
                outGain[i] = gain * depth * (dx ? kDxIndexCycles : kMaxIndexCycles) * velScale;

            // Keyboard rate scaling: envelopes run faster up the keyboard.
            const float rateMul = s.rateScale[i] > 0.0f ? clampf(fastExp2(s.rateScale[i] * octaves), 0.125f, 8.0f) : 1.0f;
            coeffs[i] = s.env[i];
            if (dx)
            {
                attackMul[i] = std::exp(s.dxEnv[i].attackLog * rateMul);
                decayMul[i] = std::exp(s.dxEnv[i].decayLog * rateMul);
                releaseMul[i] = std::exp(s.dxEnv[i].releaseLog * rateMul);
            }
            else if (rateMul != 1.0f)
            {
                coeffs[i].attackCoeff = std::pow(coeffs[i].attackCoeff, rateMul);
                coeffs[i].attackBase = EnvelopeCoeffs::kAttackTarget * (1.0f - coeffs[i].attackCoeff);
                coeffs[i].decayCoeff = std::pow(coeffs[i].decayCoeff, rateMul);
                coeffs[i].releaseCoeff = std::pow(coeffs[i].releaseCoeff, rateMul);
            }
        }
        int carriers = 0;
        for (int i = 0; i < 6; ++i)
            carriers += isCarrier[i] ? 1 : 0;
        carrierNorm = 1.0f / std::sqrt((float) std::max(1, carriers));
        const float fb = clampf(s.feedback + feedbackMod, 0.0f, 1.0f);
        if (dx)
            feedback = fb <= 0.0f ? 0.0f : kDxFeedbackCycles * fastExp2((fb - 1.0f) * 7.0f);
        else
            feedback = fb * kMaxFeedbackCycles;
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

            const float e = env[op].process(coeffs[op], dx, attackMul[op], decayMul[op], releaseMul[op]);
            const float s = fastSin(phase[op] + pm) * e;
            out[op] = s * outGain[op];

            if (op == 5)
            {
                // DX feedback follows the operator's own output level; clean mode
                // feeds back the unscaled operator so FDBK works at any level.
                fb2 = fb1;
                fb1 = dx ? s * levelGain[5] : s;
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
    OpEnvelope env[6];
    EnvelopeCoeffs coeffs[6];
    float attackMul[6] {}, decayMul[6] {}, releaseMul[6] {};
    float phase[6] {};
    float outGain[6] {};
    float levelGain[6] {};
    bool isCarrier[6] {};
    bool dx = false;
    float carrierNorm = 1.0f;
    float feedback = 0.0f;
    float fb1 = 0.0f, fb2 = 0.0f;
    float velocity = 1.0f;
};

} // namespace zsynth::dsp
