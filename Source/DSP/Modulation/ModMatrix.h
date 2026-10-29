#pragma once

// General modulation routing.
//
// A routing is (source, destination, amount). Sources produce values in
// -1..1 (bipolar) or 0..1 (unipolar). Every destination accumulates the sum of
// amount * source in normalised units; the consumer scales it with the
// destination's range below. The same accumulation is used by the matrix
// slots and by the LFO / Env 3 direct routes, so all paths behave identically.

#include "../../Parameters/ParamChoices.h"

namespace zsynth::dsp
{

struct ModRouting
{
    int source = 0;      // ModSource
    int destination = 0; // ModDest
    float amount = 0.0f; // -1..1
};

/** Destination scaling: what a normalised modulation of 1.0 means. */
namespace ModRange
{
    constexpr float kPitchSemis   = 24.0f;  // Pitch, Osc n Pitch
    constexpr float kCutoffSemis  = 96.0f;  // Cutoff and HPF: 8 octaves
    constexpr float kPulseWidth   = 0.45f;  // fraction of the cycle
    constexpr float kRateOctaves  = 4.0f;   // LFO rates
}

struct ModAccumulator
{
    float value[(int) ModDest::Count] {};

    void clear() { for (auto& v : value) v = 0.0f; }
    void add(ModDest d, float v) { value[(int) d] += v; }
    float operator[](ModDest d) const { return value[(int) d]; }
};

/** Destinations processed globally (effects) rather than per voice. */
inline bool isGlobalDestination(int d)
{
    return d >= (int) ModDest::FxDrive && d <= (int) ModDest::DelayFeedback;
}

/** Maps an LFO direct-route choice to a matrix destination. */
inline ModDest lfoDestToModDest(LfoDest d)
{
    switch (d)
    {
        case LfoDest::Pitch:        return ModDest::Pitch;
        case LfoDest::Cutoff:       return ModDest::Cutoff;
        case LfoDest::PulseWidth:   return ModDest::PulseWidth;
        case LfoDest::Amp:          return ModDest::AmpLevel;
        case LfoDest::Pan:          return ModDest::Pan;
        case LfoDest::WavetablePos: return ModDest::WavetablePos;
        case LfoDest::FmDepth:      return ModDest::FmDepth;
        case LfoDest::Osc2Pitch:    return ModDest::Osc2Pitch;
        case LfoDest::Resonance:    return ModDest::Resonance;
        default:                    return ModDest::Off;
    }
}

/** Maps an Env 3 direct-route choice to a matrix destination. */
inline ModDest env3DestToModDest(Env3Dest d)
{
    switch (d)
    {
        case Env3Dest::Pitch:        return ModDest::Pitch;
        case Env3Dest::Osc2Pitch:    return ModDest::Osc2Pitch;
        case Env3Dest::PulseWidth:   return ModDest::PulseWidth;
        case Env3Dest::Cutoff:       return ModDest::Cutoff;
        case Env3Dest::WavetablePos: return ModDest::WavetablePos;
        case Env3Dest::FmDepth:      return ModDest::FmDepth;
        case Env3Dest::Pan:          return ModDest::Pan;
        case Env3Dest::Noise:        return ModDest::NoiseLevel;
        default:                     return ModDest::Off;
    }
}

} // namespace zsynth::dsp
