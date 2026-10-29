// Engine CPU benchmark: cost per voice-sample for representative configurations.
// Usage: ZSynthBench [seconds]

#include "TestHelpers.h"

#include <cstdio>
#include <cstdlib>
#include <functional>

using namespace zstest;

namespace
{
    void bench(const char* name, int voices, const std::function<void(EngineRig&)>& setup, double seconds)
    {
        EngineRig rig(48000.0, 256);
        rig.set(P::Polyphony, 32.0f);
        setup(rig);
        for (int n = 0; n < voices; ++n)
            rig.noteOn(0.0, 36 + n * 2);
        rig.render(0.1); // warm-up
        Stopwatch sw;
        rig.render(seconds);
        const double t = sw.seconds();
        const double internalRate = 48000.0 * rig.engine.getOversampling();
        std::printf("%-44s %2d voices: %5.1f%% core, %6.1f ns/voice-sample (internal rate)\n",
                    name, rig.engine.getActiveVoices(), 100.0 * t / seconds, 1.0e9 * t / (seconds * internalRate * voices));
    }
}

int main(int argc, char** argv)
{
    const double s = argc > 1 ? std::atof(argv[1]) : 2.0;
    bench("1 saw, ladder 24", 16, [](EngineRig&) {}, s);
    bench("1 saw, SEM", 16, [](EngineRig& r) { r.set(P::FltModel, 2.0f); }, s);
    bench("1 saw, OTA", 16, [](EngineRig& r) { r.set(P::FltModel, 1.0f); }, s);
    bench("1 saw, K35", 16, [](EngineRig& r) { r.set(P::FltModel, 3.0f); }, s);
    bench("3 osc + sub + noise, ladder", 16, [](EngineRig& r) {
        r.set(P::Osc2On, 1.0f); r.set(P::Osc3On, 1.0f); r.set(P::SubLevel, 0.5f); r.set(P::NoiseLevel, 0.2f); }, s);
    bench("3 wavetable osc, ladder", 16, [](EngineRig& r) {
        r.set(P::Osc2On, 1.0f); r.set(P::Osc3On, 1.0f);
        for (int p : { P::Osc1Wave, P::Osc2Wave, P::Osc3Wave }) r.set(p, (float) OscWave::Wavetable); }, s);
    bench("FM only (6 ops), ladder", 16, [](EngineRig& r) { r.set(P::Engine, (float) EngineMode::FM); }, s);
    bench("Hybrid 3 osc + FM, ladder", 16, [](EngineRig& r) {
        r.set(P::Engine, (float) EngineMode::Hybrid); r.set(P::Osc2On, 1.0f); r.set(P::Osc3On, 1.0f); }, s);
    bench("1 saw, Eco 1x", 16, [](EngineRig& r) { r.set(P::Oversampling, 1.0f); }, s);
    bench("1 saw + all effects", 16, [](EngineRig& r) {
        for (int p : { P::DriveOn, P::ChorusOn, P::EnsOn, P::PhaserOn, P::FlangerOn, P::DelayOn, P::ReverbOn }) r.set(p, 1.0f); }, s);
    return 0;
}
