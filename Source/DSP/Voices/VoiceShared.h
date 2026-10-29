#pragma once

// Settings shared by all voices for one control block. The engine derives
// these from the (smoothed) parameter snapshot once per block, so voices do no
// parameter conversion of their own and every voice sees identical values.

#include "../../Parameters/ParamChoices.h"
#include "../Envelopes/Envelope.h"
#include "../FM/FmVoice.h"
#include "../Modulation/ModMatrix.h"
#include "../Wavetable/WavetableBank.h"

namespace zsynth::dsp
{

constexpr int kMaxVoices = 32;
constexpr int kControlBlock = 32; // internal samples per modulation update (max)

struct VoiceShared
{
    float sampleRate = 96000.0f; // internal (possibly oversampled) rate
    float invSampleRate = 1.0f / 96000.0f;

    bool analogActive = true;
    bool fmActive = false;

    struct Osc
    {
        bool on = true;
        OscWave wave = OscWave::Saw;
        float semis = 0.0f;  // octave + semitone + fine, relative to the note
        float pulseWidth = 0.5f;
        float level = 0.8f;
        bool sync = false;
        float fm = 0.0f;
        float ring = 0.0f;
    } osc[3];

    float subLevel = 0.0f;
    SubWave subWave = SubWave::SquareMinus1;
    float noiseLevel = 0.0f;
    NoiseType noiseType = NoiseType::White;
    float drift = 0.0f;
    float mixDrive = 0.0f;

    const Wavetable* wavetable = nullptr;
    float wtPos = 0.0f;
    float wtSpread = 0.0f;

    FilterModel filterModel = FilterModel::Ladder;
    FilterType filterType = FilterType::LowPass;
    FilterSlope filterSlope = FilterSlope::Pole4;
    float cutoffNote = 100.0f;   // cutoff expressed as a (fractional) MIDI note
    float resonance = 0.0f;
    float filterDrive = 0.0f;
    float filterEnvSemis = 0.0f; // signed
    float keyTrack = 0.5f;
    float filterVelSemis = 0.0f;
    float filterLfoSemis = 0.0f;
    float hpfHz = 10.0f;

    EnvelopeCoeffs env[3];
    float ampVelocity = 0.5f;
    float env2Velocity = 0.0f;
    float env3Amount = 0.0f;
    ModDest env3Dest = ModDest::Off;

    struct LfoSettings
    {
        LfoWave wave = LfoWave::Sine;
        float rateHz = 1.0f;
        float phase = 0.0f;  // cycles
        float delaySec = 0.0f;
        float amount = 0.0f;
        LfoMode mode = LfoMode::Free;
        bool unipolar = false;
        ModDest dest = ModDest::Off;
    } lfo[2];
    float globalLfo[2] {}; // shared free-running LFO values (bipolar, unscaled)

    float ampLevel = 0.8f;
    float pan = 0.0f;
    float stereoSpread = 0.5f;
    float vintage = 0.0f;
    float masterTuneSemis = 0.0f;

    // Global controller sources (normalised).
    float pitchBendSemis = 0.0f;
    float pitchBend = 0.0f;   // -1..1
    float modWheel = 0.0f;
    float aftertouch = 0.0f;
    float expression = 1.0f;
    float macro = 0.0f;
    float seqValue = 0.0f;
    float sampleHold = 0.0f;

    FmShared fm;
    float fmLevel = 0.8f;

    ModRouting routings[8];
    int numRoutings = 0;
};

} // namespace zsynth::dsp
