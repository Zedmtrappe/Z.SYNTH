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

    // ---- harmoniser
    if (inRange(i, P::HarmKey, P::HarmPart))
    {
        if (! on(v, P::HarmOn))
            return inactive("HARMONY is off. Switch it on (under PITCH TRACK).");
        const int voices = choice(v, P::HarmVoices);
        if ((i == P::HarmV2Int || i == P::HarmV2Dir) && voices < 2)
            return inactive("VOICES is 1: voice 2 is not used.");
        if ((i == P::HarmV3Int || i == P::HarmV3Dir) && voices < 3)
            return inactive("VOICES is below 3: voice 3 is not used.");
        if ((i == P::HarmKey || i == P::HarmOutKey) && (HarmScale) choice(v, P::HarmScale) == HarmScale::Fixed)
            return inactive("FIXED intervals ignore the key.");
        if (i == P::HarmPart && (SplitMode) choice(v, P::SplitMode) == SplitMode::Single)
            return inactive("KEYBOARD is SINGLE: the harmony plays the one sound.");
    }

    // ---- keyboard split / layer
    if (inRange(i, P::SplitPoint, P::SplitUpVol))
    {
        const auto mode = (SplitMode) choice(v, P::SplitMode);
        if (mode == SplitMode::Single)
            return inactive("KEYBOARD is SINGLE: choose SPLIT or LAYER to play two sounds.");
        if (i == P::SplitPoint && mode == SplitMode::Layer)
            return inactive("LAYER plays both sounds on every key: the split point is for SPLIT.");
    }

    // ---- engine mode
    if (inRange(i, P::FmAlgo, P::Op6Kls) && engine == EngineMode::Analog)
        return inactive("The FM engine is off. Set ENGINE to HYBRID or FM.");

    if ((i == P::XfmrType || i == P::XfmrAmount || i == P::XfmrHot) && ! on(v, P::XfmrOn))
        return inactive("The output transformer is off. Switch it ON.");
    if (inRange(i, P::ValveType, P::ValveMix) && ! on(v, P::ValveOn))
        return inactive("The valve stage is off. Switch VALVE on.");

    if (i == P::PeqPre && ! on(v, P::PeqOn))
        return inactive("The output EQ is switched out. Switch it ON.");
    if (inRange(i, P::PeqHpf, P::PeqOutput) && ! on(v, P::PeqOn))
        return inactive("The output EQ is switched out. Switch it ON.");
    if ((i == P::EqModel || inRange(i, P::EqDrive, P::G560B10)) && ! on(v, P::PeqOn))
        return inactive("The output EQ is switched out. Switch it ON.");
    if (i == P::EqDrive && choice(v, P::EqModel) == (int) EqModel::Pristine)
        return inactive("PRISTINE is clean: DRIVE is for the classic hardware models.");
    if (inRange(i, P::TapeModelP, P::TapeOut) && ! on(v, P::TapeOn))
        return inactive("The tape machine is off. Switch TAPE on.");

    if (i == P::CompPre && ! on(v, P::CompOn))
        return inactive("The output compressor is off. Switch COMP on.");
    if (inRange(i, P::CompModel, P::CompColour))
    {
        if (! on(v, P::CompOn))
            return inactive("The output compressor is off. Switch COMP on.");
        const auto model = (CompModel) choice(v, P::CompModel);
        if (model == CompModel::LA2A && (i == P::CompAttack || i == P::CompRelease || i == P::CompAutoRel || i == P::CompKnee))
            return inactive("The LA-2A's optical cell sets its own attack and two-stage release.");
        if (model == CompModel::Fairchild && (i == P::CompRatio || i == P::CompKnee || i == P::CompAutoRel))
            return inactive("Variable-mu: the ratio rises smoothly with level.");
        if (model == CompModel::FET1176 && (i == P::CompKnee || i == P::CompAutoRel))
            return inactive("The 1176 has a fixed knee and manual release.");
        if (i == P::CompEmph && model != CompModel::LA2A)
            return inactive("HF emphasis is an LA-2A control.");
        if (i == P::CompRelease && model == CompModel::SSLBus && on(v, P::CompAutoRel))
            return inactive("AUTO release is on.");
    }

    const bool analogOff = engine == EngineMode::FM;
    if ((inRange(i, P::Osc1On, P::Osc3Xmod) || i == P::SubLevel || i == P::SubWave || inRange(i, P::WtTable, P::WtSpread)
         || inRange(i, P::Osc1WtTable, P::WtDriftRate)) && analogOff)
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
        if (i == P::WtTable)
        {
            bool follows = false;
            for (int n = 0; n < 3; ++n)
                follows = follows || (on(v, oscs[n].on) && (OscWave) choice(v, oscs[n].wave) == OscWave::Wavetable
                                      && choice(v, P::Osc1WtTable + n * (P::Osc2WtTable - P::Osc1WtTable)) == 0);
            if (! follows)
                return inactive("Every wavetable oscillator has its own TABLE (WAVETABLE ENGINE window).");
        }
        if (i == P::WtSpread)
        {
            const bool multi = (on(v, P::Osc2On) && (OscWave) choice(v, P::Osc2Wave) == OscWave::Wavetable)
                            || (on(v, P::Osc3On) && (OscWave) choice(v, P::Osc3Wave) == OscWave::Wavetable);
            if (! multi)
                return inactive("Spread offsets oscillators 2 and 3; neither uses the Wavetable wave.");
        }
    }

    // ---- wavetable engine: per oscillator
    {
        constexpr int stride = P::Osc2WtTable - P::Osc1WtTable;
        for (int n = 0; n < 3; ++n)
        {
            const int b = P::Osc1WtTable + n * stride;
            if (! inRange(i, b, b + stride - 1))
                continue;
            const auto wave = (OscWave) choice(v, oscs[n].wave);
            const bool table = wave == OscWave::Wavetable || isRomWave(wave);
            if (! on(v, oscs[n].on))
                return inactive("Oscillator " + juce::String(n + 1) + " is switched off.");
            if (! table)
                return inactive("Oscillator " + juce::String(n + 1) + " is not playing a table: set its WAVE to Wavetable or a ROM wave.");
            if (i == b && wave != OscWave::Wavetable)
                return inactive("ROM waves play their own table (choose Wavetable to pick one).");
            if (i == P::Osc1WtWarpAmt + n * stride && choice(v, P::Osc1WtWarp + n * stride) == 0)
                return inactive("Choose a WARP.");
            if ((i == P::Osc1WtDetune + n * stride || i == P::Osc1WtSpread + n * stride || i == P::Osc1WtWidth + n * stride)
                && choice(v, P::Osc1WtVoices + n * stride) <= 1)
                return inactive("STACK is 1: add copies to detune, spread and widen them.");
        }
    }
    if (inRange(i, P::WtEvoMode, P::WtDriftRate))
    {
        bool scans = false, tables = false;
        for (const auto& o : oscs)
        {
            const auto wave = (OscWave) choice(v, o.wave);
            scans = scans || (on(v, o.on) && wave == OscWave::Wavetable);
            tables = tables || (on(v, o.on) && (wave == OscWave::Wavetable || isRomWave(wave)));
        }
        if (i <= P::WtEvoTime && ! scans)
            return inactive("EVOLVE moves the Wavetable wave's position: set an oscillator's wave to Wavetable.");
        if (! tables)
            return inactive("DRIFT moves a table's position: set an oscillator's wave to Wavetable or a ROM wave.");
        if ((i == P::WtEvoAmt || i == P::WtEvoTime) && choice(v, P::WtEvoMode) == 0)
            return inactive("EVOLVE is off. Choose SWEEP or PING-PONG.");
        if (i == P::WtDriftRate && v[P::WtDrift] <= 0.0f)
            return inactive("DRIFT is zero.");
    }

    if (inRange(i, P::WtSeqMode, P::WtSeqXfade))
    {
        bool used = false;
        for (const auto& o : oscs)
            used = used || (on(v, o.on) && (OscWave) choice(v, o.wave) == OscWave::Wavetable);
        if (! used)
            return inactive("Wave sequencing plays the WAVETABLE: set an oscillator's wave to Wavetable.");
        if (i != P::WtSeqMode && choice(v, P::WtSeqMode) == 0)
            return inactive("The wave sequence is off. Choose a MODE.");
        if (i == P::WtSeqTime && on(v, P::WtSeqSync))
            return inactive("Steps are tempo-synced: use DIVISION.");
        if (i == P::WtSeqDiv && ! on(v, P::WtSeqSync))
            return inactive("Steps are free-running: use TIME (or switch SYNC on).");
    }
    if ((i == P::VecX || i == P::VecY) && ! on(v, P::VecOn))
        return inactive("Vector mixing is off. Switch VECTOR on.");

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
                             { P::DelayOn, P::DelaySync, P::DelayDuck, "Delay" },
                             { P::ReverbOn, P::ReverbSize, P::RevEqLpf, "Reverb" } };
    for (const auto& f : fx)
        if (inRange(i, f.first, f.last) && ! on(v, f.on))
            return inactive(juce::String(f.name) + " is switched off.");
    if (i == P::Smooth && ! on(v, P::SmoothOn))
        return inactive("SMOOTH is switched off.");
    if (i == P::ChorusType && ! on(v, P::ChorusOn))
        return inactive("Chorus is switched off.");
    if (i == P::DelayTime && on(v, P::DelaySync))
        return inactive("Delay is tempo-synced: use DIVISION.");
    if (i == P::DelayDiv && ! on(v, P::DelaySync))
        return inactive("Delay sync is off: use TIME.");
    if (inRange(i, P::DelayWow, P::DelayAge) && (DelayType) choice(v, P::DelayType) == DelayType::Digital)
        return inactive("The Digital delay is clean: wow, flutter and age shape the tape, analog, diffuse and lo-fi types.");

    return {};
}

} // namespace zsynth::ui
