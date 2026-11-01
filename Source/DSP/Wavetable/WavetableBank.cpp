#include "WavetableBank.h"

#include "../../Parameters/ParamChoices.h"
#include "../Common/FFT.h"

#include <algorithm>
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
        constexpr int N = Wavetable::kSize;
        std::vector<std::complex<double>> buf((size_t) N);
        std::vector<std::vector<float>> mips(Wavetable::kNumMips, std::vector<float>((size_t) N));

        for (int m = 0; m < Wavetable::kNumMips; ++m)
        {
            std::fill(buf.begin(), buf.end(), std::complex<double>());
            const int top = Wavetable::kMaxHarmonics >> m;
            for (int k = 1; k <= top && k < N / 2; ++k)
            {
                const std::complex<double> c(h.a[(size_t) k] * 0.5, -h.b[(size_t) k] * 0.5);
                buf[(size_t) k] = c;
                buf[(size_t) (N - k)] = std::conj(c);
            }
            fftInPlace(buf, true);
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
            float* dst = const_cast<float*>(wt.frame(frameIndex, m));
            for (int n = 0; n < N; ++n)
                dst[n] = mips[(size_t) m][(size_t) n] * scale;
            dst[N] = dst[0];
        }
    }

    Wavetable buildSpectral(const SpectralFn& fn)
    {
        Wavetable wt;
        wt.data.resize((size_t) Wavetable::kFrames * Wavetable::kNumMips * (Wavetable::kSize + 1));
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
        wt.data.resize((size_t) Wavetable::kFrames * Wavetable::kNumMips * (Wavetable::kSize + 1));
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
    tables.resize((size_t) WavetableId::Count);
    tables[(size_t) WavetableId::AnalogMorph]   = buildSpectral(analogMorph);
    tables[(size_t) WavetableId::HarmonicSweep] = buildSpectral(harmonicSweep);
    tables[(size_t) WavetableId::VocalFormant]  = buildSpectral(vocalFormant);
    tables[(size_t) WavetableId::PwmScan]       = buildTimeDomain(pwmScan);
    tables[(size_t) WavetableId::DigitalBell]   = buildTimeDomain(digitalBell);
    tables[(size_t) WavetableId::SyncSweep]     = buildTimeDomain(syncSweep);
    tables[(size_t) WavetableId::Wavefolder]    = buildTimeDomain(wavefolder);
    tables[(size_t) WavetableId::DrawbarOrgan]  = buildSpectral(drawbarOrgan);
    tables[(size_t) WavetableId::SeqHarmonics]  = buildSpectral(seqHarmonics);
    tables[(size_t) WavetableId::SeqDigital]    = buildSpectral(seqDigital);
    tables[(size_t) WavetableId::SeqVocal]      = buildSpectral(seqVocal);
    tables[(size_t) WavetableId::SeqBells]      = buildSpectral(seqBells);
    tables[(size_t) WavetableId::Spectral]      = buildSpectral(spectral);

    // ROM waves, in OscWave order.
    const SpectralFn romFns[] = { romAhhChoir, romOohVoices, romGlassPad, romDigitalBell, romSpectrum, romDigiPad,
                                  romStrings, romEPiano, romBrass, romMallet, romPluck, romHeaven, romMetal,
                                  romBowedGlass, romAirVox, romSynthPad };
    static_assert(sizeof(romFns) / sizeof(romFns[0]) == (size_t) kNumRomWaves, "one generator per ROM wave");
    for (const auto& fn : romFns)
        roms.push_back(buildSpectral(fn));
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
    return tables[(size_t) std::clamp(id, 0, (int) tables.size() - 1)];
}

} // namespace zsynth::dsp
