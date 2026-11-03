#pragma once

// The JUCE AudioProcessor: the single host-facing object for the standalone
// app, VST3 and AU. It owns the authoritative parameter state (APVTS), the
// JUCE-free SynthEngine, MIDI learn and the preset library.

#include "../DSP/Engine/SynthEngine.h"
#include "../DSP/Tracking/PitchTracker.h"
#include "../MIDI/MidiLearn.h"
#include "../Parameters/ParameterLayout.h"
#include "../Presets/PresetManager.h"
#include "AuditionPlayer.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace zsynth
{

class ZSynthProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    /** Audio input for PITCH TRACK: the standalone app has one (the plug-ins don't). */
    enum class AudioInput { Auto, Enabled, Disabled };

    /** userPresetDir: override for tests; empty = platform default. */
    explicit ZSynthProcessor(juce::File userPresetDir = {}, AudioInput input = AudioInput::Auto);
    ~ZSynthProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ZYNTH"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- ZYNTH API used by the editor
    juce::AudioProcessorValueTreeState& getState() { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    PresetManager& getPresets() { return *presets; }
    MidiLearn& getMidiLearn() { return midiLearn; }
    /** The engine of the part on the panel (meters, displays). */
    const dsp::SynthEngine& getEngine() const { return engines[(size_t) getEditPart()]; }
    const dsp::SynthEngine& getPartEngine(int part) const { return engines[(size_t) juce::jlimit(0, 1, part)]; }
    void panic();

    /** Injects controller data from the on-screen wheels (thread-safe). */
    void setPitchWheel(float bipolar);
    void setModWheel(float unipolar);
    void setSustainPedal(bool down);

    /** Parameter value in real units (thread-safe). */
    float getValue(int index) const { return rawValues[(size_t) index]->load(); }

    // ---- audition player
    AuditionPlayer& getAudition() { return audition; }
    /** Names of the built-in audition clips (in menu order). */
    static juce::StringArray getBuiltInClipNames();
    /** Loads built-in clip `index` (keeps play/stop state). */
    bool loadBuiltInClip(int index);
    /** Loads a standard MIDI file from disk. */
    juce::Result loadAuditionFile(const juce::File& file);

    // ---- pitch track (audio in -> notes)
    bool hasAudioInput() const { return getTotalNumInputChannels() > 0; }
    const dsp::PitchTracker& getTracker() const { return tracker; }
    bool isTracking() const { return getValue(P::TrkOn) >= 0.5f && hasAudioInput(); }

    // ---- keyboard split / layer: two complete sounds
    // The panel (and the host's parameters) always show the part being edited;
    // the other part's sound is kept here and plays alongside.
    static constexpr int kUpper = 0, kLower = 1;
    int getEditPart() const { return editPart.load(std::memory_order_acquire); }
    /** Message thread: puts part `part` on the panel (its sound and preset name swap in). */
    void setEditPart(int part);
    /** Preset name of a part. */
    juce::String getPartName(int part) const;
    /** LEARN: the next key played sets the split point (and does not sound). */
    void setLearningSplit(bool on) { learnedNote.store(-1); learningSplit.store(on); }
    bool isLearningSplit() const { return learningSplit.load(); }
    /** Message thread (the processor's timer calls it): applies a learned split key. */
    bool applyLearnedSplit();

    uint32_t getAudioCallbackCount() const { return audioCallbacks.load(); }
    /** Smoothed fraction of the real-time budget used by processBlock. */
    float getCpuLoad() const { return cpuLoad.load(); }

private:
    static BusesProperties busesFor(AudioInput input);
    void timerCallback() override;
    void readTransport(dsp::TransportInfo& t);

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, kNumParams> rawValues {};
    float snapshot[kNumParams] {};

    // Part 0 = UPPER, part 1 = LOWER. `stored` holds the sound of the part not on the panel.
    std::array<dsp::SynthEngine, 2> engines;
    std::array<std::atomic<float>, kNumParams> stored {};
    float partParams[2][kNumParams] {};
    std::atomic<int> editPart { kUpper };
    std::atomic<uint32_t> swapSequence { 0 }; // odd while setEditPart is swapping
    juce::String storedName { "Init" }, storedCategory { "Init" };
    juce::CriticalSection storedLock;
    std::array<std::vector<dsp::MidiEvent>, 2> partEvents;
    int8_t noteMap[2][16][128] {};             // played key -> sounding note per part (-1 = none)
    std::vector<float> partL, partR;
    int partTail[2] {};                        // samples a quiet part keeps running (effect tails)
    float partGain[2] { 1.0f, 1.0f };
    std::atomic<bool> learningSplit { false };
    std::atomic<int> learnedNote { -1 };
    std::atomic<bool> clearNoteMaps { false };
    void routeEvents(const float* globals);
    void storeSound(const PresetData& p);
    juce::MidiKeyboardState keyboardState;
    MidiLearn midiLearn;
    std::unique_ptr<PresetManager> presets;
    AuditionPlayer audition;

    std::vector<dsp::MidiEvent> events;
    std::vector<float> monoScratch;
    std::vector<float> trackInput;
    dsp::PitchTracker tracker;
    bool wasTracking = false;
    static constexpr int kMaxEventsPerBlock = 4096;

    // On-screen wheels -> MIDI, consumed on the audio thread.
    std::atomic<int> pendingPitchWheel { -1 };
    std::atomic<int> pendingModWheel { -1 };
    std::atomic<int> pendingSustain { -1 };
    std::atomic<uint32_t> audioCallbacks { 0 };
    std::atomic<float> cpuLoad { 0.0f };
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZSynthProcessor)
};

} // namespace zsynth
