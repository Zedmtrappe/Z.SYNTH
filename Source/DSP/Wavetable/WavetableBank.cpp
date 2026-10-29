#include "WavetableBank.h"

#include "../../Parameters/ParamChoices.h"
#include "../Common/FFT.h"

#include <algorithm>
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
