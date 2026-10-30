#pragma once

// MIDI file player for auditioning sounds: plays a clip (built-in or a loaded
// standard MIDI file) into the synth alongside live input, so the panel can be
// tweaked while it plays.
//
// All tracks and channels of a file are merged onto one channel (the synth's
// receive channel), so a file plays as a single part: polyphonic or
// monophonic, as the current patch's voice mode decides. Notes, sustain
// pedal, modulation wheel, expression and pitch bend are kept.
//
// THREADING. Clips are parsed on the message thread and handed to the audio
// thread through a try-lock (the audio thread never waits). Replaced clips
// are released on the message thread. Transport state is atomic.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <bitset>
#include <memory>

namespace zsynth
{

class AuditionPlayer
{
public:
    struct Event
    {
        double seconds;
        uint8_t status, data1, data2;
    };

    struct Clip
    {
        juce::String name;
        std::vector<Event> events; // sorted by time, channel nibble cleared
        double lengthSeconds = 0.0; // loop length (end of the longest track)
        double bpm = 120.0;
        int notes = 0;
    };

    /** Parses a standard MIDI file. Returns nullptr (and an error) if it holds no notes. */
    static std::unique_ptr<Clip> parse(const void* data, size_t size, const juce::String& name, juce::String& error);

    /** Message thread: queues a clip for playback (keeps the transport state). */
    void setClip(std::unique_ptr<Clip> clip);
    /** Message thread: frees clips the audio thread has finished with. */
    void collectGarbage();

    void play() { restartRequested = true; playing = true; }
    void stop() { playing = false; }
    bool isPlaying() const { return playing.load(); }
    void setLooping(bool shouldLoop) { looping = shouldLoop; }
    bool isLooping() const { return looping.load(); }

    /** 0..1 position in the clip, for the display. */
    float getPosition() const { return position.load(); }
    juce::String getClipName() const;
    double getClipBpm() const;
    double getClipLength() const;

    /** Audio thread: adds this block's events to `midi`. `channel` is 1..16. */
    void render(juce::MidiBuffer& midi, int numSamples, double sampleRate, int channel);

private:
    void releaseHeldNotes(juce::MidiBuffer& midi, int sampleOffset, int channel);

    juce::SpinLock lock;
    std::unique_ptr<Clip> incoming, active, retired;
    juce::String clipName;
    double clipBpm = 0.0, clipLength = 0.0;

    std::atomic<bool> playing { false }, looping { true }, restartRequested { false };
    std::atomic<float> position { 0.0f };

    // Audio-thread state.
    bool wasPlaying = false;
    double playhead = 0.0; // seconds
    size_t nextEvent = 0;
    std::bitset<128> held;
    bool sustainDown = false;
    int lastChannel = 1;
};

} // namespace zsynth
