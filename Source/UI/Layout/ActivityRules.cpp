#include "ActivityRules.h"

namespace zsynth::ui
{

namespace
{
    int choice(const float* v, int i) { return (int) std::lround(v[i]); }
    bool on(const float* v, int i) { return v[i] >= 0.5f; }
    Activity inactive(const juce::String& why) { return { false, why }; }

    bool inRange(int i, int first, int last) { return i >= first && i <= last; }
}

Activity activityFor(int i, const float* v, bool hostTempo)
{
    const auto engine = (EngineMode) choice(v, P::Engine);

    // ---- engine mode
    if (inRange(i, P::FmAlgo, P::Op6R) && engine == EngineMode::Analog)
        return inactive("The FM engine is off. Set ENGINE to HYBRID or FM.");

    const bool analogOff = engine == EngineMode::FM;
    if ((inRange(i, P::Osc1On, P::Osc3Xmod) || i == P::SubLevel || i == P::SubWave || inRange(i, P::WtTable, P::WtSpread)) && analogOff)
        return inactive("Analog oscillators are off in FM engine mode. Set ENGINE to ANALOG or HYBRID.");

    // ---- per oscillator
    struct Osc { int on, first, last, wave, pw; };
    static const Osc oscs[3] = { { P::Osc1On, P::Osc1Wave, P::Osc1Fm, P::Osc1Wave, P::Osc1Pw },
                                 { P::Osc2On, P::Osc2Wave, P::Osc2Xmod, P::Osc2Wave, P::Osc2Pw },
                                 { P::Osc3On, P::Osc3Wave, P::Osc3Xmod, P::Osc3Wave, P::Osc3Pw } };
    for (int n = 0; n < 3; ++n)
    {
        const auto& o = oscs[n];
        if (inRange(i, o.first, o.last))
        {
            if (! on(v, o.on))
                return inactive("Oscillator " + juce::String(n + 1) + " is switched off.");
            if (i == o.pw && (OscWave) choice(v, o.wave) != OscWave::Pulse)
                return inactive("Pulse width only affects the Pulse wave.");
        }
    }

    if (inRange(i, P::WtTable, P::WtSpread))
    {
        bool used = false;
        for (const auto& o : oscs)
            used = used || (on(v, o.on) && (OscWave) choice(v, o.wave) == OscWave::Wavetable);
        if (! used)
            return inactive("No oscillator uses the Wavetable wave.");
        if (i == P::WtSpread)
        {
            const bool multi = (on(v, P::Osc2On) && (OscWave) choice(v, P::Osc2Wave) == OscWave::Wavetable)
                            || (on(v, P::Osc3On) && (OscWave) choice(v, P::Osc3Wave) == OscWave::Wavetable);
            if (! multi)
                return inactive("Spread offsets oscillators 2 and 3; neither uses the Wavetable wave.");
        }
    }

    if (i == P::SubWave && v[P::SubLevel] <= 0.0f)
        return inactive("Sub-oscillator level is zero.");
    if (i == P::NoiseType && v[P::NoiseLevel] <= 0.0f)
        return inactive("Noise level is zero.");

    // ---- filter
    if (i == P::FltSlope && (FilterModel) choice(v, P::FltModel) == FilterModel::K35)
        return inactive("The K35 model has a fixed slope (12 dB LP, resonant 6 dB HP).");
    if (i == P::FltEnvInvert && v[P::FltEnv] <= 0.0f)
        return inactive("Envelope amount is zero.");

    // ---- envelopes
    if (i == P::Env3Amt && choice(v, P::Env3Dest) == 0)
        return inactive("Choose a destination for envelope 3's direct route.");

    // ---- LFOs
    for (int l = 0; l < 2; ++l)
    {
        const int b = l * kLfoParamStride;
        if (i == P::Lfo1Rate + b && on(v, P::Lfo1Sync + b))
            return inactive("LFO is tempo-synced: use DIVISION.");
        if (i == P::Lfo1Div + b && ! on(v, P::Lfo1Sync + b))
            return inactive("LFO is free-running: use RATE (or switch SYNC on).");
        if (i == P::Lfo1Amount + b && choice(v, P::Lfo1Dest + b) == 0)
            return inactive("AMOUNT scales the direct route: choose a DEST (the LFO is still a matrix source).");
    }

    // ---- voice
    const auto mode = (VoiceMode) choice(v, P::VoiceMode);
    if (i == P::VoicePriority && mode == VoiceMode::Poly)
        return inactive("Note priority applies to Mono and Legato modes.");
    if (i == P::VoiceAlloc && mode != VoiceMode::Poly)
        return inactive("Voice allocation applies to Poly mode.");
    if ((i == P::UnisonVoices || i == P::UnisonDetune) && ! on(v, P::Unison))
        return inactive("Unison is off.");
    if (i == P::GlideMode && v[P::GlideTime] <= 0.0f)
        return inactive("Glide time is zero.");

    // ---- matrix
    for (int m = 0; m < kNumModSlots; ++m)
    {
        const int b = m * kModParamStride;
        if (i == P::Mod1Amt + b && (choice(v, P::Mod1Src + b) == 0 || choice(v, P::Mod1Dst + b) == 0))
            return inactive("Choose a source and a destination for this slot.");
    }

    // ---- performance
    const auto seqMode = (SeqMode) choice(v, P::SeqMode);
    if (inRange(i, P::ArpMode, P::ArpHold) && i != P::ArpSwing)
    {
        if (seqMode == SeqMode::Note)
            return inactive("The sequencer's NOTE mode is playing the notes; the arpeggiator is bypassed.");
        if (! on(v, P::ArpOn))
            return inactive("The arpeggiator is off.");
    }
    if (i == P::ArpOn && seqMode == SeqMode::Note)
        return inactive("The sequencer's NOTE mode is playing the notes; the arpeggiator is bypassed.");
    if (i == P::ArpSwing && ! on(v, P::ArpOn) && seqMode == SeqMode::Off)
        return inactive("Swing applies to the arpeggiator and the sequencer; both are off.");

    if (inRange(i, P::SeqDiv, P::SeqSlide16) && seqMode == SeqMode::Off)
        return inactive("The step sequencer is off. Choose MOD or NOTE.");
    if (seqMode == SeqMode::Mod)
    {
        if (i == P::SeqGate)
            return inactive("Gate length applies to NOTE mode.");
        if (i >= P::SeqVal1 && i <= P::SeqSlide16)
        {
            const int k = (i - P::SeqVal1) % kSeqParamStride;
            if (k == P::SeqGate1 - P::SeqVal1 || k == P::SeqAcc1 - P::SeqVal1)
                return inactive("Step gates and accents apply to NOTE mode.");
        }
    }
    if (i >= P::SeqVal1 && i <= P::SeqSlide16 && seqMode != SeqMode::Off)
    {
        const int step = (i - P::SeqVal1) / kSeqParamStride;
        if (step >= choice(v, P::SeqLength))
            return inactive("This step is beyond the pattern LENGTH.");
    }

    if (i == P::Tempo && hostTempo)
        return inactive("The host is providing the tempo.");

    // ---- effects
    struct Fx { int on, first, last; const char* name; };
    static const Fx fx[] = { { P::DriveOn, P::DriveType, P::DriveMix, "Drive" },
                             { P::ChorusOn, P::ChorusRate, P::ChorusMix, "Chorus" },
                             { P::EnsOn, P::EnsRate, P::EnsMix, "Ensemble" },
                             { P::PhaserOn, P::PhaserRate, P::PhaserMix, "Phaser" },
                             { P::FlangerOn, P::FlangerRate, P::FlangerMix, "Flanger" },
                             { P::DelayOn, P::DelaySync, P::DelayMix, "Delay" },
                             { P::ReverbOn, P::ReverbSize, P::ReverbMix, "Reverb" } };
    for (const auto& f : fx)
        if (inRange(i, f.first, f.last) && ! on(v, f.on))
            return inactive(juce::String(f.name) + " is switched off.");
    if (i == P::DelayTime && on(v, P::DelaySync))
        return inactive("Delay is tempo-synced: use DIVISION.");
    if (i == P::DelayDiv && ! on(v, P::DelaySync))
        return inactive("Delay sync is off: use TIME.");

    return {};
}

} // namespace zsynth::ui
