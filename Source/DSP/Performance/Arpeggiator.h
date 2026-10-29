#pragma once

// Tempo-synchronised arpeggiator.
//
// Timing model: the engine asks samplesToNextEvent(), ends its render segment
// there, calls fireDueEvents() (which emits note on/offs for "now"), then
// advance()s by the rendered length. Every arpeggio note is therefore placed
// on its exact sample.

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"
#include "NoteEvents.h"

namespace zsynth::dsp
{

class Arpeggiator
{
public:
    struct Settings
    {
        ArpMode mode = ArpMode::Up;
        int division = 13;
        int octaves = 1;
        float gate = 0.5f;
        float swing = 0.0f;
        bool hold = false;
        double bpm = 120.0;
    };

    void prepare(double sampleRate);
    void reset();
    void setSettings(const Settings& s);

    void noteOn(int note, int velocity);
    void noteOff(int note);

    /** Aligns the step grid to the host's musical position (call per block). */
    void syncToHost(double ppqPosition);

    void fireDueEvents(GeneratedNoteList& out);
    int samplesToNextEvent() const;
    void advance(int samples);

    /** Emits note-offs for anything sounding and clears all state. */
    void stop(GeneratedNoteList& out);

    bool hasNotes() const { return poolSize > 0; }

private:
    void buildPattern();
    double stepLengthSamples(long long stepIndex) const;
    void releaseSounding(GeneratedNoteList& out);

    double sampleRate = 48000.0;
    Settings settings;
    Random rng { 0xA4F1u };

    // Notes in press order (with velocities), and the derived playing pattern.
    int pool[128] {};
    int poolVel[128] {};
    int poolSize = 0;
    int physicalDown = 0;
    bool keyDown[128] {};

    int pattern[512] {};
    int patternVel[512] {};
    int patternSize = 0;
    int patternPos = 0;
    int upDownDir = 1;

    int sounding[128] {};
    int soundingCount = 0;

    double toNextStep = 0.0;
    double toNoteOff = -1.0;
    long long stepCounter = 0;
    bool running = false;
};

} // namespace zsynth::dsp
