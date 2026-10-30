#include "AuditionPlayer.h"

namespace zsynth
{

std::unique_ptr<AuditionPlayer::Clip> AuditionPlayer::parse(const void* data, size_t size, const juce::String& name, juce::String& error)
{
    juce::MemoryInputStream in(data, size, false);
    juce::MidiFile file;
    if (! file.readFrom(in, true))
    {
        error = "Not a standard MIDI file";
        return nullptr;
    }
    file.convertTimestampTicksToSeconds();

    auto clip = std::make_unique<Clip>();
    clip->name = name;
    double end = 0.0;
    for (int t = 0; t < file.getNumTracks(); ++t)
    {
        const auto* track = file.getTrack(t);
        for (const auto* holder : *track)
        {
            const auto& m = holder->message;
            end = std::max(end, m.getTimeStamp());
            if (m.isTempoMetaEvent() && clip->bpm == 120.0 && m.getTimeStamp() <= 0.0)
                clip->bpm = 60.0 / std::max(1.0e-6, m.getTempoSecondsPerQuarterNote());
            const bool keep = m.isNoteOnOrOff() || m.isPitchWheel() || m.isSustainPedalOn() || m.isSustainPedalOff()
                           || (m.isController() && (m.getControllerNumber() == 1 || m.getControllerNumber() == 11));
            if (! keep || m.getRawDataSize() > 3)
                continue;
            const auto* raw = m.getRawData();
            Event e { m.getTimeStamp(), (uint8_t) (raw[0] & 0xF0), m.getRawDataSize() > 1 ? raw[1] : (uint8_t) 0,
                      m.getRawDataSize() > 2 ? raw[2] : (uint8_t) 0 };
            if (e.status == 0x90 && e.data2 == 0)
                e.status = 0x80;
            clip->notes += e.status == 0x90 ? 1 : 0;
            clip->events.push_back(e);
        }
    }
    if (clip->notes == 0)
    {
        error = "The file has no notes";
        return nullptr;
    }
    // Note-offs before note-ons at the same time, so repeated notes retrigger.
    std::stable_sort(clip->events.begin(), clip->events.end(), [](const Event& a, const Event& b) {
        if (a.seconds != b.seconds)
            return a.seconds < b.seconds;
        return (a.status == 0x80) && (b.status != 0x80);
    });
    clip->lengthSeconds = std::max(end, clip->events.back().seconds + 0.05);
    return clip;
}

void AuditionPlayer::setClip(std::unique_ptr<Clip> clip)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    clipName = clip != nullptr ? clip->name : juce::String();
    clipBpm = clip != nullptr ? clip->bpm : 0.0;
    clipLength = clip != nullptr ? clip->lengthSeconds : 0.0;
    incoming = std::move(clip);
}

void AuditionPlayer::collectGarbage()
{
    std::unique_ptr<Clip> dead;
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        dead = std::move(retired);
    }
}

juce::String AuditionPlayer::getClipName() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return clipName;
}

double AuditionPlayer::getClipBpm() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return clipBpm;
}

double AuditionPlayer::getClipLength() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return clipLength;
}

void AuditionPlayer::releaseHeldNotes(juce::MidiBuffer& midi, int sampleOffset, int channel)
{
    for (int note = 0; note < 128; ++note)
        if (held[(size_t) note])
            midi.addEvent(juce::MidiMessage::noteOff(channel, note), sampleOffset);
    held.reset();
    if (sustainDown)
        midi.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
    sustainDown = false;
}

void AuditionPlayer::render(juce::MidiBuffer& midi, int numSamples, double sampleRate, int channel)
{
    channel = juce::jlimit(1, 16, channel);
    // Pick up a new clip if the message thread is not touching the handoff.
    {
        const juce::SpinLock::ScopedTryLockType tl(lock);
        if (tl.isLocked() && incoming != nullptr && retired == nullptr)
        {
            releaseHeldNotes(midi, 0, lastChannel);
            retired = std::move(active);
            active = std::move(incoming);
            playhead = 0.0;
            nextEvent = 0;
        }
    }

    const bool play = playing.load() && active != nullptr && sampleRate > 0.0;
    if (! play)
    {
        if (wasPlaying)
            releaseHeldNotes(midi, 0, lastChannel);
        wasPlaying = false;
        return;
    }
    if (restartRequested.exchange(false) || ! wasPlaying || channel != lastChannel)
    {
        releaseHeldNotes(midi, 0, lastChannel);
        playhead = 0.0;
        nextEvent = 0;
    }
    wasPlaying = true;
    lastChannel = channel;

    const auto& events = active->events;
    const double blockSeconds = numSamples / sampleRate;
    double blockStart = 0.0; // seconds into this block already rendered
    while (blockStart < blockSeconds)
    {
        const double end = playhead + (blockSeconds - blockStart);
        while (nextEvent < events.size() && events[nextEvent].seconds < std::min(end, active->lengthSeconds))
        {
            const auto& e = events[nextEvent++];
            const int offset = juce::jlimit(0, numSamples - 1, (int) ((blockStart + e.seconds - playhead) * sampleRate));
            const uint8_t bytes[3] = { (uint8_t) (e.status | (channel - 1)), e.data1, e.data2 };
            midi.addEvent(bytes, e.status == 0xC0 || e.status == 0xD0 ? 2 : 3, offset);
            if (e.status == 0x90)
                held[e.data1 & 127] = true;
            else if (e.status == 0x80)
                held[e.data1 & 127] = false;
            else if (e.status == 0xB0 && e.data1 == 64)
                sustainDown = e.data2 >= 64;
        }

        if (end < active->lengthSeconds)
        {
            playhead = end;
            break;
        }

        // Reached the end of the clip inside this block.
        const double consumed = active->lengthSeconds - playhead;
        blockStart += consumed;
        const int offset = juce::jlimit(0, numSamples - 1, (int) (blockStart * sampleRate));
        releaseHeldNotes(midi, offset, channel);
        playhead = 0.0;
        nextEvent = 0;
        if (! looping.load())
        {
            playing = false;
            wasPlaying = false;
            break;
        }
    }
    position.store((float) (playhead / std::max(1.0e-6, active->lengthSeconds)), std::memory_order_relaxed);
}

} // namespace zsynth
