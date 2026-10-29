#pragma once

#include "../Common/DspCommon.h"
#include "../Envelopes/Envelope.h"
#include "../FM/FmVoice.h"
#include "../Filters/Filters.h"
#include "../Modulation/Lfo.h"
#include "../Modulation/ModMatrix.h"
#include "../Oscillators/Oscillator.h"
#include "VoiceShared.h"

namespace zsynth::dsp
{

/** Everything needed to start (or restart) a voice. */
struct NoteStart
{
    int note = 60;
    float velocity = 1.0f;      // after the velocity curve, 0..1
    float unisonDetune = 0.0f;  // semitones
    float unisonPan = 0.0f;     // -1..1, before the spread amount
    float glideFrom = -1.0f;    // start pitch (MIDI note) or < 0 for no glide
    float glideTime = 0.0f;     // seconds
    bool randomPhase = false;
    uint32_t age = 0;
};

/** One physical voice. Voices never share mutable state with each other. */
class Voice
{
public:
    void prepare(float internalSampleRate, int voiceIndex);

    void start(const NoteStart& ns);
    /** Quickly fades out the current sound and then starts ns. */
    void stealAndStart(const NoteStart& ns);
    /** Mono/legato: changes pitch without retriggering envelopes. */
    void legatoTo(int note, float glideSeconds, uint32_t newAge);
    /** Mono retrigger: restarts envelopes from their current level. */
    void retrigger(int note, float velocity, float glideSeconds, uint32_t newAge);
    void release();
    void kill();

    /** Renders n (<= kControlBlock) internal samples, adding into left/right. */
    void render(float* left, float* right, int n, const VoiceShared& s);

    bool isActive() const { return active; }
    bool isReleasing() const { return ampEnv.isReleasing() && ! hasPending; }
    int getNote() const { return hasPending ? pending.note : note; }
    uint32_t getAge() const { return hasPending ? pending.age : age; }
    float getLevel() const { return ampEnv.getLevel(); }

    // Voice-manager bookkeeping.
    bool keyHeld = false;
    bool sustained = false;

    void setPolyAftertouch(float v) { polyAftertouch = v; }
    void setUnison(float detuneSemis, float pan) { unisonDetune = detuneSemis; unisonPan = pan; }

private:
    void begin(const NoteStart& ns);
    void updateControl(int n, const VoiceShared& s);
    float pitchForOsc(int i, const VoiceShared& s) const;

    // ---- configuration
    float sampleRate = 96000.0f;
    int index = 0;
    float vintageTune[3] {}, vintageCutoff = 0.0f, vintageLevel = 0.0f, vintagePan = 0.0f;

    // ---- note state
    bool active = false;
    bool hasPending = false;
    NoteStart pending;
    int note = 60;
    float velocity = 1.0f;
    uint32_t age = 0;
    float notePitch = 60.0f;   // current (gliding) pitch, MIDI note units
    float targetPitch = 60.0f;
    float glideTime = 0.0f;
    float unisonDetune = 0.0f, unisonPan = 0.0f;
    float randomValue = 0.0f;
    float polyAftertouch = 0.0f;
    bool firstBlock = true;
    bool firstBlockLfoReset = false;

    // ---- generators
    Oscillator osc[3];
    SubOscillator sub;
    NoiseGenerator noise;
    FmVoice fm;
    Envelope ampEnv, env2, env3;
    Lfo lfo[2];
    float lfoFade[2] {};
    float lfoValue[2] {};
    float lfoRateMod[2] {};
    Random rng;
    float drift[3] {}, driftTarget[3] {};
    float driftTimer = 0.0f;

    // ---- processing
    HighPass12 hpf;
    FilterUnit filter;
    float lastOsc3 = 0.0f;

    // ---- per-block interpolation endpoints
    float incStart[3] {}, incEnd[3] {};
    float baseIncStart = 0.0f, baseIncEnd = 0.0f;
    float gStart = 0.1f, gEnd = 0.1f;
    float hpfG = 0.0f;
    bool hpfOn = false;
    float gainStart = 0.0f, gainEnd = 0.0f;
    float panLStart = 0.7f, panLEnd = 0.7f, panRStart = 0.7f, panREnd = 0.7f;
    float level[3] {};
    float subLevel = 0.0f, noiseLevel = 0.0f;
    float fmGain = 0.0f;
    float ring[3] {}, fmAmt[3] {};
    float res = 0.0f, fltDrive = 0.0f;
    float wtMixStart[3] {}, wtMixEnd[3] {};
    int wtFrame[3] { -1, -1, -1 };
};

} // namespace zsynth::dsp
