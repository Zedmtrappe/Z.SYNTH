#include "StepSequencer.h"

#include <algorithm>

namespace zsynth::dsp
{

void StepSequencer::prepare(double sr)
{
    sampleRate = sr;
    reset();
}

void StepSequencer::reset()
{
    heldCount = 0;
    step = 0;
    nextStep = 0;
    prevStep = 0;
    stepCounter = 0;
    toNextStep = 0.0;
    currentStepLength = 1.0;
    toNoteOff = -1.0;
    soundingNote = -1;
}

void StepSequencer::noteOn(int note, int velocity)
{
    const bool wasEmpty = heldCount == 0;
    int w = 0;
    for (int i = 0; i < heldCount; ++i)
        if (held[i] != note)
        {
            held[w] = held[i];
            heldVel[w] = heldVel[i];
            ++w;
        }
    heldCount = w;
    if (heldCount < 128)
    {
        held[heldCount] = note;
        heldVel[heldCount] = velocity;
        ++heldCount;
    }

    if (wasEmpty && ! hostSynced)
    {
        nextStep = 0;
        stepCounter = 0;
        toNextStep = 0.0;
    }
}

void StepSequencer::noteOff(int note)
{
    int w = 0;
    for (int i = 0; i < heldCount; ++i)
        if (held[i] != note)
        {
            held[w] = held[i];
            heldVel[w] = heldVel[i];
            ++w;
        }
    heldCount = w;
    if (heldCount == 0 && soundingNote >= 0)
        toNoteOff = 0.0;
}

double StepSequencer::stepLengthSamples(long long index) const
{
    const double base = divisionBeats(settings.division) * 60.0 / std::max(1.0, settings.bpm) * sampleRate;
    const double s = settings.swing * 0.5;
    return (index % 2 == 0) ? base * (1.0 + s) : base * (1.0 - s);
}

void StepSequencer::syncToHost(double ppq)
{
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
    nextStep = (int) (stepCounter % settings.length);
}

void StepSequencer::tick(GeneratedNoteList& out)
{
    const int length = settings.length;
    const int cur = nextStep % length;
    step = cur;
    ++ticks;
    const double len = stepLengthSamples(stepCounter);
    currentStepLength = len;

    if (settings.mode == SeqMode::Note)
    {
        const int base = baseNote();
        const bool slideIn = soundingNote >= 0 && settings.slides[prevStep];
        if (base >= 0 && settings.gates[cur])
        {
            const int note = std::clamp(base + (int) std::lround(settings.values[cur] * 12.0f), 0, 127);
            const int vel = settings.accents[cur] ? 127 : 92;
            if (slideIn)
            {
                if (note != soundingNote)
                {
                    out.add(note, vel, kSlideTime); // overlap first: the voice glides legato
                    out.add(soundingNote, 0);
                }
            }
            else
            {
                if (soundingNote >= 0)
                    out.add(soundingNote, 0);
                out.add(note, vel);
            }
            soundingNote = note;
            toNoteOff = settings.slides[cur] ? -1.0 : std::max(1.0, len * settings.gate);
        }
        else
        {
            if (soundingNote >= 0)
                out.add(soundingNote, 0);
            soundingNote = -1;
            toNoteOff = -1.0;
        }
    }

    prevStep = cur;
    nextStep = (cur + 1) % length;
    toNextStep += len;
    ++stepCounter;
}

void StepSequencer::fireDueEvents(GeneratedNoteList& out)
{
    if (toNoteOff >= 0.0 && toNoteOff <= 0.5)
    {
        if (soundingNote >= 0)
            out.add(soundingNote, 0);
        soundingNote = -1;
        toNoteOff = -1.0;
    }
    if (toNextStep <= 0.5)
        tick(out);
}

int StepSequencer::samplesToNextEvent() const
{
    double next = toNextStep;
    if (toNoteOff >= 0.0)
        next = std::min(next, toNoteOff);
    return std::max(1, (int) std::ceil(next - 1.0e-9));
}

void StepSequencer::advance(int samples)
{
    toNextStep -= samples;
    if (toNoteOff >= 0.0)
        toNoteOff = std::max(0.0, toNoteOff - samples);
}

void StepSequencer::stop(GeneratedNoteList& out)
{
    if (soundingNote >= 0)
        out.add(soundingNote, 0);
    soundingNote = -1;
    toNoteOff = -1.0;
    heldCount = 0;
}

float StepSequencer::currentValue() const
{
    if (settings.mode == SeqMode::Off)
        return 0.0f;
    float v = settings.values[step];
    if (settings.slides[step])
    {
        const float frac = clampf(1.0f - (float) (toNextStep / std::max(1.0, currentStepLength)), 0.0f, 1.0f);
        v += (settings.values[(step + 1) % settings.length] - v) * frac;
    }
    return v;
}

} // namespace zsynth::dsp
