#pragma once

// The JUCE AudioProcessor: the single host-facing object for the standalone
// app, VST3 and AU. It owns the authoritative parameter state (APVTS), the
// JUCE-free SynthEngine, MIDI learn and the preset library.

#include "../DSP/Engine/SynthEngine.h"
#include "../DSP/Tracking/PitchTracker.h"
#include "../DSP/Performance/Harmonizer.h"
#include "../DSP/Performance/PatternSequencer.h"
#include "../MIDI/MidiLearn.h"
#include "../Parameters/ParameterLayout.h"
#include "../Presets/PresetManager.h"
#include "AuditionPlayer.h"
#include "SongLibrary.h"

#include <optional>

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

    // ---- pattern sequencer (two rows: A and B; eight pattern memories)
    static constexpr int kPatterns = 8;
    int getPattern() const { return currentPattern; }
    /** Message thread: stores the rows on the panel into the current pattern and recalls `index`. */
    void selectPattern(int index);
    /** Message thread: row 0 = A, 1 = B. */
    void copyRow(int row);
    void pasteRow(int row);
    bool hasCopiedRow() const { return rowClipboardValid; }
    void clearRow(int row);
    /** The built-in phrases (in menu order) a row can load. */
    static juce::StringArray getPhraseNames();
    /** Loads a built-in phrase into a row (row A takes the top line, row B the bass line). */
    bool loadPhraseIntoRow(int phrase, int row);
    /** Loads the first bars of a standard MIDI file into a row, quantised to its steps. */
    juce::Result loadMidiFileIntoRow(const juce::File& file, int row);

    // ---- song library: whole songs (patterns, song slots and repeats, tempo, swing)
    static juce::StringArray getFactorySongNames();
    bool loadFactorySong(int index);
    juce::Result loadSongFile(const juce::File& file);
    juce::Result saveSongFile(const juce::File& file);
    /** Where saved songs live (beside the user presets). */
    juce::File getSongDirectory() const;
    /** The song last loaded or saved ("" for none). */
    juce::String getSongName() const { return songName; }
    SongData captureSong() const;
    /** Replaces the patterns, slots (and tempo and swing); SONG as saved, pattern A, back to the top. */
    void applySong(const SongData& song, bool withTempo = true);
    /** CLEAR ALL: every pattern empty, no song slots, SONG and RUN off. */
    void clearSong();
    /** Saving a preset stores the sequencer song with it (set by the save dialog). */
    void setIncludeSongWithPreset(bool include) { includeSongWithPreset = include; }
    /** The current preset was loaded or saved with a sequencer song. */
    bool currentPresetHasSong() const { return presetHasSong; }
    /** Tests: whether restoring a state empties the sequencer (default: only in the standalone app). */
    void setStartupBlanksSequencer(bool blank) { startupBlanksSequencer = blank; }
    /** The step a row is on (0..15), or -1 when stopped (any thread). */
    int getSeqStep(int row) const { return pseq.getShownStep(row); }
    /** Is the sequencer playing (RUN, or the host with HOST SYNC)? (any thread) */
    bool isSeqRunning() const { return seqRunning.load(std::memory_order_relaxed); }
    /** SONG mode: the slot playing (0..15), or -1 (any thread). */
    int getSongSlot() const { return pseq.getSongSlot(); }
    /** SONG mode: which time through the playing slot's repeats (0 = first). */
    int getSongPass() const { return pseq.getSongPass(); }
    /** SONG mode: the playing slot's repeats (1..16), or 1. */
    int getSongRepeats() const
    {
        const int s = getSongSlot();
        return s >= 0 ? juce::roundToInt(getValue(P::PseqRep1 + s)) + 1 : 1;
    }
    /** Sequencer back to the top (step 1 / song slot 1); keeps playing if it was. */
    void restartSequencer() { pseq.requestRestart(); }
    /** Plays a note briefly on a row's sound (choosing a step's note on the sequencer page). */
    void previewStepNote(int row, int note) { previewRequest.store(juce::jlimit(0, 1, row) * 128 + juce::jlimit(0, 127, note)); }

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

    // ---- changing presets while SPLIT / LAYER or HARMONY is on
    /** What happens to SPLIT / HARMONY when a preset without its own is loaded. */
    enum class KeepPolicy { Ask = 0, Keep, TurnOff };
    enum KeepItem { kKeepSplit = 1, kKeepHarmony = 2 };
    KeepPolicy getKeepPolicy(KeepItem item) const { return (KeepPolicy) (item == kKeepSplit ? splitPolicy.load() : harmPolicy.load()); }
    /** Sets (and remembers, across sessions) the policy for one item. */
    void setKeepPolicy(KeepItem item, KeepPolicy policy);
    /** Items (KeepItem bits) the editor should ask about after a preset load; clears them. */
    int takeKeepQuestion() { return keepQuestion.exchange(0); }
    /** Answer to the question: switch the item off (keeping needs nothing). */
    void turnOff(KeepItem item);
    /** LEARN: the next key played sets the split point (and does not sound). */
    void setLearningSplit(bool on) { learnedNote.store(-1); learningSplit.store(on); }
    bool isLearningSplit() const { return learningSplit.load(); }
    /** Message thread (the processor's timer calls it): applies a learned split key. */
    bool applyLearnedSplit();

    // ---- harmoniser
    /** DETECT: listen to what is played and set KEY / SCALE (message thread). */
    void startKeyDetect() { keyDetector.start(); }
    void stopKeyDetect() { keyDetector.stop(); }
    bool isDetectingKey() const { return keyDetector.isListening(); }
    int getKeyDetectNotes() const { return keyDetector.notesHeard(); }
    /** Message thread (the timer calls it): applies a detected key. */
    bool applyDetectedKey();

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
    // SPLIT / LAYER: a safety limiter on the two parts mixed (each part has its own).
    float mixLimiterGain = 1.0f, mixLimiterRelease = 0.999f;
    std::atomic<bool> learningSplit { false };
    std::atomic<int> learnedNote { -1 };
    std::atomic<bool> clearNoteMaps { false };
    void routeEvents(const float* globals);
    void harmonise(int n, const float* globals);
    std::array<dsp::Harmonizer, 2> harmonizers;
    dsp::KeyDetector keyDetector;
    std::vector<dsp::MidiEvent> harmScratch;
    void storeSound(const PresetData& p);
    std::atomic<int> splitPolicy { 0 }, harmPolicy { 0 }, keepQuestion { 0 };
    juce::File keepPolicyFile() const;
    void loadKeepPolicies();
    /** After a preset load: split and harmony from the preset, or (when the
        preset has none) kept, switched off or asked about, per the policy. */
    void applyPresetSetup(const PresetData& p);
    /** When saving: the other part's sound, if SPLIT or LAYER is on. */
    void captureSplitPart(PresetData& p);
    juce::MidiKeyboardState keyboardState;
    MidiLearn midiLearn;
    std::unique_ptr<PresetManager> presets;
    // Pattern sequencer.
    dsp::PatternSequencer pseq;
    std::array<std::array<std::atomic<float>, 2 * kPseqRowParams>, kPatterns> patterns {}; // read by SONG mode
    void fillRows(const float* block, int editPart, bool single, dsp::PseqRow rows[2]) const;
    std::atomic<int> currentPattern { 0 };
    std::atomic<bool> applyingPattern { false }; // the panel's pattern is being swapped: SONG reads memory
    // Note preview (a step's note, played when it is chosen on the sequencer page).
    std::atomic<int> previewRequest { -1 }; // row * 128 + note
    int previewNote = -1, previewMask = 0, previewLeft = 0;
    std::array<float, kPseqRowParams> rowClipboard {};
    bool rowClipboardValid = false;
    int pseqTranspose = 0;
    std::atomic<bool> seqRunning { false };
    void capturePattern(int index);
    void applyPattern(int index);
    void setParamValue(int index, float value);
    void importIntoRow(const AuditionPlayer::Clip& clip, int row);
    juce::String songName;
    bool includeSongWithPreset = false, presetHasSong = false;
    std::optional<bool> startupBlanksSequencer;

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
