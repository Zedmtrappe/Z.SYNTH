#pragma once

// Power-of-two circular delay line with fractional (cubic Hermite) reads.
// Memory is allocated only in prepare().

#include "../Common/DspCommon.h"

#include <vector>

namespace zsynth::dsp
{

class DelayLine
{
public:
    void prepare(int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4)
            size <<= 1;
        buffer.assign((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    void push(float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Reads `delay` samples behind the most recent push (delay >= 1). */
    float read(float delay) const
    {
        delay = clampf(delay, 1.0f, (float) (mask - 3));
        const float readPos = (float) writePos - delay;
        const int i = (int) fastFloor(readPos);
        const float f = readPos - (float) i;

        const float xm1 = buffer[(size_t) ((i - 1) & mask)];
        const float x0  = buffer[(size_t) (i & mask)];
        const float x1  = buffer[(size_t) ((i + 1) & mask)];
        const float x2  = buffer[(size_t) ((i + 2) & mask)];

        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    /** Integer read (no interpolation). */
    float readInt(int delay) const { return buffer[(size_t) ((writePos - delay) & mask)]; }

    int capacity() const { return mask - 3; }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};

} // namespace zsynth::dsp
