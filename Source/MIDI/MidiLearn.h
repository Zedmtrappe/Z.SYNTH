#pragma once

// MIDI learn: maps incoming controller numbers to parameters.
//
// Audio thread: process() reads the map (atomics) and pushes value changes
// into a lock-free FIFO; learn assignments are also written atomically.
// Message thread: dispatchPending() (called from a timer) applies the values
// through the APVTS so host automation and the GUI see them like any other
// change. The map is saved with the plug-in state and in the standalone's
// settings file.

#include "../Parameters/ParamTable.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace zsynth
{

class MidiLearn
{
public:
    MidiLearn();

    /** Audio thread. */
    void process(const juce::MidiBuffer& midi);

    /** Message thread: applies queued controller values. Returns true if a mapping changed. */
    bool dispatchPending(juce::AudioProcessorValueTreeState& state);

    void startLearning(int paramIndex) { learnTarget.store(paramIndex); }
    void cancelLearning() { learnTarget.store(-1); }
    int getLearningTarget() const { return learnTarget.load(); }

    void clearMapping(int paramIndex);
    void clearAll();
    /** Controller mapped to the parameter, or -1. */
    int getControllerFor(int paramIndex) const;

    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);

private:
    std::atomic<int> ccToParam[128];
    std::atomic<int> learnTarget { -1 };
    std::atomic<bool> mappingChanged { false };

    struct Pending { int param; float normalised; };
    juce::AbstractFifo fifo { 256 };
    Pending pending[256];
};

} // namespace zsynth
