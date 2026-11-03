#include "WavetableBank.h"

#include "../../Parameters/ParamChoices.h"
#include "../Common/FFT.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <cstdint>
#include <cmath>
#include <complex>
#include <functional>

namespace zsynth::dsp
{
namespace
{
    constexpr double kPiD = 3.14159265358979323846;
    constexpr int kAnalysisSize = 8192; // time-domain definitions are analysed at 4x resolution

    /** x(p) = sum_k a[k] cos(2 pi k p) + b[k] sin(2 pi k p), k = 1..kMaxHarmonics. */
    struct Harmonics
    {
        std::vector<double> a = std::vector<double>(Wavetable::kMaxHarmonics + 1, 0.0);
        std::vector<double> b = std::vector<double>(Wavetable::kMaxHarmonics + 1, 0.0);
    };

    using SpectralFn = std::function<void(double t, Harmonics&)>;
    using TimeFn     = std::function<double(double p, double t)>;

    Harmonics analyse(const TimeFn& fn, double t)
    {
        std::vector<std::complex<double>> buf(kAnalysisSize);
        for (int n = 0; n < kAnalysisSize; ++n)
            buf[(size_t) n] = { fn((double) n / kAnalysisSize, t), 0.0 };
        fftInPlace(buf, false);

        Harmonics h;
        for (int k = 1; k <= Wavetable::kMaxHarmonics; ++k)
        {
            h.a[(size_t) k] =  2.0 * buf[(size_t) k].real() / kAnalysisSize;
            h.b[(size_t) k] = -2.0 * buf[(size_t) k].imag() / kAnalysisSize;
        }
        return h;
    }

    void renderFrame(const Harmonics& h, Wavetable& wt, int frameIndex)
    {
        std::vector<std::complex<double>> buf;
        std::vector<std::vector<float>> mips(Wavetable::kNumMips);

        for (int m = 0; m < Wavetable::kNumMips; ++m)
        {
            const int N = Wavetable::mipSize(m);
            buf.assign((size_t) N, std::complex<double>());
            const int top = Wavetable::kMaxHarmonics >> m;
            for (int k = 1; k <= top && k < N / 2; ++k)
            {
                const std::complex<double> c(h.a[(size_t) k] * 0.5, -h.b[(size_t) k] * 0.5);
                buf[(size_t) k] = c;
                buf[(size_t) (N - k)] = std::conj(c);
            }
            fftInPlace(buf, true);
            mips[(size_t) m].resize((size_t) N);
            for (int n = 0; n < N; ++n)
                mips[(size_t) m][(size_t) n] = (float) buf[(size_t) n].real();
        }

        // Normalise every mip of the frame by the full-bandwidth peak so that
        // mip changes never alter loudness.
        float peak = 1.0e-9f;
        for (float v : mips[0])
            peak = std::max(peak, std::abs(v));
        const float scale = 1.0f / peak;

        for (int m = 0; m < Wavetable::kNumMips; ++m)
        {
            const int N = Wavetable::mipSize(m);
            float* dst = const_cast<float*>(wt.frame(frameIndex, m));
            for (int n = 0; n < N; ++n)
                dst[n] = mips[(size_t) m][(size_t) n] * scale;
            dst[N] = dst[0];
        }
    }

    Wavetable buildSpectral(const SpectralFn& fn)
    {
        Wavetable wt;
        wt.data.resize((size_t) Wavetable::kFrames * Wavetable::kFrameStride);
        for (int f = 0; f < Wavetable::kFrames; ++f)
        {
            Harmonics h;
            fn((double) f / (Wavetable::kFrames - 1), h);
            renderFrame(h, wt, f);
        }
        return wt;
    }

    Wavetable buildTimeDomain(const TimeFn& fn)
    {
        Wavetable wt;
        wt.data.resize((size_t) Wavetable::kFrames * Wavetable::kFrameStride);
        for (int f = 0; f < Wavetable::kFrames; ++f)
            renderFrame(analyse(fn, (double) f / (Wavetable::kFrames - 1)), wt, f);
        return wt;
    }

    // ------------------------------------------------------------- generators
    // Classic shapes as sine series: sine -> triangle -> saw -> square.
    void analogMorph(double t, Harmonics& h)
    {
        auto tri = [](int k) { return (k % 2 == 1) ? 8.0 / (kPiD * kPiD * k * k) * (((k - 1) / 2) % 2 == 0 ? 1.0 : -1.0) : 0.0; };
        auto saw = [](int k) { return 2.0 / (kPiD * k); };
        auto sqr = [](int k) { return (k % 2 == 1) ? 4.0 / (kPiD * k) : 0.0; };
        auto sine = [](int k) { return k == 1 ? 1.0 : 0.0; };

        const double seg = t * 3.0;
        for (int k = 1; k <= Wavetable::kMaxHarmonics; ++k)
        {
            double v;
            if (seg < 1.0)      v = sine(k) + (tri(k) - sine(k)) * seg;
            else if (seg < 2.0) v = tri(k) + (saw(k) - tri(k)) * (seg - 1.0);
            else                v = saw(k) + (sqr(k) - saw(k)) * (seg - 2.0);
            h.b[(size_t) k] = v;
        }
    }

    // A smooth "opening filter": the harmonic count rises exponentially, with
    // the topmost harmonic faded in fractionally so frames never step.
    void harmonicSweep(double t, Harmonics& h)
    {
        const double count = std::pow(2.0, t * 8.0); // 1 .. 256 harmonics
        for (int k = 1; k <= Wavetable::kMaxHarmonics; ++k)
        {
            const double w = std::clamp(count - (double) k + 1.0, 0.0, 1.0);
            h.b[(size_t) k] = w / (double) k * (k % 2 == 0 ? 0.8 : 1.0);
        }
    }

    // Formant series: five vowels morphing A-E-I-O-U. Formant frequencies are
    // expressed relative to a nominal 110 Hz fundamental, so the formants move
    // with pitch (like any wavetable) but the vowel colour is clearly audible.
    void vocalFormant(double t, Harmonics& h)
    {
        struct Vowel { double f1, f2, f3; };
        static const Vowel vowels[] = { { 800, 1150, 2900 }, { 400, 2000, 2550 }, { 280, 2250, 2900 },
                                        { 450, 800, 2830 },  { 325, 700, 2700 } };
        const double pos = t * 4.0;
        const int i0 = std::min(3, (int) pos);
        const double fr = pos - i0;
        const Vowel v { vowels[i0].f1 + (vowels[i0 + 1].f1 - vowels[i0].f1) * fr,
                        vowels[i0].f2 + (vowels[i0 + 1].f2 - vowels[i0].f2) * fr,
                        vowels[i0].f3 + (vowels[i0 + 1].f3 - vowels[i0].f3) * fr };
        for (int k = 1; k <= 160; ++k)
        {
            const double f = 110.0 * k;
            auto peak = [f](double fc, double bw, double g) { const double d = (f - fc) / bw; return g * std::exp(-d * d); };
            const double amp = (peak(v.f1, 90, 1.0) + peak(v.f2, 120, 0.6) + peak(v.f3, 160, 0.3) + 0.04) / std::sqrt((double) k);
            h.a[(size_t) k] = amp; // cosine phase gives a sharper, more glottal pulse
        }
    }

    double pwmScan(double p, double t)
    {
        const double width = 0.5 - 0.48 * t;
        return p < width ? 1.0 : -1.0;
    }

    // Phase-modulation spectra with integer ratios: rising index brings in
    // dense, bell-like metallic partials.
    double digitalBell(double p, double t)
    {
        const double index = t * 5.0;
        const double m1 = std::sin(2.0 * kPiD * 7.0 * p);
        const double m2 = std::sin(2.0 * kPiD * 3.0 * p);
        return std::sin(2.0 * kPiD * p + index * m1 + 0.6 * index * m2);
    }

    // Hard-synced sawtooth with the slave ratio swept from 1 to 8.
    double syncSweep(double p, double t)
    {
        const double ratio = 1.0 + 7.0 * t;
        const double s = p * ratio;
        return 2.0 * (s - std::floor(s)) - 1.0;
    }

    // West-coast style sine folder with a little asymmetry for even harmonics.
    double wavefolder(double p, double t)
    {
        const double gain = 1.0 + 7.0 * t;
        const double x = std::sin(2.0 * kPiD * p) * gain + 0.15 * t;
        return std::sin(0.5 * kPiD * x);
    }

    // Tonewheel-organ registrations morphing from flute to full, using the
    // harmonics of 16', 8', 5 1/3', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1'.
    void drawbarOrgan(double t, Harmonics& h)
    {
        static const int harmonic[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
        static const double regs[5][9] = {
            { 0, 8, 0, 0, 0, 0, 0, 0, 0 },
            { 8, 8, 6, 0, 0, 0, 0, 0, 0 },
            { 8, 8, 8, 0, 0, 0, 0, 0, 8 },
            { 8, 8, 8, 8, 6, 4, 0, 0, 4 },
            { 8, 8, 8, 8, 8, 8, 8, 8, 8 },
        };
        const double pos = t * 4.0;
        const int i0 = std::min(3, (int) pos);
        const double fr = pos - i0;
        for (int d = 0; d < 9; ++d)
        {
            const double level = regs[i0][d] + (regs[i0 + 1][d] - regs[i0][d]) * fr;
            // Each drawbar step is ~3 dB.
            h.b[(size_t) harmonic[d]] += level > 0.0 ? std::pow(10.0, (level - 8.0) * 3.0 / 20.0) : 0.0;
        }
    }

    // ======================================================== sequence tables
    // Small deterministic hash so every build makes the same tables.
    double hash01(int a, int b)
    {
        uint32_t h = (uint32_t) a * 0x9E3779B1u ^ ((uint32_t) b + 0x7F4A7C15u) * 0x85EBCA77u;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
        return (double) (h & 0xFFFFFF) / (double) 0x1000000;
    }

    int frameOf(double t) { return (int) std::lround(t * (Wavetable::kFrames - 1)); }

    /** A * sin(2 pi k p + phi). */
    void addPartial(Harmonics& h, int k, double amp, double phi = 0.0)
    {
        if (k < 1 || k > Wavetable::kMaxHarmonics)
            return;
        h.a[(size_t) k] += amp * std::sin(phi);
        h.b[(size_t) k] += amp * std::cos(phi);
    }

    double formant(double f, double fc, double bw, double g) { const double d = (f - fc) / bw; return g * std::exp(-d * d); }

    // Each frame is the fundamental plus one strong harmonic (2nd .. 33rd):
    // sequenced, it plays the harmonic series as a melody.
    void seqHarmonics(double t, Harmonics& h)
    {
        const int f = frameOf(t);
        addPartial(h, 1, 0.55);
        addPartial(h, f + 2, 1.0);
        addPartial(h, 2 * (f + 2), 0.18);
    }

    // 32 unrelated digital waves of varied brightness and holes.
    void seqDigital(double t, Harmonics& h)
    {
        const int f = frameOf(t);
        const double tilt = 0.5 + 1.2 * hash01(f, 1);
        const int top = 8 + (int) (hash01(f, 2) * 120.0);
        const int holeEvery = 2 + (int) (hash01(f, 3) * 5.0);
        const bool oddOnly = hash01(f, 4) > 0.7;
        for (int k = 1; k <= top; ++k)
        {
            if (k > 1 && k % holeEvery == 0) continue;
            if (oddOnly && k % 2 == 0) continue;
            const double peak = formant((double) k, 4.0 + hash01(f, 5) * 30.0, 3.0 + hash01(f, 6) * 6.0, 1.5 * hash01(f, 7));
            addPartial(h, k, (1.0 / std::pow((double) k, tilt)) * (1.0 + peak), hash01(f, 100 + k) > 0.5 ? 0.0 : kPiD);
        }
    }

    // 32 vowel colours scattered through the vowel space.
    void seqVocal(double t, Harmonics& h)
    {
        const int f = frameOf(t);
        const double f1 = 250.0 + 650.0 * hash01(f, 11);
        const double f2 = 700.0 + 1700.0 * hash01(f, 12);
        const double f3 = 2300.0 + 900.0 * hash01(f, 13);
        for (int k = 1; k <= 140; ++k)
        {
            const double fr = 130.0 * k;
            addPartial(h, k, (formant(fr, f1, 100, 1.0) + formant(fr, f2, 140, 0.55) + formant(fr, f3, 200, 0.3) + 0.02) / std::sqrt((double) k), kPiD * 0.5);
        }
    }

    // 32 bell / metal spectra (sparse, high partial sets).
    void seqBells(double t, Harmonics& h)
    {
        const int f = frameOf(t);
        addPartial(h, 1, 0.7);
        const int n = 3 + (int) (hash01(f, 21) * 5.0);
        for (int i = 0; i < n; ++i)
        {
            const int k = 2 + (int) (hash01(f, 30 + i) * 30.0);
            addPartial(h, k, 0.25 + 0.7 * hash01(f, 50 + i), hash01(f, 70 + i) * 2.0 * kPiD);
        }
    }

    // Spectral waves (in the manner of the Virus's): a bright saw spectrum
    // with a resonant band that walks up the harmonics frame by frame.
    void spectral(double t, Harmonics& h)
    {
        const double centre = 1.0 + t * 40.0;
        const double width = 1.0 + t * 4.0;
        for (int k = 1; k <= 256; ++k)
            addPartial(h, k, (0.35 / k) + formant((double) k, centre, width, 1.0) / std::sqrt((double) k));
    }

    // ============================================================ ROM waves
    // Frames are moments in time: the first ones form the attack, the later
    // ones a slowly moving loop (see romEvolution). Partials whose phase
    // drifts from frame to frame make the loop shimmer like a multi-voice
    // sample.
    double drift(int k, double t, double rate, int seed) { return 2.0 * kPiD * (hash01(k, seed) + t * rate * (0.5 + hash01(k, seed + 1))); }

    void vowelRom(double t, Harmonics& h, double f1, double f2, double f3, double breath)
    {
        const double att = std::max(0.0, 1.0 - t * 4.0);    // brightness of the first frames
        const double wob = std::sin(2.0 * kPiD * t * 1.5);  // vowel wobble in the loop
        for (int k = 1; k <= 120; ++k)
        {
            const double fr = 220.0 * k;
            double a = formant(fr, f1 * (1.0 + 0.04 * wob), 110, 1.0) + formant(fr, f2 * (1.0 - 0.05 * wob), 150, 0.5)
                     + formant(fr, f3, 220, 0.25) + breath * (0.6 * att + 0.15) / (1.0 + 0.02 * k);
            addPartial(h, k, a / std::sqrt((double) k), drift(k, t, 3.0, 7));
        }
    }
    void romAhhChoir(double t, Harmonics& h)  { vowelRom(t, h, 730, 1100, 2550, 0.05); }
    void romOohVoices(double t, Harmonics& h) { vowelRom(t, h, 330, 800, 2300, 0.03); }
    void romAirVox(double t, Harmonics& h)    { vowelRom(t, h, 600, 1400, 2900, 0.18); }

    void romGlassPad(double t, Harmonics& h)
    {
        static const int ks[] = { 1, 2, 3, 5, 8, 11, 13, 17, 23, 29 };
        for (int i = 0; i < 10; ++i)
        {
            const int k = ks[i];
            const double attack = std::exp(-t * 6.0 * (i + 1) / 4.0);
            const double sweep = 0.5 + 0.5 * std::sin(2.0 * kPiD * (t * 1.2 + i * 0.13));
            addPartial(h, k, (i == 0 ? 1.0 : 0.5 / std::sqrt((double) i)) * (0.4 * attack + 0.6 * sweep), drift(k, t, 2.0, 9));
        }
    }

    void romDigitalBell(double t, Harmonics& h)
    {
        static const int ks[] = { 1, 3, 7, 11, 14, 19, 26, 34 };
        for (int i = 0; i < 8; ++i)
            addPartial(h, ks[i], (i == 0 ? 1.0 : 0.8 / (0.6 * i + 0.4)) * std::exp(-t * 0.7 * ks[i]), 0.3 * i);
    }

    void romSpectrum(double t, Harmonics& h)
    {
        const double c = 0.18 + 0.12 * std::sin(2.0 * kPiD * t);
        const double bright = std::exp(-t * 3.0);
        for (int k = 1; k <= 200; ++k)
            addPartial(h, k, std::abs(std::cos(kPiD * k * c)) / std::pow((double) k, 0.9 - 0.3 * bright));
    }

    void romDigiPad(double t, Harmonics& h)
    {
        const int shift = (int) (t * 6.0);
        for (int k = 1; k <= 48; ++k)
        {
            if ((k + shift) % 4 == 0) continue;
            addPartial(h, k, 1.0 / std::pow((double) k, 0.75), drift(k, t, 2.0, 13));
        }
    }

    void romStrings(double t, Harmonics& h)
    {
        const double scratch = std::max(0.0, 1.0 - t * 5.0);
        for (int k = 1; k <= 160; ++k)
        {
            const double fr = 220.0 * k;
            const double body = 0.4 + formant(fr, 500, 200, 0.8) + formant(fr, 1500, 400, 0.6) + formant(fr, 3200, 700, 0.4);
            const double s = scratch * hash01(k, 31) * (k > 12 ? 1.5 : 0.0);
            addPartial(h, k, (body + s) / k, drift(k, t, 4.0, 17));
        }
    }

    void romEPiano(double t, Harmonics& h)
    {
        addPartial(h, 1, 1.0);
        addPartial(h, 2, 0.35 * std::exp(-t * 3.0) + 0.06);
        addPartial(h, 3, 0.2 * std::exp(-t * 5.0));
        addPartial(h, 4, 0.12 * std::exp(-t * 6.0));
        addPartial(h, 7, 0.5 * std::exp(-t * 12.0)); // the tine "ping"
        addPartial(h, 15, 0.2 * std::exp(-t * 20.0));
    }

    void romBrass(double t, Harmonics& h)
    {
        // Dark start, quick blat to bright, then settles.
        const double bright = t < 0.3 ? t / 0.3 : 1.0 - 0.35 * std::min(1.0, (t - 0.3) / 0.3);
        const double count = 3.0 + 40.0 * bright;
        for (int k = 1; k <= 64; ++k)
        {
            const double w = std::clamp(count - (double) k + 1.0, 0.0, 1.0);
            addPartial(h, k, w * (1.0 + formant((double) k * 220.0, 1200, 500, 0.8)) / k, drift(k, t, 1.0, 19));
        }
    }

    void romMallet(double t, Harmonics& h)
    {
        addPartial(h, 1, 1.0);
        addPartial(h, 4, 0.6 * std::exp(-t * 8.0));
        addPartial(h, 10, 0.35 * std::exp(-t * 16.0));
        addPartial(h, 3, 0.08);
    }

    void romPluck(double t, Harmonics& h)
    {
        for (int k = 1; k <= 120; ++k)
            addPartial(h, k, std::exp(-t * 0.25 * k * (1.0 + 0.02 * k)) / k);
    }

    void romHeaven(double t, Harmonics& h)
    {
        addPartial(h, 1, 1.0);
        addPartial(h, 2, 0.3);
        for (int k = 6; k <= 32; ++k)
        {
            const double m = 0.5 + 0.5 * std::sin(2.0 * kPiD * (t * 2.0 + k * 0.071));
            addPartial(h, k, 0.25 * m * m / std::sqrt((double) k - 4.0), drift(k, t, 2.5, 23));
        }
    }

    void romMetal(double t, Harmonics& h)
    {
        static const int ks[] = { 1, 5, 9, 13, 17, 23, 31, 41 };
        for (int i = 0; i < 8; ++i)
        {
            const double m = 0.6 + 0.4 * std::sin(2.0 * kPiD * (t * 1.5 + i * 0.21));
            addPartial(h, ks[i], (i == 0 ? 0.8 : 0.6) * m * (i == 0 ? 1.0 : std::exp(-t * 0.5 * i) * 0.6 + 0.4), drift(ks[i], t, 3.0, 29));
        }
    }

    void romBowedGlass(double t, Harmonics& h)
    {
        addPartial(h, 1, 1.0);
        const double centre = 6.0 + 18.0 * (0.5 + 0.5 * std::sin(2.0 * kPiD * t));
        for (int k = 2; k <= 48; ++k)
            addPartial(h, k, 0.45 * formant((double) k, centre, 2.0, 1.0) + 0.02 / k, drift(k, t, 1.5, 37));
    }

    void romSynthPad(double t, Harmonics& h)
    {
        for (int k = 1; k <= 200; ++k)
        {
            // Two saws a little apart in "phase", drifting: a PWM-like pad.
            const double pw = 0.5 + 0.3 * std::sin(2.0 * kPiD * t);
            const double a = (1.0 - std::cos(2.0 * kPiD * k * pw)) * 0.5;
            addPartial(h, k, (0.5 + 0.5 * a) / k, drift(k, t, 1.0, 41));
        }
    }

    // ===================================================== ambient tables
    // Made to be scanned slowly: neighbouring frames are close, and partial
    // phases drift from frame to frame, so a moving position shimmers like an
    // ensemble instead of just changing colour.
    double smooth01(double x) { x = std::clamp(x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }
    double lfoAt(double t, double rate, double phase) { return 0.5 + 0.5 * std::sin(2.0 * kPiD * (t * rate + phase)); }
    double gaussLog(int k, double centre, double octaves)
    {
        const double d = (std::log2((double) k) - std::log2(std::max(1.0, centre))) / octaves;
        return std::exp(-d * d);
    }
    /** Smooth value noise over t: n keyframes per table, cosine-interpolated. */
    double valueNoise(double t, int keys, int a, int b)
    {
        const double x = t * keys;
        const int i = std::min(keys - 1, (int) x);
        const double f = 0.5 - 0.5 * std::cos(kPiD * (x - i));
        return hash01(a * 131 + i, b) * (1.0 - f) + hash01(a * 131 + i + 1, b) * f;
    }

    // Aurora: a soft saw bed with three bands of "light" travelling through
    // the harmonics at different speeds.
    void aurora(double t, Harmonics& h)
    {
        for (int k = 1; k <= 96; ++k)
        {
            double a = 0.22 / std::pow((double) k, 1.15);
            for (int j = 0; j < 3; ++j)
            {
                const double c = std::pow(2.0, 1.0 + 4.2 * lfoAt(t, 0.6 + 0.45 * j, 0.17 * j + 0.1));
                a += (0.9 - 0.2 * j) * gaussLog(k, c, 0.28) / std::sqrt((double) k);
            }
            addPartial(h, k, a, drift(k, t, 2.5, 101));
        }
    }

    // Nebula: smooth random spectra (value noise per harmonic), a slowly
    // changing cloud of partials.
    void nebula(double t, Harmonics& h)
    {
        addPartial(h, 1, 0.6);
        for (int k = 2; k <= 110; ++k)
        {
            const double n = valueNoise(t, 5, k, 211);
            addPartial(h, k, std::pow(n, 2.2) * 1.4 / std::pow((double) k, 0.75), drift(k, t, 3.0, 223));
        }
    }

    // Bloom: from a pure sine, harmonics open one by one into a bright pad.
    void bloom(double t, Harmonics& h)
    {
        const double count = 1.0 + 63.0 * std::pow(t, 1.6);
        for (int k = 1; k <= 64; ++k)
        {
            const double w = smooth01(count - (double) k + 1.0);
            addPartial(h, k, w / std::pow((double) k, 0.85), k == 1 ? 0.0 : drift(k, t, 1.5, 307));
        }
    }

    // Shimmer: octaves and fifths with the energy climbing five octaves.
    void shimmer(double t, Harmonics& h)
    {
        static const int ks[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 };
        const double centre = std::pow(2.0, 0.3 + 5.4 * t);
        for (int i = 0; i < 12; ++i)
        {
            const int k = ks[i];
            const double a = (k == 1 ? 0.55 : 0.0) + gaussLog(k, centre, 0.9) * (k == 1 ? 0.4 : 0.8 / std::pow((double) k, 0.2));
            addPartial(h, k, a, drift(k, t, 2.0, 401));
        }
    }

    // Tidal: a saw through a comb whose spacing sweeps (a phaser frozen in
    // each frame; scanning it is the sweep).
    void tidal(double t, Harmonics& h)
    {
        const double d = 0.012 + 0.2 * t * t;
        for (int k = 1; k <= 160; ++k)
        {
            const double comb = 0.5 + 0.5 * std::cos(2.0 * kPiD * k * d);
            addPartial(h, k, (0.08 + comb * comb) / std::pow((double) k, 0.9), drift(k, t, 1.0, 503));
        }
    }

    // Drift Pad: three pulse waves with widths moving at different rates, the
    // spectrum of an ensemble of PWM oscillators.
    void driftPad(double t, Harmonics& h)
    {
        const double w[3] = { 0.5 + 0.42 * std::sin(2.0 * kPiD * t), 0.5 + 0.38 * std::sin(2.0 * kPiD * (t * 1.7 + 0.3)),
                              0.5 + 0.3 * std::sin(2.0 * kPiD * (t * 2.6 + 0.6)) };
        for (int k = 1; k <= 200; ++k)
        {
            double a = 0.0, b = 0.0;
            for (int j = 0; j < 3; ++j)
            {
                // A pulse of width w: (2 / (pi k)) sin(pi k w), with its phase.
                const double amp = 2.0 / (kPiD * k) * std::sin(kPiD * k * w[j]);
                const double phi = -kPiD * k * w[j] + 0.9 * j;
                a += amp * std::cos(phi);
                b += amp * std::sin(phi);
            }
            h.a[(size_t) k] += a / 3.0;
            h.b[(size_t) k] += b / 3.0;
        }
    }

    // Dark Matter: weight in the low harmonics, a dark formant moving through
    // them, odd harmonics growing (hollow) towards the end.
    void darkMatter(double t, Harmonics& h)
    {
        const double centre = 2.0 + 5.0 * lfoAt(t, 1.0, 0.75);
        for (int k = 1; k <= 40; ++k)
        {
            const double odd = (k % 2 == 1) ? 1.0 : 1.0 - 0.7 * t;
            addPartial(h, k, odd * (1.0 / ((double) k * k) + 0.5 * gaussLog(k, centre, 0.4) / k), drift(k, t, 1.2, 601));
        }
    }

    // Ether: a pure fundamental with a breathing band of quiet high partials
    // (air), the band moving and thinning out.
    void ether(double t, Harmonics& h)
    {
        addPartial(h, 1, 1.0);
        addPartial(h, 2, 0.12);
        const double centre = 14.0 + 22.0 * lfoAt(t, 0.8, 0.0);
        for (int k = 6; k <= 64; ++k)
        {
            const double n = 0.3 + 0.7 * valueNoise(t, 7, k, 701);
            addPartial(h, k, 0.16 * n * gaussLog(k, centre, 0.45), hash01(k, 703) * 2.0 * kPiD + drift(k, t, 4.0, 707));
        }
    }

    // ------------------------------------------------------------- voices
    double vowelAmp(double f, double f1, double f2, double f3, double f4)
    {
        return formant(f, f1, 90, 1.0) + formant(f, f2, 130, 0.55) + formant(f, f3, 190, 0.3) + formant(f, f4, 260, 0.15);
    }

    // Choir Morph: a choir through A - E - I - O - U - A, with choral drift.
    void choirMorph(double t, Harmonics& h)
    {
        struct V { double f1, f2, f3, f4; };
        static const V v[] = { { 750, 1150, 2800, 3500 }, { 450, 1900, 2600, 3400 }, { 300, 2250, 3000, 3600 },
                               { 450, 800, 2800, 3300 }, { 330, 700, 2600, 3200 }, { 750, 1150, 2800, 3500 } };
        const double pos = t * 5.0;
        const int i = std::min(4, (int) pos);
        const double fr = smooth01(pos - i);
        auto L = [&](double a, double b) { return a + (b - a) * fr; };
        const V c { L(v[i].f1, v[i + 1].f1), L(v[i].f2, v[i + 1].f2), L(v[i].f3, v[i + 1].f3), L(v[i].f4, v[i + 1].f4) };
        for (int k = 1; k <= 140; ++k)
        {
            const double f = 165.0 * k;
            addPartial(h, k, (vowelAmp(f, c.f1, c.f2, c.f3, c.f4) + 0.015) / std::sqrt((double) k), drift(k, t, 4.0, 809));
        }
    }

    // Overtone Singer: a drone with one very narrow formant that walks up the
    // harmonics (4th to 16th): throat singing, the melody is the scan.
    void overtoneSinger(double t, Harmonics& h)
    {
        const double centre = 4.0 + 12.0 * t;
        for (int k = 1; k <= 48; ++k)
        {
            const double d = ((double) k - centre) / 0.55;
            const double low = formant(110.0 * k, 450, 120, 0.5);
            addPartial(h, k, (0.04 + low + 1.1 * std::exp(-d * d)) / std::sqrt((double) k), drift(k, t, 1.0, 811));
        }
    }

    // Whisper: breath shaped by moving vowel formants, the partials' phases
    // scattered frame to frame (a noise-like, airy voice).
    void whisper(double t, Harmonics& h)
    {
        const int f = frameOf(t);
        const double f1 = 350.0 + 400.0 * lfoAt(t, 1.0, 0.2), f2 = 900.0 + 1300.0 * lfoAt(t, 0.7, 0.6);
        addPartial(h, 1, 0.15);
        for (int k = 2; k <= 220; ++k)
        {
            const double fr = 110.0 * k;
            const double a = (vowelAmp(fr, f1, f2, 2700, 3600) * 0.9 + 0.05) * (0.6 + 0.4 * hash01(k, 900 + f));
            addPartial(h, k, a / std::pow((double) k, 0.35), hash01(k, 950 + f) * 2.0 * kPiD);
        }
    }

    // Formant Glide: two formants crossing (one rising, one falling) over a
    // bright source.
    void formantGlide(double t, Harmonics& h)
    {
        const double up = 250.0 * std::pow(14.0, t), down = 3500.0 * std::pow(1.0 / 9.0, t);
        for (int k = 1; k <= 160; ++k)
        {
            const double f = 110.0 * k;
            const double a = 0.05 + formant(f, up, 70 + 0.08 * up, 1.0) + formant(f, down, 90 + 0.08 * down, 0.7);
            addPartial(h, k, a / std::sqrt((double) k), drift(k, t, 1.5, 1009));
        }
    }

    // ------------------------------------------------------ bells & glass
    // Crystal: sparse high partials twinkling at their own rates.
    void crystal(double t, Harmonics& h)
    {
        static const int ks[] = { 1, 5, 9, 14, 21, 27, 33, 42, 51, 63 };
        for (int i = 0; i < 10; ++i)
        {
            const double tw = std::pow(lfoAt(t, 1.0 + 0.37 * i, hash01(i, 1101)), 3.0);
            addPartial(h, ks[i], i == 0 ? 0.8 : (0.15 + 0.6 * tw) / std::pow(1.0 + 0.25 * i, 0.8), drift(ks[i], t, 2.0, 1103));
        }
    }

    // Singing Bowl: bowl partials (about 1 : 2.7 : 5.2 : 8.2 : 11.5) on a high
    // harmonic grid, from the strike (bright) to the hum, with beating.
    void singingBowl(double t, Harmonics& h)
    {
        static const int ks[] = { 4, 11, 21, 33, 46, 61 };
        addPartial(h, 1, 0.08);
        for (int i = 0; i < 6; ++i)
        {
            const double decay = std::exp(-t * 1.5 * i);
            const double beat = 0.75 + 0.25 * std::sin(2.0 * kPiD * t * (2.0 + i));
            addPartial(h, ks[i], (i == 0 ? 1.0 : 0.7 / (0.5 + i)) * (0.2 + 0.8 * decay) * beat, drift(ks[i], t, 1.0, 1201));
        }
    }

    // Bell Garden: four bell spectra cross-fading into each other.
    void bellGarden(double t, Harmonics& h)
    {
        const double pos = t * 3.0;
        const int i = std::min(2, (int) pos);
        const double fr = smooth01(pos - i);
        addPartial(h, 1, 0.6);
        for (int b = 0; b < 2; ++b)
        {
            const int set = i + b;
            const double w = b == 0 ? 1.0 - fr : fr;
            for (int j = 0; j < 7; ++j)
            {
                const int k = 2 + (int) (hash01(set, 1300 + j) * 46.0);
                addPartial(h, k, w * (0.2 + 0.6 * hash01(set, 1320 + j)) / (1.0 + 0.3 * j), hash01(set, 1340 + j) * 2.0 * kPiD);
            }
        }
    }

    // Glass Harmonica: almost a sine, with a soft bright band moving across
    // harmonics 5 - 12.
    void glassHarmonica(double t, Harmonics& h)
    {
        addPartial(h, 1, 1.0);
        addPartial(h, 2, 0.1 + 0.08 * t);
        addPartial(h, 3, 0.05);
        const double centre = 5.0 + 7.0 * lfoAt(t, 1.0, 0.75);
        for (int k = 4; k <= 16; ++k)
            addPartial(h, k, 0.3 * gaussLog(k, centre, 0.18), drift(k, t, 1.0, 1401));
    }

    // ------------------------------------------------------------- organic
    // Breath Flute: a flute tone with breath, overblowing (2nd harmonic) late.
    void breathFlute(double t, Harmonics& h)
    {
        const int f = frameOf(t);
        const double breath = 0.5 * (1.0 - t) + 0.08;
        addPartial(h, 1, 1.0 - 0.45 * smooth01((t - 0.6) / 0.4));
        addPartial(h, 2, 0.12 + 0.75 * smooth01((t - 0.6) / 0.4));
        addPartial(h, 3, 0.1);
        addPartial(h, 4, 0.05);
        for (int k = 5; k <= 120; ++k)
            addPartial(h, k, breath * 0.25 * (0.4 + 0.6 * hash01(k, 1500 + f)) / std::pow((double) k, 0.6), hash01(k, 1550 + f) * 2.0 * kPiD);
    }

    // Bowed Strings: bow pressure from flautando (few harmonics) to a hard,
    // rich bow with body resonances.
    void bowedStrings(double t, Harmonics& h)
    {
        const double count = 4.0 + 120.0 * t * t;
        for (int k = 1; k <= 140; ++k)
        {
            const double fr = 196.0 * k;
            const double body = 0.5 + formant(fr, 280, 80, 0.5) + formant(fr, 500, 150, 0.6) + formant(fr, 2500, 900, 0.9);
            const double w = smooth01((count - k) / 6.0 + 1.0);
            addPartial(h, k, w * body / k, drift(k, t, 3.0, 1601));
        }
    }

    // Reed: clarinet (odd harmonics) to oboe (all harmonics with a 1.2 kHz formant).
    void reed(double t, Harmonics& h)
    {
        for (int k = 1; k <= 80; ++k)
        {
            const double clar = (k % 2 == 1) ? 1.0 / k : 0.02 / k;
            const double oboe = (0.25 + formant(220.0 * k, 1200, 400, 1.2) + formant(220.0 * k, 2900, 500, 0.4)) / std::sqrt((double) k);
            addPartial(h, k, clar * (1.0 - t) + oboe * t * 0.8, drift(k, t, 1.0, 1701));
        }
    }

    // Cathedral Organ: pipe stops drawn in turn: principal, octave, fifteenth,
    // then the mixture.
    void cathedralOrgan(double t, Harmonics& h)
    {
        struct Stop { int k; double start, level; };
        static const Stop stops[] = { { 1, 0.0, 1.0 }, { 2, 0.15, 0.6 }, { 3, 0.35, 0.3 }, { 4, 0.3, 0.45 }, { 6, 0.55, 0.3 },
                                      { 8, 0.5, 0.35 }, { 12, 0.7, 0.25 }, { 16, 0.7, 0.25 }, { 24, 0.85, 0.18 }, { 32, 0.85, 0.15 } };
        for (const auto& s : stops)
        {
            // Pipes are rich in their own right: a few weak harmonics of each stop.
            const double w = smooth01((t - s.start) / 0.12 + (s.start == 0.0 ? 1.0 : 0.0)) * s.level;
            addPartial(h, s.k, w);
            addPartial(h, 2 * s.k, w * 0.12);
            addPartial(h, 3 * s.k, w * 0.05);
        }
    }

    // ------------------------------------------------------------- digital
    // Stepped Digital: an 8-bit era wave, sample-and-hold steps and amplitude
    // steps getting coarser through the table.
    double steppedDigital(double p, double t)
    {
        const double steps = std::round(64.0 * std::pow(0.125, t)); // 64 .. 8 samples per cycle
        const double levels = std::round(2.0 + 30.0 * (1.0 - t));    // 32 .. 2 levels
        const double q = std::floor(p * steps) / steps;
        const double x = std::sin(2.0 * kPiD * q) + 0.5 * std::sin(4.0 * kPiD * q + 1.0) * t;
        return std::round(x / 1.5 * levels) / levels;
    }

    // Resonant PD: phase-distortion resonance (CZ style) sweeping up.
    double resonantPd(double p, double t)
    {
        const double ratio = 1.0 + 15.0 * t * t;
        return (1.0 - p) * std::cos(2.0 * kPiD * ratio * p) - (1.0 - p) * 0.5;
    }

    // FM Pad: two-operator FM, ratio 1:2 to 1:3, the index rising gently.
    double fmPad(double p, double t)
    {
        const double index = 2.4 * t;
        const double r2 = std::sin(2.0 * kPiD * 2.0 * p), r3 = std::sin(2.0 * kPiD * 3.0 * p);
        const double mix = smooth01((t - 0.4) / 0.6);
        return std::sin(2.0 * kPiD * p + index * ((1.0 - mix) * r2 + mix * r3));
    }

    // Prism: odd harmonics -> even -> all, the brightness tilting as it goes.
    void prism(double t, Harmonics& h)
    {
        const double odd = 1.0 - 0.85 * smooth01(t * 2.0), even = smooth01(t * 2.0) * (1.0 - 0.0 * t);
        const double tilt = 1.3 - 0.6 * t;
        for (int k = 1; k <= 128; ++k)
        {
            const double w = (k % 2 == 1) ? odd + 0.6 * smooth01((t - 0.5) * 2.0) : even;
            addPartial(h, k, (k == 1 ? 1.0 : w) / std::pow((double) k, tilt), drift(k, t, 1.0, 1901));
        }
    }
} // namespace

int Wavetable::mipForIncrement(float inc)
{
    if (inc <= 0.0f)
        return 0;
    const float maxHarmonic = 0.5f / inc;
    int top = kMaxHarmonics;
    int mip = 0;
    while (mip < kNumMips - 1 && (float) top > maxHarmonic)
    {
        top >>= 1;
        ++mip;
    }
    return mip;
}

WavetableBank::WavetableBank()
{
    // Every table is independent: build them on a few threads.
    using Job = std::function<Wavetable()>;
    auto spec = [](SpectralFn f) { return Job([f] { return buildSpectral(f); }); };
    auto time = [](TimeFn f) { return Job([f] { return buildTimeDomain(f); }); };
    std::vector<Job> jobs = {
        spec(analogMorph), spec(harmonicSweep), spec(vocalFormant), time(pwmScan), time(digitalBell), time(syncSweep),
        time(wavefolder), spec(drawbarOrgan), spec(seqHarmonics), spec(seqDigital), spec(seqVocal), spec(seqBells), spec(spectral),
        spec(aurora), spec(nebula), spec(bloom), spec(shimmer), spec(tidal), spec(driftPad), spec(darkMatter), spec(ether),
        spec(choirMorph), spec(overtoneSinger), spec(whisper), spec(formantGlide),
        spec(crystal), spec(singingBowl), spec(bellGarden), spec(glassHarmonica),
        spec(breathFlute), spec(bowedStrings), spec(reed), spec(cathedralOrgan),
        time(steppedDigital), time(resonantPd), time(fmPad), spec(prism),
    };
    if ((int) jobs.size() != kNumGeneratedTables)
        std::abort(); // one generator per WavetableId

    // ROM waves, in OscWave order (they are also tables RomFirst ..).
    const SpectralFn romFns[] = { romAhhChoir, romOohVoices, romGlassPad, romDigitalBell, romSpectrum, romDigiPad,
                                  romStrings, romEPiano, romBrass, romMallet, romPluck, romHeaven, romMetal,
                                  romBowedGlass, romAirVox, romSynthPad };
    static_assert(sizeof(romFns) / sizeof(romFns[0]) == (size_t) kNumRomWaves, "one generator per ROM wave");
    for (const auto& fn : romFns)
        jobs.push_back(spec(fn));

    std::vector<Wavetable> built(jobs.size());
    std::atomic<size_t> next { 0 };
    auto worker = [&] {
        for (size_t j; (j = next.fetch_add(1)) < jobs.size();)
            built[j] = jobs[j]();
    };
    const unsigned n = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
    std::vector<std::thread> threads;
    for (unsigned i = 1; i < n; ++i)
        threads.emplace_back(worker);
    worker();
    for (auto& th : threads)
        th.join();

    tables.assign(std::make_move_iterator(built.begin()), std::make_move_iterator(built.begin() + kNumGeneratedTables));
    roms.assign(std::make_move_iterator(built.begin() + kNumGeneratedTables), std::make_move_iterator(built.end()));
}

RomEvolution WavetableBank::romEvolution(int i)
{
    // attack (s), loop start, loop end (frames), loop period (s)
    static const RomEvolution e[] = {
        { 0.18f, 6.0f, 31.0f, 3.2f },   // Ahh Choir
        { 0.20f, 6.0f, 31.0f, 3.6f },   // Ooh Voices
        { 0.45f, 10.0f, 31.0f, 5.0f },  // Glass Pad
        { 1.40f, 26.0f, 31.0f, 6.0f },  // Digital Bell
        { 0.30f, 8.0f, 31.0f, 4.0f },   // Spectrum
        { 0.25f, 4.0f, 31.0f, 5.5f },   // Digi Pad
        { 0.22f, 7.0f, 31.0f, 3.0f },   // Strings
        { 0.90f, 27.0f, 31.0f, 4.0f },  // E.Piano
        { 0.35f, 18.0f, 31.0f, 4.5f },  // Brass
        { 0.50f, 28.0f, 31.0f, 4.0f },  // Mallet
        { 1.60f, 30.0f, 31.0f, 4.0f },  // Pluck
        { 0.50f, 4.0f, 31.0f, 7.0f },   // Heaven
        { 0.30f, 6.0f, 31.0f, 3.5f },   // Metal
        { 0.60f, 2.0f, 31.0f, 8.0f },   // Bowed Glass
        { 0.15f, 4.0f, 31.0f, 2.4f },   // Air Vox
        { 0.20f, 4.0f, 31.0f, 4.5f },   // Synth Pad
    };
    return e[std::clamp(i, 0, kNumRomWaves - 1)];
}

const Wavetable& WavetableBank::rom(int i) const
{
    return roms[(size_t) std::clamp(i, 0, (int) roms.size() - 1)];
}

const WavetableBank& WavetableBank::get()
{
    static const WavetableBank bank;
    return bank;
}

const Wavetable& WavetableBank::table(int id) const
{
    id = std::clamp(id, 0, numTables() - 1);
    return id < (int) tables.size() ? tables[(size_t) id] : roms[(size_t) id - tables.size()];
}

} // namespace zsynth::dsp
