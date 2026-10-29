#pragma once

// Original, procedurally generated wavetables with band-limited mipmaps.
//
// Each table holds kFrames single-cycle frames. Every frame is stored at
// kNumMips band-limits: mip m contains harmonics 1 .. (kMaxHarmonics >> m),
// so an oscillator can pick the richest mip whose top harmonic stays below
// Nyquist for the note being played. No commercial wavetables are used: every
// frame is computed from a mathematical description in WavetableBank.cpp.

#include <vector>

namespace zsynth::dsp
{

class Wavetable
{
public:
    static constexpr int kSize = 2048;           // samples per cycle
    static constexpr int kMaxHarmonics = kSize / 2;
    static constexpr int kNumMips = 11;          // 1024, 512, ... 1 harmonics
    static constexpr int kFrames = 32;

    /** Pointer to kSize + 1 samples (last = first, for interpolation). */
    const float* frame(int frameIndex, int mip) const
    {
        return data.data() + ((size_t) frameIndex * kNumMips + (size_t) mip) * (kSize + 1);
    }

    /** Selects the mip whose top harmonic is below Nyquist for a phase
        increment in cycles per sample. */
    static int mipForIncrement(float inc);

    std::vector<float> data;
};

class WavetableBank
{
public:
    /** Thread-safe lazily-built singleton. First call takes ~100-300 ms;
        the plug-in calls it from the constructor, never from audio. */
    static const WavetableBank& get();

    const Wavetable& table(int id) const;
    int numTables() const { return (int) tables.size(); }

private:
    WavetableBank();
    std::vector<Wavetable> tables;
};

} // namespace zsynth::dsp
