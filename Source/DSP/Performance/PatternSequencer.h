#pragma once

// PATTERN SEQUENCER: a two-row analog-style step sequencer (in the spirit of
// the 1970s 16-step units: a pitch knob, a gate, an accent and a slide per
// step), playing the UPPER and LOWER sounds.
//
// CLOCK. Everything runs on a beat clock: step k of a row starts at beat
// k * stepBeats, with SWING delaying every second step. Free-running, the
// clock starts at 0 when RUN is pressed and follows the tempo; with HOST SYNC
// in a host that is playing, it is the host's song position, so the rows stay
// locked to the bars. Each row has its own length, direction and step size
// (polymeter is allowed).
//
// NOTES. A step with its gate on plays root + octave + pitch (+ key
// transpose) at the row's velocity (127 when accented) for GATE % of the step.
// A SLIDE step is held into the next one, which starts before it ends
// (legato: mono / legato voices glide). A step whose gate is off is a rest.
// Each note ends on the parts it started on, even if the routing changes.
//
// Real-time safe: no allocation; events go to a caller-provided sink.

#include "../Engine/SynthEngine.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace zsynth::dsp
{

struct PseqStep
{
    int pitch = 0; // semitones from the row's root
    bool gate = true, accent = false, slide = false;
};

struct PseqRow
{
    static constexpr int kSteps = 16;
    bool on = true;
    int partMask = 1;          // bit 0 = UPPER, bit 1 = LOWER
    int length = 16;           // 1..16
    int direction = 0;         // PseqDirection
    double stepBeats = 0.25;   // a 1/16 step = a quarter of a beat
    float gate = 0.5f;         // 0.05..1 of the step
    int root = 60;             // MIDI note of pitch 0, octave included
    int velocity = 96;
    PseqStep steps[kSteps];
};

enum class PseqDirection : int { Forward, Backward, Pendulum, Random, Count };

struct PseqSettings
{
    bool run = false;
    bool hostSync = false;   // follow the host's transport and song position
    double bpm = 120.0;
    float swing = 0.5f;      // 0.5 = straight .. 0.75 = heavy shuffle (share of a step pair)
    int transpose = 0;       // semitones (KEY TRANSPOSE)
    int channel = 0;         // MIDI channel nibble of the events
    PseqRow rows[2];
};

class PatternSequencer
{
public:
    static constexpr int kRows = 2;

    /** Receives events: part 0 = UPPER, 1 = LOWER. */
    struct Sink
    {
        virtual ~Sink() = default;
        virtual void add(int part, const MidiEvent& e) = 0;
    };

    void prepare(double sampleRate)
    {
        fs = sampleRate;
        reset();
    }

    void reset()
    {
        running = false;
        clock = 0.0;
        for (auto& r : rows)
            r = {};
        for (auto& s : shownStep)
            s.store(-1);
    }

    /** One block of n samples. transport: the host's (ppq valid and playing for HOST SYNC). */
    void process(int n, const PseqSettings& s, const TransportInfo& transport, Sink& sink)
    {
        const bool hostDriven = s.hostSync && transport.ppqValid;
        const bool shouldRun = hostDriven ? transport.playing : s.run;
        const double bpm = std::max(20.0, hostDriven && transport.hostTempo ? transport.bpm : s.bpm);
        const double beatsPerSample = bpm / (60.0 * fs);

        if (! shouldRun)
        {
            if (running)
                stopAll(0, sink);
            running = false;
            return;
        }

        double beat0 = clock;
        if (hostDriven)
        {
            // Follow the song position; a jump (locate, loop) re-aligns the rows.
            const double ppq = transport.ppqPosition;
            if (! running || std::abs(ppq - clock) > 0.01)
            {
                stopAll(0, sink);
                for (int r = 0; r < kRows; ++r)
                    rows[r].nextK = firstStepAtOrAfter(ppq, s.rows[r], s.swing);
            }
            beat0 = ppq;
        }
        else if (! running)
        {
            beat0 = 0.0;
            for (auto& r : rows)
            {
                r.nextK = 0;
                r.offBeat = kNever;
            }
        }
        running = true;
        const double beat1 = beat0 + (double) n * beatsPerSample;
        auto offsetOf = [&](double b) { return std::clamp((int) std::floor((b - beat0) / beatsPerSample), 0, n - 1); };

        for (int r = 0; r < kRows; ++r)
        {
            auto& st = rows[r];
            const auto& row = s.rows[r];
            for (;;)
            {
                const double stepBeat = stepStart(st.nextK, row, s.swing);
                const double next = std::min(stepBeat, st.offBeat);
                if (next >= beat1)
                    break;
                if (st.offBeat <= stepBeat)
                {
                    noteOff(r, offsetOf(st.offBeat), sink);
                    continue;
                }
                fireStep(r, st.nextK, offsetOf(stepBeat), stepBeat, row, s, sink);
                ++st.nextK;
            }
        }
        clock = beat1;
    }

    /** Ends every sounding note now (e.g. PANIC or a pattern change). */
    void allNotesOff(Sink& sink) { stopAll(0, sink); }

    /** The step each row is on (0..15), or -1 when stopped: for the LEDs (any thread). */
    int getShownStep(int row) const { return shownStep[row].load(std::memory_order_relaxed); }

    /** Step index played by the k-th step of a row (0-based), for any direction. */
    static int stepIndex(long long k, const PseqRow& row)
    {
        const int len = std::clamp(row.length, 1, PseqRow::kSteps);
        if (len == 1)
            return 0;
        switch ((PseqDirection) row.direction)
        {
            case PseqDirection::Backward: return len - 1 - (int) (k % len);
            case PseqDirection::Pendulum:
            {
                const int period = 2 * len - 2;
                const int p = (int) (k % period);
                return p < len ? p : period - p;
            }
            case PseqDirection::Random:
            {
                // Deterministic per step count, so a host-synced pattern repeats the same "random" run.
                uint64_t x = (uint64_t) k * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
                x ^= x >> 31; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 29;
                return (int) (x % (uint64_t) len);
            }
            default: return (int) (k % len);
        }
    }

private:
    static constexpr double kNever = std::numeric_limits<double>::infinity();

    struct RowState
    {
        long long nextK = 0;
        double offBeat = kNever;
        int note = -1;         // sounding note, or -1
        int mask = 0;          // parts it is sounding on
        bool held = false;     // the sounding step slides into the next
    };

    static double stepStart(long long k, const PseqRow& row, float swing)
    {
        // Every second step is pushed late by the swing.
        const double b = std::max(1.0e-3, row.stepBeats);
        const double push = (k & 1) ? (std::clamp((double) swing, 0.5, 0.75) - 0.5) * 2.0 * b : 0.0;
        return (double) k * b + push;
    }

    static long long firstStepAtOrAfter(double beat, const PseqRow& row, float swing)
    {
        const double b = std::max(1.0e-3, row.stepBeats);
        long long k = std::max(0LL, (long long) std::floor(beat / b) - 1);
        while (stepStart(k, row, swing) < beat - 1.0e-9)
            ++k;
        return k;
    }

    void fireStep(int r, long long k, int offset, double beat, const PseqRow& row, const PseqSettings& s, Sink& sink)
    {
        auto& st = rows[r];
        const int idx = stepIndex(k, row);
        shownStep[r].store(idx, std::memory_order_relaxed);
        const auto& step = row.steps[idx];
        if (! row.on || ! step.gate || row.partMask == 0)
        {
            noteOff(r, offset, sink); // a rest ends a held (slid) note too
            return;
        }
        const int note = std::clamp(row.root + step.pitch + s.transpose, 0, 127);
        const int vel = step.accent ? 127 : std::clamp(row.velocity, 1, 127);
        const double stepLen = stepStart(k + 1, row, s.swing) - beat;
        if (st.note >= 0 && st.held && st.mask == row.partMask)
        {
            // Slide from the held note: the new note starts first, then the old one ends (legato).
            if (note != st.note)
            {
                send(st.mask, offset, 0x90, note, vel, s.channel, sink);
                send(st.mask, offset, 0x80, st.note, 0, s.channel, sink);
            }
        }
        else
        {
            noteOff(r, offset, sink);
            send(row.partMask, offset, 0x90, note, vel, s.channel, sink);
        }
        st.note = note;
        st.mask = row.partMask;
        st.held = step.slide;
        st.offBeat = step.slide ? kNever : beat + stepLen * std::clamp((double) row.gate, 0.02, 1.0) * 0.999;
        channelOf[r] = s.channel;
    }

    void noteOff(int r, int offset, Sink& sink)
    {
        auto& st = rows[r];
        if (st.note >= 0)
            send(st.mask, offset, 0x80, st.note, 0, channelOf[r], sink);
        st.note = -1;
        st.held = false;
        st.offBeat = kNever;
    }

    void stopAll(int offset, Sink& sink)
    {
        for (int r = 0; r < kRows; ++r)
        {
            noteOff(r, offset, sink);
            shownStep[r].store(-1, std::memory_order_relaxed);
        }
    }

    static void send(int mask, int offset, int kind, int note, int vel, int channel, Sink& sink)
    {
        MidiEvent e;
        e.offset = offset;
        e.status = (uint8_t) (kind | (channel & 0x0F));
        e.data1 = (uint8_t) note;
        e.data2 = (uint8_t) vel;
        for (int part = 0; part < 2; ++part)
            if ((mask >> part) & 1)
                sink.add(part, e);
    }

    double fs = 48000.0;
    bool running = false;
    double clock = 0.0; // beats
    RowState rows[kRows];
    int channelOf[kRows] {};
    std::atomic<int> shownStep[kRows] { -1, -1 };
};

} // namespace zsynth::dsp
