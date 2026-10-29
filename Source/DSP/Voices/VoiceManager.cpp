#include "VoiceManager.h"

namespace zsynth::dsp
{

void VoiceManager::prepare(float internalSampleRate)
{
    for (int i = 0; i < kMaxVoices; ++i)
        voices[i].prepare(internalSampleRate, i);
    for (auto& v : lastVoiceForNote)
        v = -1;
    for (auto& k : keyDown)
        k = false;
    heldCount = 0;
    monoStackSize = 0;
    monoCurrent = -1;
    monoGate = false;
    monoSustained = false;
    sustainDown = false;
}

void VoiceManager::setSettings(const VoiceSettings& s)
{
    const bool wasMono = settings.mode != VoiceMode::Poly;
    const bool isMono = s.mode != VoiceMode::Poly;
    const bool modeFamilyChanged = settingsValid && wasMono != isMono;
    const bool limitShrunk = settingsValid && s.polyLimit < settings.polyLimit;

    settings = s;
    settings.polyLimit = std::clamp(settings.polyLimit, 1, kMaxVoices);
    settings.unison = std::clamp(settings.unison, 1, 8);
    settingsValid = true;

    if (modeFamilyChanged)
    {
        // Switching between poly and mono families: release cleanly rather than
        // leave voices in states the new mode does not track.
        for (auto& v : voices)
            if (v.isActive())
                v.release();
        monoStackSize = 0;
        monoGate = false;
        monoCurrent = -1;
        for (auto& k : keyDown)
            k = false;
        heldCount = 0;
    }

    if (limitShrunk)
        for (int i = settings.polyLimit; i < kMaxVoices; ++i)
            if (voices[i].isActive())
                voices[i].release();
}

float VoiceManager::curveVelocity(int velocity) const
{
    const float x = std::clamp((float) velocity / 127.0f, 0.0f, 1.0f);
    switch (settings.velocityCurve)
    {
        case VelocityCurve::Soft:  return std::pow(x, 0.55f);
        case VelocityCurve::Hard:  return std::pow(x, 1.8f);
        case VelocityCurve::Fixed: return 1.0f;
        default:                   return x;
    }
}

int VoiceManager::unisonCount() const
{
    return std::min(settings.unison, settings.polyLimit);
}

NoteStart VoiceManager::makeStart(int note, float vel, int stackIndex, int stackSize, int voiceIndex, float glideFrom, float glideTime) const
{
    NoteStart ns;
    ns.note = note;
    ns.velocity = vel;
    ns.glideFrom = glideFrom;
    ns.glideTime = glideTime;
    ns.age = ageCounter;
    if (stackSize > 1)
    {
        const float x = (float) stackIndex / (float) (stackSize - 1) * 2.0f - 1.0f;
        ns.unisonDetune = x * settings.detune * 0.5f;
        ns.unisonPan = x;
        ns.randomPhase = true;
    }
    else
    {
        // Poly voices get a fixed stereo position per slot (golden-ratio spread),
        // scaled by the stereo spread control.
        const float g = std::fmod((float) voiceIndex * 0.618034f, 1.0f);
        ns.unisonPan = (g * 2.0f - 1.0f) * 0.6f;
        ns.randomPhase = settings.vintage > 0.05f;
    }
    return ns;
}

int VoiceManager::pickVoice(int note, const bool* excluded)
{
    const int limit = settings.polyLimit;

    if (settings.alloc == VoiceAlloc::Reuse)
    {
        const int prev = lastVoiceForNote[note];
        if (prev >= 0 && prev < limit && ! excluded[prev] && ! voices[prev].isActive())
            return prev;
    }

    if (settings.alloc == VoiceAlloc::Lowest)
    {
        for (int i = 0; i < limit; ++i)
            if (! excluded[i] && ! voices[i].isActive())
                return i;
    }
    else
    {
        for (int k = 0; k < limit; ++k)
        {
            const int i = (rotateIndex + k) % limit;
            if (! excluded[i] && ! voices[i].isActive())
            {
                rotateIndex = (i + 1) % limit;
                return i;
            }
        }
    }

    // Steal: released first, then sustained, then held; oldest first.
    int best = -1;
    int bestClass = 3;
    uint32_t bestAge = 0;
    for (int i = 0; i < limit; ++i)
    {
        if (excluded[i])
            continue;
        const Voice& v = voices[i];
        const int cls = v.keyHeld ? 2 : (v.sustained ? 1 : 0);
        if (best < 0 || cls < bestClass || (cls == bestClass && v.getAge() < bestAge))
        {
            best = i;
            bestClass = cls;
            bestAge = v.getAge();
        }
    }
    return best;
}

void VoiceManager::noteOn(int note, int velocity, float glideOverride)
{
    if (note < 0 || note > 127)
        return;
    if (velocity <= 0)
    {
        noteOff(note);
        return;
    }

    const float vel = curveVelocity(velocity);
    float glide = glideOverride >= 0.0f ? glideOverride : settings.glideTime;

    if (! keyDown[note])
    {
        keyDown[note] = true;
        ++heldCount;
    }
    ++ageCounter;

    if (settings.mode == VoiceMode::Poly)
        polyNoteOn(note, vel, glide);
    else
        monoNoteOn(note, vel, glide);

    lastPlayedNote = note;
    lastPlayedVelocity = vel;
}

void VoiceManager::polyNoteOn(int note, float vel, float glideTime)
{
    const bool anyOtherHeld = heldCount > 1;
    float glideFrom = -1.0f;
    if (glideTime > 0.0f && lastPlayedNote >= 0
        && (settings.glideMode == GlideMode::Always || anyOtherHeld))
        glideFrom = (float) lastPlayedNote;

    // A re-struck note replaces any voice still holding it (e.g. under sustain).
    const int count = unisonCount();
    bool excluded[kMaxVoices] {};

    if (settings.alloc == VoiceAlloc::Reuse)
    {
        int reused = 0;
        for (int i = 0; i < settings.polyLimit && reused < count; ++i)
        {
            Voice& v = voices[i];
            if (v.isActive() && v.getNote() == note && ! v.isReleasing())
            {
                v.retrigger(note, vel, glideFrom >= 0.0f ? glideTime : 0.0f, ageCounter);
                v.keyHeld = true;
                v.sustained = false;
                excluded[i] = true;
                ++reused;
            }
        }
        if (reused == count)
            return;
    }
    else
    {
        for (int i = 0; i < kMaxVoices; ++i)
        {
            Voice& v = voices[i];
            if (v.isActive() && v.getNote() == note && (v.keyHeld || v.sustained))
            {
                v.release();
                v.keyHeld = false;
                v.sustained = false;
            }
        }
    }

    int stackIndex = 0;
    for (int u = 0; u < count; ++u)
    {
        const int idx = pickVoice(note, excluded);
        if (idx < 0)
            break;
        excluded[idx] = true;
        Voice& v = voices[idx];
        const NoteStart ns = makeStart(note, vel, stackIndex++, count, idx, glideFrom, glideTime);
        if (v.isActive())
            v.stealAndStart(ns);
        else
            v.start(ns);
        v.keyHeld = true;
        v.sustained = false;
        lastVoiceForNote[note] = idx;
    }
}

void VoiceManager::polyNoteOff(int note)
{
    for (int i = 0; i < kMaxVoices; ++i)
    {
        Voice& v = voices[i];
        if (v.isActive() && v.keyHeld && v.getNote() == note)
        {
            v.keyHeld = false;
            if (sustainDown)
                v.sustained = true;
            else
                v.release();
        }
    }
}

int VoiceManager::monoSelect() const
{
    if (monoStackSize == 0)
        return -1;
    switch (settings.priority)
    {
        case NotePriority::Low:
        {
            int n = 127;
            for (int i = 0; i < monoStackSize; ++i)
                n = std::min(n, monoStack[i]);
            return n;
        }
        case NotePriority::High:
        {
            int n = 0;
            for (int i = 0; i < monoStackSize; ++i)
                n = std::max(n, monoStack[i]);
            return n;
        }
        default:
            return monoStack[monoStackSize - 1];
    }
}

void VoiceManager::monoNoteOn(int note, float vel, float glideTime)
{
    // Move the note to the top of the stack.
    int w = 0;
    for (int i = 0; i < monoStackSize; ++i)
        if (monoStack[i] != note)
            monoStack[w++] = monoStack[i];
    monoStackSize = w;
    if (monoStackSize < 128)
        monoStack[monoStackSize++] = note;

    const int target = monoSelect();
    const int count = unisonCount();
    const bool wasGated = monoGate;
    monoGate = true;
    monoSustained = false;

    if (wasGated && target == monoCurrent)
        return; // priority keeps the current note

    const bool legatoTransition = wasGated;
    const bool glideAllowed = glideTime > 0.0f && monoCurrent >= 0
                            && (settings.glideMode == GlideMode::Always || legatoTransition);
    const float glide = glideAllowed ? glideTime : 0.0f;

    for (int u = 0; u < count; ++u)
    {
        Voice& v = voices[u];
        if (legatoTransition && settings.mode == VoiceMode::Legato && v.isActive())
        {
            v.legatoTo(target, glide, ageCounter);
        }
        else if (v.isActive())
        {
            v.retrigger(target, vel, glide, ageCounter);
        }
        else
        {
            NoteStart ns = makeStart(target, vel, u, count, u, glideAllowed ? (float) monoCurrent : -1.0f, glide);
            v.start(ns);
        }
        v.keyHeld = true;
        v.sustained = false;
    }
    monoCurrent = target;
}

void VoiceManager::monoNoteOff(int note)
{
    int w = 0;
    for (int i = 0; i < monoStackSize; ++i)
        if (monoStack[i] != note)
            monoStack[w++] = monoStack[i];
    monoStackSize = w;

    const int count = unisonCount();
    if (monoStackSize == 0)
    {
        monoGate = false;
        for (int u = 0; u < count; ++u)
        {
            voices[u].keyHeld = false;
            if (sustainDown)
                voices[u].sustained = true;
        }
        if (sustainDown)
            monoSustained = true;
        else
            for (int u = 0; u < count; ++u)
                voices[u].release();
        return;
    }

    const int target = monoSelect();
    if (target != monoCurrent)
    {
        const float glide = settings.glideTime > 0.0f ? settings.glideTime : 0.0f;
        for (int u = 0; u < count; ++u)
            if (voices[u].isActive())
                voices[u].legatoTo(target, glide, ageCounter);
        monoCurrent = target;
    }
}

void VoiceManager::noteOff(int note)
{
    if (note < 0 || note > 127)
        return;
    if (keyDown[note])
    {
        keyDown[note] = false;
        --heldCount;
    }
    if (settings.mode == VoiceMode::Poly)
        polyNoteOff(note);
    else
        monoNoteOff(note);
}

void VoiceManager::setSustain(bool down)
{
    if (down == sustainDown)
        return;
    sustainDown = down;
    if (down)
        return;

    for (auto& v : voices)
    {
        if (v.isActive() && v.sustained)
        {
            v.sustained = false;
            if (! v.keyHeld)
                v.release();
        }
    }
    if (monoSustained && monoStackSize == 0)
    {
        monoSustained = false;
        for (auto& v : voices)
            if (v.isActive() && ! v.keyHeld)
                v.release();
    }
}

void VoiceManager::polyAftertouch(int note, float value)
{
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note)
            v.setPolyAftertouch(value);
}

void VoiceManager::allNotesOff()
{
    for (auto& v : voices)
    {
        if (v.isActive())
            v.release();
        v.keyHeld = false;
        v.sustained = false;
    }
    for (auto& k : keyDown)
        k = false;
    heldCount = 0;
    monoStackSize = 0;
    monoGate = false;
    monoSustained = false;
    monoCurrent = -1;
}

void VoiceManager::killAll()
{
    allNotesOff();
    for (auto& v : voices)
        v.kill();
    sustainDown = false;
}

void VoiceManager::render(float* left, float* right, int n, const VoiceShared& shared)
{
    for (auto& v : voices)
        if (v.isActive())
            v.render(left, right, n, shared);
}

int VoiceManager::activeVoiceCount() const
{
    int c = 0;
    for (const auto& v : voices)
        c += v.isActive() ? 1 : 0;
    return c;
}

} // namespace zsynth::dsp
