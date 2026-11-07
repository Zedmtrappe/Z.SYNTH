#pragma once

// Intelligent harmoniser: turns every played note (or only the top note) into
// up to three extra notes that stay in the chosen key and scale.
//
// INTERVALS. In a seven-note scale an interval is counted in scale steps
// (a "3rd up" from E in C major is G, from F it is A): diatonic harmony, the
// way a singer or a guitarist harmonises. In the pentatonic and blues scales
// the interval's usual size in semitones is snapped to the nearest scale note.
// FIXED ignores the key and plays exact intervals (3rd = major 3rd, 7th =
// minor 7th, ...).
//
// OUT OF KEY. A played note outside the scale can be snapped to the nearest
// scale note, harmonised anyway (the harmony moves by the same amount), or
// played alone.
//
// Every note the harmoniser plays is reference-counted per channel and key,
// so a harmony note that coincides with a played note (or with another
// voice) is never duplicated and always released. Strummed (delayed) notes
// that are released before they start are simply dropped.
//
// Real-time safe: fixed-size state, no allocation.

#include "../Engine/SynthEngine.h"
#include "../../Parameters/ParamChoices.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace zsynth::dsp
{

struct HarmonySettings
{
    int key = 0;                          // 0 = C .. 11 = B
    HarmScale scale = HarmScale::Major;
    int voices = 2;                       // 1..3 harmony notes
    HarmInterval interval[3] { HarmInterval::Third, HarmInterval::Fifth, HarmInterval::Octave };
    bool down[3] { false, false, true };
    HarmOutOfKey outOfKey = HarmOutOfKey::Harmonise;
    bool topNoteOnly = true;
    float level = 0.8f;                   // harmony velocity relative to the played note
    float strumMs = 0.0f;                 // delay between successive harmony notes
    int lowest = 36, highest = 96;        // harmony notes fold back by octaves into this range
};

class Harmonizer
{
public:
    static constexpr int kMaxVoices = 3;

    void prepare(double sampleRate) { fs = sampleRate; reset(); }

    void reset()
    {
        for (auto& ch : count) ch.fill(0);
        for (auto& ch : held) ch.fill(0);
        for (auto& ch : sounding) ch.fill(-1);
        for (auto& ch : harmony)
            for (auto& h : ch)
                h = {};
        topNote.fill(-1);
        pendingCount = 0;
        now = 0;
    }

    /** True while any note it played is still sounding (or waiting to be strummed). */
    bool isActive() const
    {
        if (pendingCount > 0)
            return true;
        for (const auto& ch : count)
            for (auto c : ch)
                if (c > 0)
                    return true;
        return false;
    }

    /** Harmonises a block of events (time-ordered); writes the result (time-ordered) to out. */
    int process(const MidiEvent* in, int numIn, int n, const HarmonySettings& s, MidiEvent* out, int maxOut)
    {
        settings = s;
        settings.voices = std::clamp(s.voices, 1, kMaxVoices);
        outBuf = out;
        outCap = maxOut;
        outCount = 0;
        strumSamples = (int64_t) std::max(0.0, (double) s.strumMs * 0.001 * fs);

        for (int i = 0; i < numIn; ++i)
        {
            const auto& e = in[i];
            firePending(now + e.offset);
            handle(e);
        }
        firePending(now + n);
        now += n;
        return outCount;
    }

    /** Releases everything it is playing (switched off, or the part is no longer harmonised). */
    int flush(MidiEvent* out, int maxOut)
    {
        outBuf = out;
        outCap = maxOut;
        outCount = 0;
        pendingCount = 0;
        for (int ch = 0; ch < 16; ++ch)
            for (int k = 0; k < 128; ++k)
                if (count[(size_t) ch][(size_t) k] > 0)
                    emit({ 0, (uint8_t) (0x80 | ch), (uint8_t) k, 0 });
        reset();
        return outCount;
    }

    // ------------------------------------------------------------- theory
    /** Pitch classes of a scale, from its tonic (0 = the key note). */
    static int scaleNotes(HarmScale sc, int* pcs)
    {
        static const int maj[] = { 0, 2, 4, 5, 7, 9, 11 }, nat[] = { 0, 2, 3, 5, 7, 8, 10 }, harm[] = { 0, 2, 3, 5, 7, 8, 11 },
                         mel[] = { 0, 2, 3, 5, 7, 9, 11 }, dor[] = { 0, 2, 3, 5, 7, 9, 10 }, phr[] = { 0, 1, 3, 5, 7, 8, 10 },
                         lyd[] = { 0, 2, 4, 6, 7, 9, 11 }, mix[] = { 0, 2, 4, 5, 7, 9, 10 }, pmaj[] = { 0, 2, 4, 7, 9 },
                         pmin[] = { 0, 3, 5, 7, 10 }, blues[] = { 0, 3, 5, 6, 7, 10 };
        const int* src = maj;
        int len = 7;
        switch (sc)
        {
            case HarmScale::NaturalMinor:    src = nat; break;
            case HarmScale::HarmonicMinor:   src = harm; break;
            case HarmScale::MelodicMinor:    src = mel; break;
            case HarmScale::Dorian:          src = dor; break;
            case HarmScale::Phrygian:        src = phr; break;
            case HarmScale::Lydian:          src = lyd; break;
            case HarmScale::Mixolydian:      src = mix; break;
            case HarmScale::MajorPentatonic: src = pmaj; len = 5; break;
            case HarmScale::MinorPentatonic: src = pmin; len = 5; break;
            case HarmScale::Blues:           src = blues; len = 6; break;
            default: break;
        }
        std::copy(src, src + len, pcs);
        return len;
    }

    static int intervalSemitones(HarmInterval iv)
    {
        static const int s[] = { 2, 4, 5, 7, 9, 10, 12, 14, 16 };
        return s[std::clamp((int) iv, 0, (int) HarmInterval::Count - 1)];
    }

    static bool inScale(int note, int key, HarmScale sc)
    {
        if (sc == HarmScale::Fixed)
            return true;
        int pcs[7];
        const int len = scaleNotes(sc, pcs);
        const int pc = ((note - key) % 12 + 12) % 12;
        return std::find(pcs, pcs + len, pc) != pcs + len;
    }

    /** The nearest scale note (ties go down). */
    static int snap(int note, int key, HarmScale sc)
    {
        for (int d = 0; d < 7; ++d)
        {
            if (inScale(note - d, key, sc))
                return note - d;
            if (inScale(note + d, key, sc))
                return note + d;
        }
        return note;
    }

    /** The harmony note for `note` (already in the scale, or handled as out of key). */
    static int harmonyFor(int note, HarmInterval iv, bool down, int key, HarmScale sc)
    {
        const int sign = down ? -1 : 1;
        if (sc == HarmScale::Fixed)
            return note + sign * intervalSemitones(iv);
        int pcs[7];
        const int len = scaleNotes(sc, pcs);
        if (len == 7)
        {
            // Diatonic: count scale steps. Out-of-key notes keep their offset from
            // the scale note below them.
            int base = note;
            while (! inScale(base, key, sc))
                --base;
            const int chromatic = note - base;
            const int pc = ((base - key) % 12 + 12) % 12;
            const int degree = (int) (std::find(pcs, pcs + 7, pc) - pcs);
            const int octave = (base - key - pc) / 12;
            const int steps = sign * ((int) iv + 1); // 2nd = 1 step ... 10th = 9 steps
            const int target = degree + steps;
            const int oct = target >= 0 ? target / 7 : -((6 - target) / 7);
            const int deg = target - oct * 7;
            return key + 12 * (octave + oct) + pcs[deg] + chromatic;
        }
        // Pentatonic / blues: the usual size of the interval, snapped into the scale
        // (never onto the played note itself).
        int t = snap(note + sign * intervalSemitones(iv), key, sc);
        if (t == note)
            t = snap(note + sign * (intervalSemitones(iv) + 1), key, sc);
        return t;
    }

private:
    struct Owned
    {
        std::array<int8_t, kMaxVoices> notes { { -1, -1, -1 } };
        std::array<bool, kMaxVoices> started { { false, false, false } };
    };
    struct Pending { int64_t at; uint8_t ch, note, vel, owner; int voice; };

    void emit(const MidiEvent& e)
    {
        if (outCount < outCap)
            outBuf[outCount++] = e;
    }

    int offsetOf(int64_t t) const { return (int) std::max<int64_t>(0, t - now); }

    void noteOn(int ch, int k, int vel, int64_t t)
    {
        if (count[(size_t) ch][(size_t) k]++ == 0)
            emit({ offsetOf(t), (uint8_t) (0x90 | ch), (uint8_t) k, (uint8_t) std::clamp(vel, 1, 127) });
    }

    void noteOff(int ch, int k, int64_t t)
    {
        auto& c = count[(size_t) ch][(size_t) k];
        if (c > 0 && --c == 0)
            emit({ offsetOf(t), (uint8_t) (0x80 | ch), (uint8_t) k, 0 });
    }

    void firePending(int64_t until)
    {
        // Strummed harmony notes, in time order.
        for (;;)
        {
            int best = -1;
            for (int i = 0; i < pendingCount; ++i)
                if (pending[(size_t) i].at < until && (best < 0 || pending[(size_t) i].at < pending[(size_t) best].at))
                    best = i;
            if (best < 0)
                return;
            const auto p = pending[(size_t) best];
            pending[(size_t) best] = pending[(size_t) --pendingCount];
            noteOn(p.ch, p.note, p.vel, p.at);
            harmony[p.ch][p.owner].started[(size_t) p.voice] = true;
        }
    }

    void harmonise(int ch, int owner, int base, int vel, int64_t t)
    {
        auto& own = harmony[(size_t) ch][(size_t) owner];
        own = {};
        const int velocity = std::max(1, (int) std::lround((float) vel * std::clamp(settings.level, 0.0f, 1.0f)));
        int made = 0;
        for (int v = 0; v < settings.voices; ++v)
        {
            int h = harmonyFor(base, settings.interval[v], settings.down[v], settings.key, settings.scale);
            const int lo = std::min(settings.lowest, settings.highest), hi = std::max(settings.lowest, settings.highest);
            while (h < lo && h + 12 <= 127)
                h += 12;
            while (h > hi && h - 12 >= 0)
                h -= 12;
            if (h < 0 || h > 127 || h < lo || h > hi || h == base)
                continue;
            bool dup = false;
            for (int u = 0; u < v; ++u)
                dup = dup || own.notes[(size_t) u] == h;
            if (dup)
                continue;
            own.notes[(size_t) v] = (int8_t) h;
            ++made;
            const int64_t at = t + strumSamples * made;
            if (strumSamples <= 0)
            {
                noteOn(ch, h, velocity, t);
                own.started[(size_t) v] = true;
            }
            else if (pendingCount < (int) pending.size())
            {
                pending[(size_t) pendingCount++] = { at, (uint8_t) ch, (uint8_t) h, (uint8_t) velocity, (uint8_t) owner, v };
            }
        }
    }

    void release(int ch, int owner, int64_t t)
    {
        auto& own = harmony[(size_t) ch][(size_t) owner];
        // Not yet strummed: just forget them.
        for (int i = pendingCount; --i >= 0;)
            if (pending[(size_t) i].ch == ch && pending[(size_t) i].owner == owner)
                pending[(size_t) i] = pending[(size_t) --pendingCount];
        for (int v = 0; v < kMaxVoices; ++v)
            if (own.notes[(size_t) v] >= 0 && own.started[(size_t) v])
                noteOff(ch, own.notes[(size_t) v], t);
        own = {};
    }

    int highestHeld(int ch) const
    {
        for (int k = 127; k >= 0; --k)
            if (held[(size_t) ch][(size_t) k])
                return k;
        return -1;
    }

    void handle(const MidiEvent& e)
    {
        const int kind = e.status & 0xF0, ch = e.status & 0x0F, k = e.data1 & 0x7F;
        const int64_t t = now + e.offset;
        const bool on = kind == 0x90 && e.data2 > 0;
        const bool off = kind == 0x80 || (kind == 0x90 && e.data2 == 0);
        if (on)
        {
            if (held[(size_t) ch][(size_t) k])
                return; // already held (a repeated note-on): ignore
            const bool inKey = inScale(k, settings.key, settings.scale);
            const int played = (! inKey && settings.outOfKey == HarmOutOfKey::Snap) ? snap(k, settings.key, settings.scale) : k;
            held[(size_t) ch][(size_t) k] = (uint8_t) std::max<int>(1, e.data2);
            sounding[(size_t) ch][(size_t) k] = (int8_t) played;
            noteOn(ch, played, e.data2, t);
            if (! settings.topNoteOnly)
            {
                if (inKey || settings.outOfKey != HarmOutOfKey::PlayAlone)
                    harmonise(ch, k, played, e.data2, t);
                return;
            }
            const int top = highestHeld(ch);
            if (top != topNote[(size_t) ch])
            {
                if (topNote[(size_t) ch] >= 0)
                    release(ch, topNote[(size_t) ch], t);
                topNote[(size_t) ch] = top;
                rehamoniseTop(ch, t);
            }
            return;
        }
        if (off)
        {
            if (! held[(size_t) ch][(size_t) k])
            {
                emit(e); // not ours (e.g. a note held from before): pass it on
                return;
            }
            held[(size_t) ch][(size_t) k] = 0;
            noteOff(ch, sounding[(size_t) ch][(size_t) k], t);
            sounding[(size_t) ch][(size_t) k] = -1;
            if (! settings.topNoteOnly)
            {
                release(ch, k, t);
                return;
            }
            if (k == topNote[(size_t) ch])
            {
                release(ch, k, t);
                topNote[(size_t) ch] = highestHeld(ch);
                if (topNote[(size_t) ch] >= 0)
                    rehamoniseTop(ch, t);
            }
            return;
        }
        if (kind == 0xB0 && (e.data1 == 120 || e.data1 == 123))
        {
            // All notes off / all sound off: the engine releases everything.
            for (auto& c : count[(size_t) ch]) c = 0;
            for (auto& h : held[(size_t) ch]) h = 0;
            for (auto& s : sounding[(size_t) ch]) s = -1;
            for (auto& h : harmony[(size_t) ch]) h = {};
            topNote[(size_t) ch] = -1;
            for (int i = pendingCount; --i >= 0;)
                if (pending[(size_t) i].ch == ch)
                    pending[(size_t) i] = pending[(size_t) --pendingCount];
        }
        emit(e); // controllers, bend, pressure ...
    }

    void rehamoniseTop(int ch, int64_t t)
    {
        const int top = topNote[(size_t) ch];
        if (top < 0)
            return;
        const bool inKey = inScale(top, settings.key, settings.scale);
        if (! inKey && settings.outOfKey == HarmOutOfKey::PlayAlone)
            return;
        harmonise(ch, top, sounding[(size_t) ch][(size_t) top], held[(size_t) ch][(size_t) top], t);
    }

    double fs = 48000.0;
    HarmonySettings settings;
    std::array<std::array<uint16_t, 128>, 16> count {};
    std::array<std::array<uint8_t, 128>, 16> held {};     // played velocity, 0 = not held
    std::array<std::array<int8_t, 128>, 16> sounding {};  // what a held key plays (after snapping)
    std::array<std::array<Owned, 128>, 16> harmony {};
    std::array<int, 16> topNote {};
    std::array<Pending, 96> pending {};
    int pendingCount = 0;
    int64_t now = 0, strumSamples = 0;
    MidiEvent* outBuf = nullptr;
    int outCap = 0, outCount = 0;
};

// -----------------------------------------------------------------------------
/** DETECT: finds the key from what is played (Krumhansl-Schmuckler key profiles,
    notes weighted by how long they are held). Major and natural minor. */
class KeyDetector
{
public:
    void prepare(double sampleRate) { fs = sampleRate; start(); listening.store(false); }

    void start()
    {
        weights.fill(0.0);
        heldSince.fill(-1);
        notes = 0;
        firstAt = -1;
        now = 0;
        result.store(-1);
        listening.store(true);
    }
    void stop() { listening.store(false); }
    bool isListening() const { return listening.load(); }
    int notesHeard() const { return notesSeen.load(); }
    /** key + 12 * (0 = major, 1 = minor) once found, else -1. Clears it. */
    int takeResult() { return result.exchange(-1); }

    /** Watches a block of (input) events; n = block length. */
    void observe(const MidiEvent* e, int numEvents, int n)
    {
        if (! listening.load(std::memory_order_relaxed))
        {
            now += n;
            return;
        }
        for (int i = 0; i < numEvents; ++i)
        {
            const int kind = e[i].status & 0xF0, k = e[i].data1 & 0x7F;
            const int64_t t = now + e[i].offset;
            if (kind == 0x90 && e[i].data2 > 0)
            {
                heldSince[(size_t) k] = t;
                ++notes;
                if (firstAt < 0)
                    firstAt = t;
                weights[(size_t) (k % 12)] += 0.25; // every attack counts a little
            }
            else if ((kind == 0x80 || kind == 0x90) && heldSince[(size_t) k] >= 0)
            {
                weights[(size_t) (k % 12)] += std::min(2.0, (double) (t - heldSince[(size_t) k]) / fs);
                heldSince[(size_t) k] = -1;
            }
        }
        now += n;
        notesSeen.store(notes, std::memory_order_relaxed);
        // Enough heard: 16 notes, or 12 s after the first one (with at least 6 notes).
        if (notes >= 16 || (firstAt >= 0 && now - firstAt > (int64_t) (12.0 * fs) && notes >= 6))
        {
            for (int k = 0; k < 128; ++k)
                if (heldSince[(size_t) k] >= 0)
                    weights[(size_t) (k % 12)] += std::min(2.0, (double) (now - heldSince[(size_t) k]) / fs);
            result.store(bestKey(weights));
            listening.store(false);
        }
    }

    static int bestKey(const std::array<double, 12>& w)
    {
        static const double major[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double minor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto corr = [&](const double* prof, int key) {
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += w[(size_t) ((i + key) % 12)]; my += prof[i]; }
            mx /= 12.0; my /= 12.0;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double x = w[(size_t) ((i + key) % 12)] - mx, y = prof[i] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return sxx > 0 && syy > 0 ? sxy / std::sqrt(sxx * syy) : -1.0;
        };
        int best = 0;
        double bestR = -2.0;
        for (int key = 0; key < 12; ++key)
            for (int m = 0; m < 2; ++m)
            {
                const double r = corr(m == 0 ? major : minor, key);
                if (r > bestR)
                {
                    bestR = r;
                    best = key + 12 * m;
                }
            }
        return best;
    }

private:
    double fs = 48000.0;
    std::array<double, 12> weights {};
    std::array<int64_t, 128> heldSince {};
    int notes = 0;
    int64_t firstAt = -1, now = 0;
    std::atomic<int> result { -1 }, notesSeen { 0 };
    std::atomic<bool> listening { false };
};

} // namespace zsynth::dsp
