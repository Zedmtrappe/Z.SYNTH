#pragma once

// Voice allocation, note priority, unison, sustain and stealing.
//
// Poly: every note takes `unison` physical voices from the first `polyLimit`
// slots. Free voices are chosen by the allocation mode; when none is free the
// oldest released voice is stolen first, then the oldest sustained one, then
// the oldest held one. A stolen voice fades over ~3 ms before restarting, so
// stealing never clicks.
//
// Mono / Legato: one note at a time on voices 0..unison-1, chosen from the
// held-key stack by note priority. Mono retriggers envelopes (from their
// current level) on every new key; Legato only when no key was held. Returning
// to a still-held note after a release is always legato.

#include "../../Parameters/ParamChoices.h"
#include "Voice.h"

namespace zsynth::dsp
{

struct VoiceSettings
{
    VoiceMode mode = VoiceMode::Poly;
    NotePriority priority = NotePriority::Last;
    VoiceAlloc alloc = VoiceAlloc::Rotate;
    int polyLimit = 16;
    int unison = 1;
    float detune = 0.0f;  // 0..1 -> +/-50 cents across the stack
    float glideTime = 0.0f;
    GlideMode glideMode = GlideMode::Always;
    VelocityCurve velocityCurve = VelocityCurve::Linear;
    float vintage = 0.0f;
};

class VoiceManager
{
public:
    void prepare(float internalSampleRate);
    void setSettings(const VoiceSettings& s);

    /** velocity 1..127. glideOverride >= 0 replaces the glide time for this note. */
    void noteOn(int note, int velocity, float glideOverride = -1.0f);
    void noteOff(int note);
    void setSustain(bool down);
    void polyAftertouch(int note, float value);

    /** Releases every note (envelopes run their release). */
    void allNotesOff();
    /** Silences everything immediately. */
    void killAll();

    void render(float* left, float* right, int n, const VoiceShared& shared);

    int activeVoiceCount() const;
    bool anyKeyHeld() const { return heldCount > 0; }
    int lastNote() const { return lastPlayedNote; }
    float lastVelocity() const { return lastPlayedVelocity; }

    const Voice& voice(int i) const { return voices[i]; }

    float curveVelocity(int velocity) const;

private:
    void polyNoteOn(int note, float vel, float glideTime);
    void polyNoteOff(int note);
    void monoNoteOn(int note, float vel, float glideTime);
    void monoNoteOff(int note);
    int monoSelect() const;
    int pickVoice(int note, const bool* excluded);
    int unisonCount() const;
    NoteStart makeStart(int note, float vel, int stackIndex, int stackSize, int voiceIndex, float glideFrom, float glideTime) const;

    Voice voices[kMaxVoices];
    VoiceSettings settings;
    bool settingsValid = false;

    bool keyDown[128] {};
    int heldCount = 0;
    int monoStack[128] {};
    int monoStackSize = 0;
    int monoCurrent = -1;
    bool monoGate = false;
    bool monoSustained = false;

    bool sustainDown = false;
    uint32_t ageCounter = 0;
    int rotateIndex = 0;
    int lastVoiceForNote[128] {};
    int lastPlayedNote = -1;
    float lastPlayedVelocity = 0.0f;
};

} // namespace zsynth::dsp
