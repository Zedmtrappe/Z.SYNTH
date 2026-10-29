#pragma once

// =============================================================================
//  SynthEngine — the complete Z.SYNTH sound engine, independent of JUCE, the
//  GUI and any plug-in format.
//
//  The host layer calls process() with a snapshot of parameter values (real
//  units, indexed by zsynth::P) and the block's raw MIDI events. The engine:
//    1. renders voices at the internal (1x or 2x) rate in segments that end at
//       every MIDI event, arpeggiator/sequencer event and control-rate tick,
//    2. decimates to the output rate,
//    3. runs the effects chain and output stage.
//
//  Real-time safety: all memory is allocated in prepare(); process() never
//  allocates, locks or performs I/O. Status for the GUI is published through
//  atomics.
// =============================================================================

#include "../../Parameters/ParamTable.h"
#include "../Common/DspCommon.h"
#include "../Common/HalfBand.h"
#include "../Effects/DriveAndOutput.h"
#include "../Effects/ModulationEffects.h"
#include "../Effects/TimeEffects.h"
#include "../Performance/Arpeggiator.h"
#include "../Performance/NoteEvents.h"
#include "../Performance/StepSequencer.h"
#include "../Voices/VoiceManager.h"

#include <atomic>
#include <vector>

namespace zsynth::dsp
{

/** A short MIDI message at a sample offset within the block. */
struct MidiEvent
{
    int offset = 0;
    uint8_t status = 0, data1 = 0, data2 = 0;
};

class SynthEngine
{
public:
    SynthEngine();

    void prepare(double sampleRate, int maxBlockSize);

    void process(float* left, float* right, int numSamples,
                 const float* params, const MidiEvent* events, int numEvents,
                 const TransportInfo& transport);

    /** Thread-safe: requests an immediate silence of all voices and effect tails. */
    void requestPanic() { panicRequested.store(true, std::memory_order_relaxed); }

    // ---- status for the GUI (thread-safe reads)
    int getActiveVoices() const      { return activeVoices.load(std::memory_order_relaxed); }
    float getPeak(int channel) const { return peaks[channel].load(std::memory_order_relaxed); }
    int getSeqStep() const           { return seqStepAtomic.load(std::memory_order_relaxed); }
    uint32_t getMidiActivity() const { return midiCounter.load(std::memory_order_relaxed); }
    int getOversampling() const      { return osFactor; }
    /** Count of blocks in which NaN/Inf was detected and suppressed. */
    uint32_t getBadSampleEvents() const { return badBlocks.load(std::memory_order_relaxed); }

    double getSampleRate() const { return baseRate; }

private:
    void configureOversampling(int factor);
    void processChunk(float* left, float* right, int n, const MidiEvent* events, int numEvents,
                      int chunkOffset, const TransportInfo& transport);
    void handleMidi(const MidiEvent& e);
    void noteOnIn(int channel, int note, int velocity);
    void noteOffIn(int channel, int note);
    void dispatchGenerated();
    void updateBlockSettings(const TransportInfo& transport);
    void smoothParams(float dtSeconds);
    void computeShared(int internalSamples);
    void runEffects(float* left, float* right, int n);
    float p(int index) const { return smoothed[index]; }
    int pi(int index) const { return (int) std::lround(smoothed[index]); }
    bool pb(int index) const { return smoothed[index] >= 0.5f; }

    double baseRate = 48000.0;
    int maxBlock = 512;
    int osFactor = 2;
    float internalRate = 96000.0f;

    const float* targets = nullptr;
    float smoothed[kNumParams] {};
    bool isLogParam[kNumParams] {};
    bool isContinuous[kNumParams] {};
    bool smoothedValid = false;

    VoiceShared shared;
    VoiceSettings voiceSettings;
    VoiceManager voices;
    Arpeggiator arp;
    StepSequencer seq;
    GeneratedNoteList generated;
    Lfo globalLfo[2];
    Random globalRng { 0xC0FFEEu };
    float sampleHoldValue = 0.0f;
    uint32_t lastSeqTick = 0;

    // Routing state that must stay consistent between note-on and note-off.
    enum class Route { Voices, Arp, Seq };
    Route route = Route::Voices;
    int playedNote[16][128];

    // Controllers.
    float pitchBend = 0.0f, modWheel = 0.0f, aftertouch = 0.0f, expression = 1.0f;
    double bpm = 120.0;

    // Oversampled voice bus.
    std::vector<float> busL, busR;
    HalfBandDecimator decimL, decimR;

    // Effects.
    Drive drive;
    Phaser phaser;
    Flanger flanger;
    Chorus chorus;
    Ensemble ensemble;
    StereoDelay delay;
    Reverb reverb;
    OutputStage output;
    // Each effect fades in/out over 20 ms when switched, so toggling never clicks.
    float fades[7] {};
    std::vector<float> fxTempL, fxTempR;

    std::atomic<bool> panicRequested { false };
    std::atomic<int> activeVoices { 0 };
    std::atomic<float> peaks[2] { 0.0f, 0.0f };
    std::atomic<int> seqStepAtomic { 0 };
    std::atomic<uint32_t> midiCounter { 0 };
    std::atomic<uint32_t> badBlocks { 0 };
};

} // namespace zsynth::dsp
