#include "Arpeggiator.h"

#include <algorithm>

namespace zsynth::dsp
{

void Arpeggiator::prepare(double sr)
{
    sampleRate = sr;
    reset();
}

void Arpeggiator::reset()
{
    poolSize = 0;
    physicalDown = 0;
    for (auto& k : keyDown)
        k = false;
    patternSize = 0;
    patternPos = 0;
    upDownDir = 1;
    soundingCount = 0;
    toNextStep = 0.0;
    toNoteOff = -1.0;
    stepCounter = 0;
    running = false;
}

void Arpeggiator::setSettings(const Settings& s)
{
    const bool rebuild = s.mode != settings.mode || s.octaves != settings.octaves;
    settings = s;
    settings.octaves = std::clamp(settings.octaves, 1, 4);
    if (rebuild)
        buildPattern();
}

void Arpeggiator::noteOn(int note, int velocity)
{
    if (! keyDown[note])
    {
        keyDown[note] = true;
        ++physicalDown;
    }

    // With hold on, the first key of a new chord replaces the latched chord.
    if (settings.hold && physicalDown == 1)
        poolSize = 0;

    for (int i = 0; i < poolSize; ++i)
        if (pool[i] == note)
            return;
    if (poolSize < 128)
    {
        pool[poolSize] = note;
        poolVel[poolSize] = velocity;
        ++poolSize;
    }
    buildPattern();

    if (! running)
    {
        running = true;
        toNextStep = 0.0;
        stepCounter = 0;
        patternPos = 0;
        upDownDir = 1;
    }
}

void Arpeggiator::noteOff(int note)
{
    if (keyDown[note])
    {
        keyDown[note] = false;
        --physicalDown;
    }
    if (settings.hold)
        return;

    int w = 0;
    for (int i = 0; i < poolSize; ++i)
        if (pool[i] != note)
        {
            pool[w] = pool[i];
            poolVel[w] = poolVel[i];
            ++w;
        }
    poolSize = w;
    buildPattern();

    if (poolSize == 0)
        toNoteOff = 0.0; // release whatever is sounding right away
}

void Arpeggiator::buildPattern()
{
    int notes[128], vels[128];
    const int n = poolSize;
    for (int i = 0; i < n; ++i)
    {
        notes[i] = pool[i];
        vels[i] = poolVel[i];
    }

    if (settings.mode != ArpMode::AsPlayed)
    {
        // Insertion sort, ascending (keeps velocities paired).
        for (int i = 1; i < n; ++i)
            for (int j = i; j > 0 && notes[j - 1] > notes[j]; --j)
            {
                std::swap(notes[j - 1], notes[j]);
                std::swap(vels[j - 1], vels[j]);
            }
    }

    patternSize = 0;
    for (int o = 0; o < settings.octaves; ++o)
        for (int i = 0; i < n; ++i)
        {
            const int note = notes[i] + 12 * o;
            if (note <= 127 && patternSize < 512)
            {
                pattern[patternSize] = note;
                patternVel[patternSize] = vels[i];
                ++patternSize;
            }
        }

    if (settings.mode == ArpMode::Down)
    {
        std::reverse(pattern, pattern + patternSize);
        std::reverse(patternVel, patternVel + patternSize);
    }
    if (patternSize > 0)
        patternPos %= patternSize;
    else
        patternPos = 0;
}

double Arpeggiator::stepLengthSamples(long long stepIndex) const
{
    const double base = divisionBeats(settings.division) * 60.0 / std::max(1.0, settings.bpm) * sampleRate;
    const double s = settings.swing * 0.5;
    return (stepIndex % 2 == 0) ? base * (1.0 + s) : base * (1.0 - s);
}

void Arpeggiator::syncToHost(double ppq)
{
    if (! running)
        return;
    const double stepBeats = divisionBeats(settings.division);
    const double pairBeats = 2.0 * stepBeats;
    const double samplesPerBeat = 60.0 / std::max(1.0, settings.bpm) * sampleRate;
    const double pairPos = ppq / pairBeats;
    const long long pairIndex = (long long) std::floor(pairPos + 1.0e-9);
    const double frac = pairPos - (double) pairIndex;
    const double secondStart = 0.5 * (1.0 + settings.swing * 0.5);

    double beatsToNext;
    if (frac < 1.0e-6)
    {
        beatsToNext = 0.0;
        stepCounter = pairIndex * 2;
    }
    else if (frac < secondStart)
    {
        beatsToNext = (secondStart - frac) * pairBeats;
        stepCounter = pairIndex * 2 + 1;
    }
    else
    {
        beatsToNext = (1.0 - frac) * pairBeats;
        stepCounter = pairIndex * 2 + 2;
    }
    toNextStep = beatsToNext * samplesPerBeat;
}

void Arpeggiator::releaseSounding(GeneratedNoteList& out)
{
    for (int i = 0; i < soundingCount; ++i)
        out.add(sounding[i], 0);
    soundingCount = 0;
}

void Arpeggiator::fireDueEvents(GeneratedNoteList& out)
{
    if (toNoteOff >= 0.0 && toNoteOff <= 0.5)
    {
        releaseSounding(out);
        toNoteOff = -1.0;
    }

    if (! running || toNextStep > 0.5)
        return;

    if (patternSize == 0)
    {
        releaseSounding(out);
        running = false;
        return;
    }

    releaseSounding(out);

    if (settings.mode == ArpMode::Chord)
    {
        const int shift = 12 * (int) (stepCounter % settings.octaves);
        for (int i = 0; i < poolSize && soundingCount < 128; ++i)
        {
            const int note = pool[i] + shift;
            if (note <= 127)
            {
                out.add(note, poolVel[i]);
                sounding[soundingCount++] = note;
            }
        }
    }
    else
    {
        int idx = 0;
        switch (settings.mode)
        {
            case ArpMode::Random:
                idx = (int) (rng.next() % (uint32_t) patternSize);
                break;
            case ArpMode::UpDown:
                if (patternSize == 1)
                {
                    idx = 0;
                }
                else
                {
                    idx = patternPos;
                    if (patternPos + upDownDir < 0 || patternPos + upDownDir >= patternSize)
                        upDownDir = -upDownDir;
                    patternPos += upDownDir;
                }
                break;
            default:
                idx = patternPos;
                patternPos = (patternPos + 1) % patternSize;
                break;
        }
        idx = std::clamp(idx, 0, patternSize - 1);
        out.add(pattern[idx], patternVel[idx]);
        sounding[soundingCount++] = pattern[idx];
    }

    const double len = stepLengthSamples(stepCounter);
    toNoteOff = settings.gate >= 0.999f ? -1.0 : std::max(1.0, len * settings.gate);
    toNextStep += len;
    ++stepCounter;
}

int Arpeggiator::samplesToNextEvent() const
{
    double next = 1.0e9;
    if (running)
        next = std::min(next, toNextStep);
    if (toNoteOff >= 0.0)
        next = std::min(next, toNoteOff);
    return std::max(1, (int) std::ceil(next - 1.0e-9));
}

void Arpeggiator::advance(int samples)
{
    if (running)
        toNextStep -= samples;
    if (toNoteOff >= 0.0)
        toNoteOff = std::max(0.0, toNoteOff - samples);
}

void Arpeggiator::stop(GeneratedNoteList& out)
{
    releaseSounding(out);
    reset();
}

} // namespace zsynth::dsp
