#pragma once

// 16-step sequencer.
//
// The step clock always runs (it also clocks the S&H modulation source).
//   Mod mode : the current step value (-1..1) is the "Seq Step" matrix source;
//              steps with slide ramp smoothly to the next value.
//   Note mode: while a key is held, the pattern plays monophonically,
//              transposed from that key by each step's value (+/-12 st).
//              Gate off = rest, accent = full velocity, slide = the next step
//              starts before this one ends, with a short glide (acid style).
// The clock restarts at step 1 when a key is pressed with no other key held,
// unless the host transport is running (then it follows the song position).

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"
#include "NoteEvents.h"

namespace zsynth::dsp
{

class StepSequencer
{
public:
    static constexpr int kSteps = 16;
    static constexpr float kSlideTime = 0.07f;

    struct Settings
    {
        SeqMode mode = SeqMode::Off;
        int division = 13;
        int length = 16;
        float gate = 0.6f;
        float swing = 0.0f;
        double bpm = 120.0;
        float values[kSteps] {};
        bool gates[kSteps] {};
        bool accents[kSteps] {};
        bool slides[kSteps] {};
    };

    void prepare(double sampleRate);
    void reset();
    void setSettings(const Settings& s) { settings = s; settings.length = std::clamp(settings.length, 1, kSteps); }

    void noteOn(int note, int velocity);
    void noteOff(int note);
    void syncToHost(double ppqPosition);
    /** While the host transport runs, key presses do not restart the clock. */
    void setHostRunning(bool running) { hostSynced = running; }

    void fireDueEvents(GeneratedNoteList& out);
    int samplesToNextEvent() const;
    void advance(int samples);
    void stop(GeneratedNoteList& out);

    /** Current modulation value (-1..1); 0 when the sequencer is off. */
    float currentValue() const;
    int currentStep() const { return step; }
    /** Increments once per step; used to clock S&H. */
    uint32_t tickCount() const { return ticks; }
    /** True when any key is held (or was pressed) since the clock restarted. */
    bool keysHeld() const { return heldCount > 0; }

private:
    double stepLengthSamples(long long index) const;
    void tick(GeneratedNoteList& out);
    int baseNote() const { return heldCount > 0 ? held[heldCount - 1] : -1; }

    double sampleRate = 48000.0;
    Settings settings;

    int held[128] {};
    int heldVel[128] {};
    int heldCount = 0;

    int step = 0;
    int nextStep = 0;
    int prevStep = 0;
    bool hostSynced = false;
    long long stepCounter = 0;
    double toNextStep = 0.0;
    double currentStepLength = 1.0;
    double toNoteOff = -1.0;
    int soundingNote = -1;
    uint32_t ticks = 0;
};

} // namespace zsynth::dsp
