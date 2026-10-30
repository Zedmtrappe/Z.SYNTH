#pragma once

// The JUCE AudioProcessor: the single host-facing object for the standalone
// app, VST3 and AU. It owns the authoritative parameter state (APVTS), the
// JUCE-free SynthEngine, MIDI learn and the preset library.

#include "../DSP/Engine/SynthEngine.h"
#include "../MIDI/MidiLearn.h"
#include "../Parameters/ParameterLayout.h"
#include "../Presets/PresetManager.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace zsynth
{

class ZSynthProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    /** userPresetDir: override for tests; empty = platform default. */
    explicit ZSynthProcessor(juce::File userPresetDir = {});
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
    const dsp::SynthEngine& getEngine() const { return engine; }
    void panic();

    /** Injects controller data from the on-screen wheels (thread-safe). */
    void setPitchWheel(float bipolar);
    void setModWheel(float unipolar);

    /** Parameter value in real units (thread-safe). */
    float getValue(int index) const { return rawValues[(size_t) index]->load(); }

    uint32_t getAudioCallbackCount() const { return audioCallbacks.load(); }
    /** Smoothed fraction of the real-time budget used by processBlock. */
    float getCpuLoad() const { return cpuLoad.load(); }

private:
    void timerCallback() override;
    void readTransport(dsp::TransportInfo& t);

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, kNumParams> rawValues {};
    float snapshot[kNumParams] {};

    dsp::SynthEngine engine;
    juce::MidiKeyboardState keyboardState;
    MidiLearn midiLearn;
    std::unique_ptr<PresetManager> presets;

    std::vector<dsp::MidiEvent> events;
    std::vector<float> monoScratch;
    static constexpr int kMaxEventsPerBlock = 4096;

    // On-screen wheels -> MIDI, consumed on the audio thread.
    std::atomic<int> pendingPitchWheel { -1 };
    std::atomic<int> pendingModWheel { -1 };
    std::atomic<uint32_t> audioCallbacks { 0 };
    std::atomic<float> cpuLoad { 0.0f };
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZSynthProcessor)
};

} // namespace zsynth
