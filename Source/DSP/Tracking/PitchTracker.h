#pragma once

// Monophonic pitch-to-MIDI for a guitar or bass on an audio input.
//
// SIGNAL PATH. Input gain -> DC block -> envelope (gate, picking dynamics,
// re-pick detection) and, in parallel, a 4th-order low-pass and decimation to
// ~12 kHz (guitar) / ~6 kHz (bass) for the pitch detector.
//
// PITCH. YIN (de Cheveigné & Kawahara 2002): the cumulative-mean-normalised
// difference function over a window of one longest period, the first dip
// below the threshold (which avoids octave-down errors), refined by
// parabolic interpolation. Analysed every 2.5 ms (guitar) / 4 ms (bass).
//
// NOTES. A note starts when the gate is open and the pitch has been stable
// for 1-3 analysis frames (TRACKING: loose = fast, strict = steadier); its
// velocity follows the pick attack. It ends when the level falls below the
// gate (with hysteresis and a short hold). A fresh pick on the same string
// retriggers. With BEND FOLLOW the note's deviation is sent as pitch bend
// (scaled to the synth's bend range); past that range, or without bend
// follow, a pitch change plays the new note legato.
//
// CLEAN UP. The typical tracking glitch is a short, low note: the detector
// briefly locks onto a subharmonic (an octave or more down) as a note
// decays, or re-voices a note in the tail with no new pick. A candidate is
// SUSPICIOUS when it arrives without a fresh pick attack and lies well below
// the note being played (or just played), sits below the instrument's range,
// or comes from a weak pitch reading. Suspicious candidates must hold steady
// for longer (up to ~20 ms guitar / ~30 ms bass at full CLEAN UP) before they
// play, and an unpicked jump to an exact subharmonic (an octave, an octave and
// a fifth, two octaves down) is ignored outright. Notes that come with a pick
// attack and a clean reading are never delayed: no added latency.
//
// SUSTAIN. Without it a note ends when the level falls below the gate. With
// it, a note that is still clearly heard rings on below the gate: down to
// 36 dB under it at full, while the detector keeps finding the note's pitch
// (within a semitone, or its bend); it ends ~60 ms after the pitch is lost.
// No new note can start below the gate. HOLD (full): a note holds until the
// next one starts; a pick with no pitch (a muted strum) stops it.
//
// Real-time safe: all memory is allocated in prepare().

#include "../Engine/SynthEngine.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace zsynth::dsp
{

struct TrackerSettings
{
    float inputGainDb = 0.0f;  // input trim
    float gateDb = -50.0f;     // level (dBFS after gain) that opens the gate
    float threshold = 0.5f;    // TRACKING 0 = loose (fast) .. 1 = strict (steady)
    float velocity = 0.7f;     // 0 = fixed velocity 100 .. 1 = follows the picking fully
    bool bass = false;         // range: guitar (70 Hz - 1.4 kHz) or bass (28 - 450 Hz)
    bool bendFollow = true;
    float bendRange = 2.0f;    // the synth's pitch-bend range in semitones
    int channel = 1;           // MIDI channel of the events (1..16)
    float cleanUp = 0.5f;      // CLEAN UP 0 = off .. 1 = strongest glitch suppression
    float sustain = 0.0f;      // SUSTAIN 0 = off .. ~1 = notes ring on below the gate; >= 0.98 = HOLD
};

class PitchTracker
{
public:
    void prepare(double sampleRate)
    {
        fs = (float) sampleRate;
        // Allocate for the larger of the two ranges.
        size_t maxHist = 0, maxTau = 0;
        for (bool b : { false, true })
        {
            const auto c = configFor(b);
            maxHist = std::max(maxHist, (size_t) (c.window + c.tauMax + 2));
            maxTau = std::max(maxTau, (size_t) (c.tauMax + 2));
        }
        hist.assign(maxHist, 0.0f);
        diff.assign(maxTau, 0.0f);
        configured = -1;
        reset();
    }

    void reset()
    {
        std::fill(hist.begin(), hist.end(), 0.0f);
        histLen = 0;
        decimPhase = hopPhase = 0;
        for (auto& s : lpf)
            s = {};
        dcX = dcY = 0.0f;
        env = slowEnv = 0.0f;
        gateOpen = false;
        gateHold = 0;
        onset = false;
        sinceOnset = 1 << 20;
        peakSinceOnset = 0.0f;
        noteOn = false;
        note = -1;
        lastBend = 0.0f;
        lastMidi = -100.0f;
        stable = 0;
        velocityNow = 100;
        recentNote = -1;
        sinceNoteEnd = 1 << 20;
        lastAperiodicity = 1.0f;
        lostHops = 0;
        uiLevelDb.store(-120.0f);
        uiFrequency.store(0.0f);
        uiNote.store(-1);
        uiGate.store(false);
    }

    /** Appends this block's MIDI events (offsets within the block) to `out`; returns how many. */
    int process(const float* in, int n, const TrackerSettings& s, MidiEvent* out, int maxOut)
    {
        if (configured != (s.bass ? 1 : 0))
            configure(s.bass);
        count = 0;
        events = out;
        capacity = maxOut;
        channel = std::clamp(s.channel, 1, 16);

        const float gain = std::pow(10.0f, s.inputGainDb / 20.0f);
        const float gateOn = std::pow(10.0f, s.gateDb / 20.0f);
        const float gateOff = gateOn * 0.5f; // 6 dB hysteresis
        const float sus = std::clamp(s.sustain, 0.0f, 1.0f);
        hold = sus >= 0.98f;
        sustaining = sus > 0.001f;
        tailFloor = gateOff * std::pow(10.0f, -36.0f * sus / 20.0f); // SUSTAIN: how far below the gate a note may ring
        const float thr = 0.25f - 0.17f * std::clamp(s.threshold, 0.0f, 1.0f);
        const int needStable = 1 + (int) std::lround(2.0f * std::clamp(s.threshold, 0.0f, 1.0f));

        for (int i = 0; i < n; ++i)
        {
            // DC block, then the level envelope (fast attack) and a slower one for re-pick detection.
            const float raw = in[i] * gain;
            dcY = raw - dcX + dcR * dcY;
            dcX = raw;
            const float x = dcY;
            const float a = std::abs(x);
            env = a > env ? a + (env - a) * envAttack : a + (env - a) * envRelease;
            slowEnv = a > slowEnv ? a + (slowEnv - a) * slowAttack : a + (slowEnv - a) * slowRelease;
            ++sinceOnset;
            if (env > gateOn && env > slowEnv * 2.2f && sinceOnset > minOnsetGap)
            {
                onset = true;
                sinceOnset = 0;
                sinceOnsetDecim = 0;
                peakSinceOnset = 0.0f;
            }
            peakSinceOnset = std::max(peakSinceOnset, env);

            // Gate with hysteresis and a short hold.
            if (env > gateOn)
            {
                gateOpen = true;
                gateHold = holdSamples;
            }
            else if (env < gateOff && gateOpen && --gateHold <= 0)
            {
                gateOpen = false;
                if (noteOn && ! sustaining)
                    endNote(i);
            }
            // SUSTAIN: a note ringing below the gate ends when it fades under the
            // floor (not with HOLD) or the pitch has been lost for a while.
            if (noteOn && ! gateOpen && sustaining && ! hold && (env < tailFloor || lostHops > lostLimit))
                endNote(i);
            // HOLD: a pick that brings no pitch (a muted strum) stops the held note.
            if (noteOn && hold && onset && sinceOnset > muteSamples)
            {
                onset = false;
                endNote(i);
            }

            // Anti-alias filter and decimation for the pitch detector.
            float y = x;
            for (auto& st : lpf)
            {
                const float o = st.b0 * y + st.z1;
                st.z1 = st.b1 * y - st.a1 * o + st.z2;
                st.z2 = st.b2 * y - st.a2 * o;
                y = o;
            }
            if (++decimPhase < cfg.decim)
                continue;
            decimPhase = 0;
            push(y);
            if (++hopPhase < cfg.hop)
                continue;
            hopPhase = 0;
            analyse(i, s, thr, needStable, gateOn);
        }

        uiLevelDb.store(20.0f * std::log10(env + 1.0e-6f));
        uiGate.store(gateOpen);
        uiNote.store(noteOn ? note : -1);
        return count;
    }

    /** Ends a sounding note (when tracking is switched off). */
    int release(MidiEvent* out, int maxOut, int ch)
    {
        count = 0;
        events = out;
        capacity = maxOut;
        channel = std::clamp(ch, 1, 16);
        if (noteOn)
            endNote(0);
        return count;
    }

    // For the display (any thread).
    float getLevelDb() const { return uiLevelDb.load(); }
    float getFrequency() const { return uiFrequency.load(); }
    int getNote() const { return uiNote.load(); }
    bool isGateOpen() const { return uiGate.load(); }

    /** Latency of one analysis hop, in seconds (detection itself needs ~1-2 periods more). */
    float getHopSeconds() const { return (float) (cfg.hop * cfg.decim) / fs; }

private:
    struct Config { int decim, hop, window, tauMin, tauMax; float fsd, cutoff; };
    struct Biquad { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; };

    Config configFor(bool bass) const
    {
        Config c {};
        c.decim = std::max(1, (int) std::lround(fs / (bass ? 6000.0f : 12000.0f)));
        c.fsd = fs / (float) c.decim;
        const float fmin = bass ? 28.0f : 70.0f, fmax = bass ? 450.0f : 1400.0f;
        c.tauMax = (int) std::ceil(c.fsd / fmin);
        c.tauMin = std::max(2, (int) std::floor(c.fsd / fmax));
        c.window = c.tauMax;
        c.hop = std::max(1, (int) std::lround(c.fsd * (bass ? 0.004f : 0.0025f)));
        c.cutoff = std::min(bass ? 1200.0f : 2500.0f, 0.4f * c.fsd);
        return c;
    }

    void configure(bool bass)
    {
        configured = bass ? 1 : 0;
        cfg = configFor(bass);
        // 4th-order Butterworth low-pass: two biquads (Q 0.541 and 1.307).
        const float qs[2] = { 0.5412f, 1.3066f };
        for (int k = 0; k < 2; ++k)
        {
            const float w = 2.0f * 3.14159265f * cfg.cutoff / fs;
            const float alpha = std::sin(w) / (2.0f * qs[k]);
            const float cw = std::cos(w);
            const float a0 = 1.0f + alpha;
            lpf[k].b0 = (1.0f - cw) * 0.5f / a0;
            lpf[k].b1 = (1.0f - cw) / a0;
            lpf[k].b2 = lpf[k].b0;
            lpf[k].a1 = -2.0f * cw / a0;
            lpf[k].a2 = (1.0f - alpha) / a0;
            lpf[k].z1 = lpf[k].z2 = 0.0f;
        }
        dcR = std::exp(-2.0f * 3.14159265f * (bass ? 15.0f : 40.0f) / fs);
        envAttack = std::exp(-1.0f / (0.0007f * fs));
        envRelease = std::exp(-1.0f / (0.045f * fs));
        slowAttack = std::exp(-1.0f / (0.012f * fs));
        slowRelease = std::exp(-1.0f / (0.25f * fs));
        holdSamples = (int) (0.03f * fs);
        muteSamples = (int) (0.15f * fs);
        lostLimit = std::max(2, (int) std::lround(0.06f * cfg.fsd / (float) cfg.hop));
        // CLEAN UP timing, in analysis hops: up to ~20 ms (guitar) / ~32 ms (bass) of extra
        // confirmation for suspicious readings; "just played" = the last 0.4 s.
        cleanHops = 8;
        recentHops = (int) std::lround(0.4f * cfg.fsd / (float) cfg.hop);
        minOnsetGap = (int) (0.07f * fs);
        histLen = 0;
        decimPhase = hopPhase = 0;
        lastMidi = -100.0f;
        stable = 0;
    }

    void push(float v)
    {
        ++sinceOnsetDecim;
        const int cap = cfg.window + cfg.tauMax + 1;
        if (histLen < cap)
        {
            hist[(size_t) histLen++] = v;
            return;
        }
        std::copy(hist.begin() + 1, hist.begin() + cap, hist.begin());
        hist[(size_t) cap - 1] = v;
    }

    /** YIN on the newest window; returns the frequency, or 0 when unvoiced.
        Just after a pick the window starts at the pick (skipping its first
        2 ms of noise), so short periods are found before the full window of
        the lowest note has filled. */
    float detect(float thr)
    {
        int W = cfg.window, tauLim = cfg.tauMax;
        const int skip = (int) (0.002f * cfg.fsd);
        const int sinceAttack = sinceOnsetDecim - skip;
        int avail = std::min(histLen, W + cfg.tauMax + 1);
        bool early = false;
        if (sinceAttack < W + cfg.tauMax + 1)
        {
            if (sinceAttack < 3 * cfg.tauMin)
                return 0.0f;
            avail = std::min(histLen, sinceAttack);
            tauLim = std::min(cfg.tauMax, avail * 3 / 5);
            W = std::min(cfg.window, avail - tauLim - 1);
            early = true;
        }
        if (histLen < W + tauLim + 1 || tauLim <= cfg.tauMin)
            return 0.0f;
        const float* x = hist.data() + (histLen - W - tauLim - 1);
        double energy = 0.0;
        for (int j = 0; j < W + tauLim; ++j)
            energy += (double) x[j] * x[j];
        if (energy < 1.0e-9 * (W + tauLim))
            return 0.0f;

        float* d = diff.data();
        d[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauLim; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const float delta = x[j] - x[j + tau];
                sum += delta * delta;
            }
            running += sum;
            d[tau] = running > 0.0 ? (float) (sum * tau / running) : 1.0f;
        }
        int best = -1;
        for (int tau = cfg.tauMin; tau < tauLim; ++tau)
        {
            if (d[tau] < thr)
            {
                while (tau + 1 < tauLim && d[tau + 1] < d[tau])
                    ++tau;
                best = tau;
                break;
            }
        }
        // From a short window, only trust periods short enough that a period
        // twice as long was also examined (guards against octave-up errors).
        if (best < 0 || (early && 2 * best > tauLim) || best >= tauLim)
            return 0.0f;
        lastAperiodicity = d[best];
        float refined = (float) best;
        if (best > 1)
        {
            const float s0 = d[best - 1], s1 = d[best], s2 = d[best + 1];
            const float den = s0 + s2 - 2.0f * s1;
            if (std::abs(den) > 1.0e-9f)
                refined += 0.5f * (s0 - s2) / den;
        }
        return cfg.fsd / refined;
    }

    void analyse(int offset, const TrackerSettings& s, float thr, int needStable, float gateOn)
    {
        if (sinceNoteEnd < (1 << 20))
            ++sinceNoteEnd;
        // Below the gate the detector keeps listening only to a note ringing on (SUSTAIN).
        const bool tail = ! gateOpen && noteOn && sustaining;
        const float f = (gateOpen || tail) ? detect(thr) : 0.0f;
        const bool voiced = f > 0.0f;
        uiFrequency.store(f);
        if (! voiced)
        {
            stable = 0;
            lastMidi = -100.0f;
            ++lostHops;
            return;
        }
        const float midi = 69.0f + 12.0f * std::log2(f / 440.0f);
        if (noteOn)
        {
            // Still the note being played (or its bend)? Then it is not lost.
            const float expected = (float) note + lastBend;
            if (std::abs(midi - expected) < 1.0f)
                lostHops = 0;
            else
                ++lostHops;
        }
        if (tail)
        {
            // Ringing on below the gate: follow the bend, but never start or change notes here.
            if (s.bendFollow && std::abs(midi - (float) note) < std::max(0.5f, s.bendRange) - 0.25f
                && std::abs((midi - (float) note) - lastBend) > 0.02f && std::abs(midi - ((float) note + lastBend)) < 1.0f)
                sendBend(offset, midi - (float) note, std::max(0.5f, s.bendRange));
            return;
        }
        stable = std::abs(midi - lastMidi) < 0.5f ? stable + 1 : 1;
        lastMidi = midi;

        // CLEAN UP: does this reading look like a glitch? (see the header)
        const float clean = std::clamp(s.cleanUp, 0.0f, 1.0f);
        int need = needStable;
        if (clean > 0.0f)
        {
            const bool picked = onset; // a pick attack since the last note started
            const int ref = noteOn ? note : (sinceNoteEnd < recentHops ? recentNote : -1);
            const float drop = ref >= 0 ? (float) ref - midi : 0.0f;
            const float lowest = s.bass ? 23.0f : 38.0f; // low B of a 5-string bass / drop-D guitar
            const bool weak = lastAperiodicity > thr * 0.55f;
            bool suspicious = false;
            if (! picked && drop >= 5.0f)
                suspicious = true;                                      // fell well below what is playing
            if (! picked && ! noteOn && ref >= 0)
                suspicious = true;                                      // re-voiced in a tail, no new pick
            if (midi < lowest - 0.5f)
                suspicious = true;                                      // below the instrument's range
            if (weak && drop > 0.5f)
                suspicious = true;                                      // a doubtful reading, and lower
            if (suspicious)
            {
                // An unpicked exact subharmonic of the note is a detector error: never play it.
                if (! picked && ref >= 0 && clean >= 0.25f)
                    for (float sub : { 12.0f, 19.0f, 24.0f })
                        if (std::abs(drop - sub) < 0.45f)
                            return;
                need += (int) std::lround(clean * (float) cleanHops);
            }
        }
        if (stable < need)
            return;

        const float bendRange = std::max(0.5f, s.bendRange);
        if (! noteOn)
        {
            startNote(offset, midi, s, gateOn);
            return;
        }
        if (onset)
        {
            // A fresh pick: retrigger (the same note or a new one).
            onset = false;
            endNote(offset);
            startNote(offset, midi, s, gateOn);
            return;
        }
        if (s.bendFollow)
        {
            const float off = midi - (float) note;
            if (std::abs(off) > bendRange - 0.25f)
            {
                legato(offset, midi, s);
            }
            else if (std::abs(off - lastBend) > 0.02f)
            {
                sendBend(offset, off, bendRange);
            }
        }
        else
        {
            const int nearest = (int) std::lround(midi);
            if (nearest != note && std::abs(midi - (float) nearest) < 0.35f)
                legato(offset, midi, s);
        }
    }

    void startNote(int offset, float midi, const TrackerSettings& s, float gateOn)
    {
        onset = false;
        lostHops = 0;
        note = std::clamp((int) std::lround(midi), 0, 127);
        const float peakDb = 20.0f * std::log10(std::max(peakSinceOnset, env) + 1.0e-9f);
        const float gateDb = 20.0f * std::log10(gateOn);
        const float v = std::clamp((peakDb - gateDb) / 36.0f, 0.0f, 1.0f);
        const float amt = std::clamp(s.velocity, 0.0f, 1.0f);
        velocityNow = std::clamp((int) std::lround(100.0f * (1.0f - amt) + amt * (20.0f + 107.0f * v)), 1, 127);
        if (s.bendFollow)
            sendBend(offset, midi - (float) note, std::max(0.5f, s.bendRange));
        else if (lastBend != 0.0f)
            sendBend(offset, 0.0f, 1.0f);
        emit(offset, (uint8_t) (0x90 | (channel - 1)), (uint8_t) note, (uint8_t) velocityNow);
        noteOn = true;
    }

    void legato(int offset, float midi, const TrackerSettings& s)
    {
        const int old = note;
        note = std::clamp((int) std::lround(midi), 0, 127);
        if (s.bendFollow)
            sendBend(offset, midi - (float) note, std::max(0.5f, s.bendRange));
        emit(offset, (uint8_t) (0x90 | (channel - 1)), (uint8_t) note, (uint8_t) velocityNow);
        emit(offset, (uint8_t) (0x80 | (channel - 1)), (uint8_t) old, 0);
    }

    void endNote(int offset)
    {
        recentNote = note;
        sinceNoteEnd = 0;
        emit(offset, (uint8_t) (0x80 | (channel - 1)), (uint8_t) note, 0);
        noteOn = false;
        note = -1;
        if (lastBend != 0.0f)
            sendBend(offset, 0.0f, 1.0f);
    }

    void sendBend(int offset, float semitones, float range)
    {
        lastBend = semitones;
        const int v = std::clamp(8192 + (int) std::lround(semitones / range * 8191.0f), 0, 16383);
        emit(offset, (uint8_t) (0xE0 | (channel - 1)), (uint8_t) (v & 127), (uint8_t) (v >> 7));
    }

    void emit(int offset, uint8_t status, uint8_t d1, uint8_t d2)
    {
        if (count < capacity)
            events[count++] = { offset, status, d1, d2 };
    }

    float fs = 48000.0f;
    Config cfg {};
    int configured = -1;
    Biquad lpf[2];
    float dcR = 0.995f, dcX = 0.0f, dcY = 0.0f;
    float envAttack = 0, envRelease = 0, slowAttack = 0, slowRelease = 0;
    float env = 0.0f, slowEnv = 0.0f, peakSinceOnset = 0.0f;
    int holdSamples = 0, gateHold = 0, minOnsetGap = 0, sinceOnset = 0;
    // SUSTAIN / HOLD
    bool hold = false, sustaining = false;
    float tailFloor = 0.0f;
    int muteSamples = 0, lostLimit = 2, lostHops = 0;
    bool gateOpen = false, onset = false;

    std::vector<float> hist, diff;
    int histLen = 0, decimPhase = 0, hopPhase = 0, sinceOnsetDecim = 1 << 20;

    bool noteOn = false;
    int note = -1, velocityNow = 100, stable = 0;
    int recentNote = -1, sinceNoteEnd = 1 << 20, cleanHops = 8, recentHops = 160;
    float lastAperiodicity = 1.0f;
    float lastMidi = -100.0f, lastBend = 0.0f;

    MidiEvent* events = nullptr;
    int count = 0, capacity = 0, channel = 1;

    std::atomic<float> uiLevelDb { -120.0f }, uiFrequency { 0.0f };
    std::atomic<int> uiNote { -1 };
    std::atomic<bool> uiGate { false };
};

} // namespace zsynth::dsp
