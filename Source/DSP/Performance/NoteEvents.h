#pragma once

namespace zsynth::dsp
{

/** A note event produced by the arpeggiator or sequencer. velocity 0 = note off. */
struct GeneratedNote
{
    int note = 60;
    int velocity = 0;
    float glide = -1.0f; // >= 0 overrides the glide time (sequencer slides)
};

/** Fixed-capacity list, so generating events never allocates. */
struct GeneratedNoteList
{
    static constexpr int kCapacity = 64;
    GeneratedNote events[kCapacity];
    int count = 0;

    void clear() { count = 0; }
    void add(int note, int velocity, float glide = -1.0f)
    {
        if (count < kCapacity)
            events[count++] = GeneratedNote { note, velocity, glide };
    }
};

/** Host or internal timing for one audio block. */
struct TransportInfo
{
    double bpm = 120.0;
    bool hostTempo = false;
    bool playing = false;
    bool ppqValid = false;
    double ppqPosition = 0.0;
};

} // namespace zsynth::dsp
