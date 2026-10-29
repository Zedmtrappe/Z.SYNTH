#pragma once

// Control-rate LFO. Advanced once per control block (32 internal samples),
// which is far above the 50 Hz maximum rate. Destinations interpolate.

#include "../../Parameters/ParamChoices.h"
#include "../Common/DspCommon.h"

namespace zsynth::dsp
{

class Lfo
{
public:
    void setSeed(uint32_t seed) { rng.setSeed(seed); held = rng.bipolar(); nextHeld = rng.bipolar(); }

    void reset(float startPhaseCycles)
    {
        phase = startPhaseCycles - std::floor(startPhaseCycles);
        finished = false;
        held = rng.bipolar();
        nextHeld = rng.bipolar();
    }

    /** Advances by dtSeconds at freqHz and returns the bipolar value. */
    float advance(float freqHz, float dtSeconds, LfoWave wave, bool oneShot)
    {
        if (! finished)
        {
            phase += freqHz * dtSeconds;
            if (phase >= 1.0f)
            {
                if (oneShot)
                {
                    phase = 0.9999f;
                    finished = true;
                }
                else
                {
                    phase = wrapPhase(phase);
                    held = nextHeld;
                    nextHeld = rng.bipolar();
                }
            }
        }
        return valueAt(wave);
    }

    /** Value at the current phase plus an offset in cycles. */
    float valueAt(LfoWave wave, float offset = 0.0f) const
    {
        float p = phase + offset;
        p = wrapPhase(p);
        switch (wave)
        {
            case LfoWave::Sine:      return fastSin(p);
            case LfoWave::Triangle:  return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case LfoWave::SawUp:     return 2.0f * p - 1.0f;
            case LfoWave::RampDown:  return 1.0f - 2.0f * p;
            case LfoWave::Square:    return p < 0.5f ? 1.0f : -1.0f;
            case LfoWave::SampleHold: return held;
            case LfoWave::SmoothRandom:
            {
                const float s = p * p * (3.0f - 2.0f * p); // smoothstep between held values
                return held + (nextHeld - held) * s;
            }
            default: return 0.0f;
        }
    }

    float getPhase() const { return phase; }
    /** Hard-sets the phase (used to lock free-running LFOs to the host position). */
    void setPhase(float p) { phase = p - std::floor(p); finished = false; }

private:
    Random rng;
    float phase = 0.0f;
    float held = 0.0f, nextHeld = 0.0f;
    bool finished = false;
};

} // namespace zsynth::dsp
