#pragma once

// SONG LIBRARY: whole pattern-sequencer songs (the eight patterns, the sixteen
// song slots with their repeats, tempo and swing), saved as .zsong files and
// shipped as a few factory songs (public-domain pieces only).

#include "../Parameters/ParamTable.h"

#include <juce_core/juce_core.h>

#include <array>
#include <memory>
#include <vector>

namespace zsynth
{

struct SongData
{
    static constexpr int kPatterns = 8, kSlots = 16;
    juce::String name;
    float tempo = 120.0f; // BPM (master.tempo; a host's tempo wins when it has one)
    float swing = 50.0f;  // %
    // Each pattern: every parameter of both rows, from P::PseqAOn on.
    std::array<std::array<float, 2 * kPseqRowParams>, kPatterns> patterns {};
    std::array<int, kSlots> slots {};   // ZS_CH_PSEQ_SLOT index: 0 empty, 1..8 = A..H, 9 LOOP, 10 END
    std::array<int, kSlots> repeats {}; // ZS_CH_PSEQ_REPEAT index: 0 = x1 .. 15 = x16
    bool songMode = true;               // SONG on when it is loaded
};

/** An empty sequencer: every step of every pattern a rest, no song slots, SONG off. */
SongData blankSong();

/** The factory songs (public-domain pieces arranged for the two rows). */
const std::vector<SongData>& factorySongs();

std::unique_ptr<juce::XmlElement> songToXml(const SongData&);
/** False if the XML is not a song; missing values keep their defaults. */
bool songFromXml(const juce::XmlElement&, SongData&);

} // namespace zsynth
