#include "SynthEngine.h"

#include <cstring>

namespace zsynth::dsp
{

namespace
{
    constexpr float kSmoothingTime = 0.015f; // seconds, for continuous parameters

    float snapRatio(float r)
    {
        if (r < 0.75f)
            return 0.5f;
        return std::round(r);
    }
}

SynthEngine::SynthEngine()
{
    // Build shared tables now, on the constructing (non-audio) thread.
    (void) WavetableBank::get();
    (void) SineTable::get();
    (void) HalfBandCoefficients::get();

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamDef& d = paramDef(i);
        isContinuous[i] = d.type == ParamType::Float;
        isLogParam[i] = d.type == ParamType::Float && d.scale == LOG;
    }
    fillDefaults(smoothed);
    for (auto& ch : playedNote)
        for (auto& n : ch)
            n = -1;
    globalLfo[0].setSeed(0x1111u);
    globalLfo[1].setSeed(0x2222u);
}

void SynthEngine::prepare(double sampleRate, int maxBlockSize)
{
    baseRate = sampleRate;
    maxBlock = std::max(1, maxBlockSize);
    busL.assign((size_t) maxBlock * 2 + 64, 0.0f);
    busR.assign((size_t) maxBlock * 2 + 64, 0.0f);
    fxTempL.assign((size_t) maxBlock + 16, 0.0f);
    fxTempR.assign((size_t) maxBlock + 16, 0.0f);

    arp.prepare(sampleRate);
    seq.prepare(sampleRate);

    const float fs = (float) sampleRate;
    drive.prepare(fs);
    phaser.prepare(fs);
    flanger.prepare(fs);
    chorus.prepare(fs);
    ensemble.prepare(fs);
    delay.prepare(fs);
    reverb.prepare(fs);
    output.prepare(fs);
    for (auto& f : fades)
        f = 0.0f;

    configureOversampling(sampleRate < 80000.0 ? 2 : 1);
    smoothedValid = false;
}

void SynthEngine::configureOversampling(int factor)
{
    osFactor = factor;
    internalRate = (float) (baseRate * factor);
    voices.prepare(internalRate);
    decimL.reset();
    decimR.reset();
    shared.sampleRate = internalRate;
    shared.invSampleRate = 1.0f / internalRate;
    for (auto& ch : playedNote)
        for (auto& n : ch)
            n = -1;
}

// -----------------------------------------------------------------------------
void SynthEngine::process(float* left, float* right, int numSamples,
                          const float* params, const MidiEvent* events, int numEvents,
                          const TransportInfo& transport)
{
    ScopedNoDenormals noDenormals;
    targets = params;

    if (! smoothedValid)
    {
        std::memcpy(smoothed, params, sizeof(float) * (size_t) kNumParams);
        smoothedValid = true;
    }

    if (panicRequested.exchange(false, std::memory_order_relaxed))
    {
        voices.killAll();
        arp.reset();
        seq.reset();
        drive.reset(); phaser.reset(); flanger.reset(); chorus.reset();
        ensemble.reset(); delay.reset(); reverb.reset();
        for (auto& ch : playedNote)
            for (auto& n : ch)
                n = -1;
        pitchBend = 0.0f;
    }

    // Oversampling: Auto = 2x below 80 kHz.
    const auto osMode = (OversamplingMode) (int) std::lround(params[P::Oversampling]);
    const int desiredOs = osMode == OversamplingMode::Eco ? 1
                        : osMode == OversamplingMode::High ? 2
                        : (baseRate < 80000.0 ? 2 : 1);
    if (desiredOs != osFactor)
        configureOversampling(desiredOs);

    // Large host blocks are processed in chunks that fit the prepared buffers.
    int offset = 0;
    int eventIndex = 0;
    while (offset < numSamples)
    {
        const int n = std::min(maxBlock, numSamples - offset);
        int firstEvent = eventIndex;
        while (eventIndex < numEvents && events[eventIndex].offset < offset + n)
            ++eventIndex;
        processChunk(left + offset, right + offset, n, events + firstEvent, eventIndex - firstEvent, offset, transport);
        offset += n;
    }
    // Events beyond the block (malformed input) are applied at the end.
    for (; eventIndex < numEvents; ++eventIndex)
        handleMidi(events[eventIndex]);
}

void SynthEngine::processChunk(float* left, float* right, int n, const MidiEvent* events, int numEvents,
                               int chunkOffset, const TransportInfo& transport)
{
    updateBlockSettings(transport);

    const int os = osFactor;
    std::fill(busL.begin(), busL.begin() + n * os, 0.0f);
    std::fill(busR.begin(), busR.begin() + n * os, 0.0f);

    const int controlBase = std::max(1, kControlBlock / os);
    int pos = 0;
    int ev = 0;
    while (pos < n)
    {
        while (ev < numEvents && events[ev].offset - chunkOffset <= pos)
            handleMidi(events[ev++]);

        generated.clear();
        if (route == Route::Arp)
            arp.fireDueEvents(generated);
        seq.fireDueEvents(generated);
        dispatchGenerated();

        if (seq.tickCount() != lastSeqTick)
        {
            lastSeqTick = seq.tickCount();
            sampleHoldValue = globalRng.bipolar();
        }

        int seg = std::min(n - pos, controlBase);
        if (ev < numEvents)
            seg = std::min(seg, std::max(1, events[ev].offset - chunkOffset - pos));
        if (route == Route::Arp)
            seg = std::min(seg, arp.samplesToNextEvent());
        seg = std::min(seg, seq.samplesToNextEvent());
        seg = std::max(1, seg);

        smoothParams((float) seg / (float) baseRate);
        computeShared(seg * os);
        voices.render(busL.data() + pos * os, busR.data() + pos * os, seg * os, shared);

        arp.advance(seg);
        seq.advance(seg);
        pos += seg;
    }
    while (ev < numEvents)
        handleMidi(events[ev++]);

    if (os == 2)
    {
        for (int i = 0; i < n; ++i)
        {
            left[i] = decimL.process(busL[(size_t) (2 * i)], busL[(size_t) (2 * i + 1)]);
            right[i] = decimR.process(busR[(size_t) (2 * i)], busR[(size_t) (2 * i + 1)]);
        }
    }
    else
    {
        std::copy(busL.begin(), busL.begin() + n, left);
        std::copy(busR.begin(), busR.begin() + n, right);
    }

    runEffects(left, right, n);

    const bool bad = output.process(left, right, n, p(P::Width), p(P::Character),
                                    p(P::MasterVolume) <= -59.9f ? 0.0f : dbToGain(p(P::MasterVolume)),
                                    pb(P::Limiter));
    if (bad)
    {
        // Something produced NaN/Inf: reset the generators so it cannot persist.
        badBlocks.fetch_add(1, std::memory_order_relaxed);
        voices.killAll();
        drive.reset(); phaser.reset(); flanger.reset(); chorus.reset();
        ensemble.reset(); delay.reset(); reverb.reset();
    }

    activeVoices.store(voices.activeVoiceCount(), std::memory_order_relaxed);
    peaks[0].store(output.getPeak(0), std::memory_order_relaxed);
    peaks[1].store(output.getPeak(1), std::memory_order_relaxed);
    seqStepAtomic.store(seq.currentStep(), std::memory_order_relaxed);
}

// -----------------------------------------------------------------------------
void SynthEngine::smoothParams(float dtSeconds)
{
    const float coeff = std::exp(-dtSeconds / kSmoothingTime);
    for (int i = 0; i < kNumParams; ++i)
    {
        const float t = targets[i];
        float& s = smoothed[i];
        if (! isContinuous[i])
        {
            s = t;
            continue;
        }
        if (s == t)
            continue;
        if (isLogParam[i] && s > 0.0f && t > 0.0f)
            s = t * std::pow(s / t, coeff);
        else
            s = t + (s - t) * coeff;
        if (std::abs(s - t) <= 1.0e-6f * (1.0f + std::abs(t)))
            s = t;
    }
}

void SynthEngine::updateBlockSettings(const TransportInfo& transport)
{
    bpm = transport.hostTempo && transport.bpm > 0.0 ? transport.bpm : (double) targets[P::Tempo];

    const auto seqMode = (SeqMode) (int) std::lround(targets[P::SeqMode]);
    const bool arpOn = targets[P::ArpOn] >= 0.5f;
    const Route newRoute = seqMode == SeqMode::Note ? Route::Seq : (arpOn ? Route::Arp : Route::Voices);
    if (newRoute != route)
    {
        generated.clear();
        arp.stop(generated);
        seq.stop(generated);
        dispatchGenerated();
        voices.allNotesOff();
        for (auto& ch : playedNote)
            for (auto& note : ch)
                note = -1;
        route = newRoute;
    }

    VoiceSettings vs;
    vs.mode = (VoiceMode) (int) std::lround(targets[P::VoiceMode]);
    vs.priority = (NotePriority) (int) std::lround(targets[P::VoicePriority]);
    vs.alloc = (VoiceAlloc) (int) std::lround(targets[P::VoiceAlloc]);
    vs.polyLimit = (int) std::lround(targets[P::Polyphony]);
    vs.unison = targets[P::Unison] >= 0.5f ? (int) std::lround(targets[P::UnisonVoices]) : 1;
    vs.detune = targets[P::UnisonDetune];
    vs.glideTime = targets[P::GlideTime] < 0.001f ? 0.0f : targets[P::GlideTime];
    vs.glideMode = (GlideMode) (int) std::lround(targets[P::GlideMode]);
    vs.velocityCurve = (VelocityCurve) (int) std::lround(targets[P::VelCurve]);
    vs.vintage = targets[P::Vintage];
    if (route == Route::Seq)
    {
        // Note-mode sequencing is monophonic and legato; glides only on slides.
        vs.mode = VoiceMode::Legato;
        vs.glideTime = 0.0f;
    }
    voices.setSettings(vs);

    Arpeggiator::Settings as;
    as.mode = (ArpMode) (int) std::lround(targets[P::ArpMode]);
    as.division = (int) std::lround(targets[P::ArpDiv]);
    as.octaves = (int) std::lround(targets[P::ArpOctaves]);
    as.gate = targets[P::ArpGate];
    as.swing = targets[P::ArpSwing];
    as.hold = targets[P::ArpHold] >= 0.5f;
    as.bpm = bpm;
    arp.setSettings(as);

    StepSequencer::Settings ss;
    ss.mode = seqMode;
    ss.division = (int) std::lround(targets[P::SeqDiv]);
    ss.length = (int) std::lround(targets[P::SeqLength]);
    ss.gate = targets[P::SeqGate];
    ss.swing = targets[P::ArpSwing];
    ss.bpm = bpm;
    for (int s = 0; s < kNumSeqSteps; ++s)
    {
        const int base = s * kSeqParamStride;
        ss.values[s] = targets[P::SeqVal1 + base];
        ss.gates[s] = targets[P::SeqGate1 + base] >= 0.5f;
        ss.accents[s] = targets[P::SeqAcc1 + base] >= 0.5f;
        ss.slides[s] = targets[P::SeqSlide1 + base] >= 0.5f;
    }
    seq.setSettings(ss);

    const bool hostRunning = transport.playing && transport.ppqValid;
    seq.setHostRunning(hostRunning);
    if (hostRunning)
    {
        arp.syncToHost(transport.ppqPosition);
        seq.syncToHost(transport.ppqPosition);
        for (int l = 0; l < 2; ++l)
        {
            const int base = l * kLfoParamStride;
            if (targets[P::Lfo1Sync + base] >= 0.5f && (LfoMode) (int) std::lround(targets[P::Lfo1Mode + base]) == LfoMode::Free)
            {
                const double beats = divisionBeats((int) std::lround(targets[P::Lfo1Div + base]));
                globalLfo[l].setPhase((float) std::fmod(transport.ppqPosition / beats, 1.0));
            }
        }
    }
}

// -----------------------------------------------------------------------------
void SynthEngine::computeShared(int internalSamples)
{
    VoiceShared& s = shared;
    s.sampleRate = internalRate;
    s.invSampleRate = 1.0f / internalRate;

    const auto engine = (EngineMode) pi(P::Engine);
    s.analogActive = engine != EngineMode::FM;
    s.fmActive = engine != EngineMode::Analog;

    // ---- oscillators
    for (int i = 0; i < 3; ++i)
    {
        static constexpr int on[3]    = { P::Osc1On, P::Osc2On, P::Osc3On };
        static constexpr int wave[3]  = { P::Osc1Wave, P::Osc2Wave, P::Osc3Wave };
        static constexpr int oct[3]   = { P::Osc1Octave, P::Osc2Octave, P::Osc3Octave };
        static constexpr int semi[3]  = { P::Osc1Semi, P::Osc2Semi, P::Osc3Semi };
        static constexpr int fine[3]  = { P::Osc1Fine, P::Osc2Fine, P::Osc3Fine };
        static constexpr int pw[3]    = { P::Osc1Pw, P::Osc2Pw, P::Osc3Pw };
        static constexpr int level[3] = { P::Osc1Level, P::Osc2Level, P::Osc3Level };
        static constexpr int fm[3]    = { P::Osc1Fm, P::Osc2Fm, P::Osc3Fm };

        auto& o = s.osc[i];
        o.on = pb(on[i]);
        o.wave = (OscWave) pi(wave[i]);
        o.semis = (float) ((pi(oct[i]) - 2) * 12 + pi(semi[i])) + p(fine[i]) * 0.01f;
        o.pulseWidth = p(pw[i]) * 0.01f;
        o.level = p(level[i]);
        o.fm = p(fm[i]);
        o.sync = i == 1 ? pb(P::Osc2Sync) : (i == 2 ? pb(P::Osc3Sync) : false);
        o.ring = i == 1 ? p(P::Osc2Xmod) : (i == 2 ? p(P::Osc3Xmod) : 0.0f);
    }
    s.subLevel = p(P::SubLevel);
    s.subWave = (SubWave) pi(P::SubWave);
    s.noiseLevel = p(P::NoiseLevel);
    s.noiseType = (NoiseType) pi(P::NoiseType);
    s.drift = p(P::Drift);
    s.mixDrive = p(P::MixDrive);

    s.wavetable = &WavetableBank::get().table(pi(P::WtTable));
    s.wtPos = p(P::WtPos);
    s.wtSpread = p(P::WtSpread);

    // ---- filter
    s.filterModel = (FilterModel) pi(P::FltModel);
    s.filterType = (FilterType) pi(P::FltType);
    s.filterSlope = (FilterSlope) pi(P::FltSlope);
    s.cutoffNote = 69.0f + 12.0f * std::log2(std::max(1.0f, p(P::FltCutoff)) / 440.0f);
    s.resonance = p(P::FltRes);
    s.filterDrive = p(P::FltDrive);
    s.filterEnvSemis = p(P::FltEnv) * 96.0f * (pb(P::FltEnvInvert) ? -1.0f : 1.0f);
    s.keyTrack = p(P::FltKey);
    s.filterVelSemis = p(P::FltVel) * 48.0f;
    s.filterLfoSemis = p(P::FltLfo) * 48.0f;
    s.hpfHz = p(P::FltHpf);

    // ---- envelopes
    s.env[0].set(p(P::Env1A), p(P::Env1D), p(P::Env1S), p(P::Env1R), internalRate);
    s.env[1].set(p(P::Env2A), p(P::Env2D), p(P::Env2S), p(P::Env2R), internalRate);
    s.env[2].set(p(P::Env3A), p(P::Env3D), p(P::Env3S), p(P::Env3R), internalRate);
    s.ampVelocity = p(P::Env1Vel);
    s.env2Velocity = p(P::Env2Vel);
    s.env3Amount = p(P::Env3Amt);
    s.env3Dest = env3DestToModDest((Env3Dest) pi(P::Env3Dest));

    // ---- LFOs (the global instances serve Free mode and the effects)
    const float dt = (float) internalSamples / internalRate;
    for (int l = 0; l < 2; ++l)
    {
        const int b = l * kLfoParamStride;
        auto& L = s.lfo[l];
        L.wave = (LfoWave) pi(P::Lfo1Wave + b);
        L.rateHz = pb(P::Lfo1Sync + b)
                     ? (float) (bpm / 60.0 / divisionBeats(pi(P::Lfo1Div + b)))
                     : p(P::Lfo1Rate + b);
        L.phase = p(P::Lfo1Phase + b) / 360.0f;
        L.delaySec = p(P::Lfo1Delay + b);
        L.amount = p(P::Lfo1Amount + b);
        L.mode = (LfoMode) pi(P::Lfo1Mode + b);
        L.unipolar = pb(P::Lfo1Unipolar + b);
        L.dest = lfoDestToModDest((LfoDest) pi(P::Lfo1Dest + b));

        globalLfo[l].advance(L.rateHz, dt, L.wave, false);
        s.globalLfo[l] = globalLfo[l].valueAt(L.wave, L.phase);
    }

    // ---- amplifier / voice
    s.ampLevel = p(P::AmpLevel);
    s.pan = p(P::AmpPan);
    s.stereoSpread = p(P::UnisonSpread);
    s.vintage = p(P::Vintage);
    s.masterTuneSemis = p(P::MasterTune) * 0.01f;

    // ---- controllers
    s.pitchBend = pitchBend;
    s.pitchBendSemis = pitchBend * (float) pi(P::BendRange);
    s.modWheel = modWheel;
    s.aftertouch = aftertouch;
    s.expression = expression;
    s.macro = p(P::Macro);
    s.seqValue = seq.currentValue();
    s.sampleHold = sampleHoldValue;

    // ---- FM
    s.fmLevel = p(P::FmLevel);
    s.fm.algorithm = pi(P::FmAlgo);
    s.fm.depth = p(P::FmDepth);
    s.fm.feedback = p(P::FmFeedback);
    s.fm.velocitySens = p(P::FmVel);
    if (s.fmActive)
    {
        for (int op = 0; op < kNumOperators; ++op)
        {
            const int b = op * kOpParamStride;
            s.fm.ratio[op] = snapRatio(p(P::Op1Ratio + b)) * (1.0f + p(P::Op1Fine + b));
            s.fm.level[op] = p(P::Op1Level + b);
            s.fm.env[op].set(p(P::Op1A + b), p(P::Op1D + b), p(P::Op1S + b), p(P::Op1R + b), internalRate);
        }
    }

    // ---- modulation matrix
    s.numRoutings = 0;
    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const int b = slot * kModParamStride;
        const int src = pi(P::Mod1Src + b);
        const int dst = pi(P::Mod1Dst + b);
        const float amt = p(P::Mod1Amt + b);
        if (src != (int) ModSource::Off && dst != (int) ModDest::Off && amt != 0.0f)
            s.routings[s.numRoutings++] = ModRouting { src, dst, amt };
    }
}

// -----------------------------------------------------------------------------
void SynthEngine::handleMidi(const MidiEvent& e)
{
    const int type = e.status & 0xF0;
    const int channel = e.status & 0x0F;
    const int receive = (int) std::lround(targets != nullptr ? targets[P::MidiChannel] : 0.0f);
    if (receive > 0 && channel != receive - 1)
        return;

    midiCounter.fetch_add(1, std::memory_order_relaxed);

    switch (type)
    {
        case 0x90:
            if (e.data2 > 0) noteOnIn(channel, e.data1 & 0x7F, e.data2);
            else             noteOffIn(channel, e.data1 & 0x7F);
            break;
        case 0x80:
            noteOffIn(channel, e.data1 & 0x7F);
            break;
        case 0xB0:
            switch (e.data1)
            {
                case 1:   modWheel = (float) e.data2 / 127.0f; break;
                case 11:  expression = (float) e.data2 / 127.0f; break;
                case 64:  voices.setSustain(e.data2 >= 64); break;
                case 120: voices.killAll(); break;
                case 121:
                    pitchBend = 0.0f; modWheel = 0.0f; aftertouch = 0.0f; expression = 1.0f;
                    voices.setSustain(false);
                    break;
                case 123:
                {
                    generated.clear();
                    arp.stop(generated);
                    seq.stop(generated);
                    voices.allNotesOff();
                    for (auto& ch : playedNote)
                        for (auto& n : ch)
                            n = -1;
                    break;
                }
                default: break;
            }
            break;
        case 0xE0:
            pitchBend = clampf((float) (((e.data2 & 0x7F) << 7 | (e.data1 & 0x7F)) - 8192) / 8191.0f, -1.0f, 1.0f);
            break;
        case 0xD0:
            aftertouch = (float) e.data1 / 127.0f;
            break;
        case 0xA0:
        {
            const int played = playedNote[channel][e.data1 & 0x7F];
            if (played >= 0)
                voices.polyAftertouch(played, (float) e.data2 / 127.0f);
            break;
        }
        default:
            break;
    }
}

void SynthEngine::noteOnIn(int channel, int note, int velocity)
{
    if (playedNote[channel][note] >= 0)
        noteOffIn(channel, note); // repeated note-on without note-off

    const int shift = (int) std::lround(targets[P::Transpose]) + 12 * (int) std::lround(targets[P::OctaveShift]);
    const int played = note + shift;
    if (played < 0 || played > 127)
        return;
    playedNote[channel][note] = played;

    switch (route)
    {
        case Route::Arp: arp.noteOn(played, velocity); break;
        case Route::Seq: seq.noteOn(played, velocity); break;
        default:         voices.noteOn(played, velocity); break;
    }
}

void SynthEngine::noteOffIn(int channel, int note)
{
    const int played = playedNote[channel][note];
    if (played < 0)
        return;
    playedNote[channel][note] = -1;

    switch (route)
    {
        case Route::Arp: arp.noteOff(played); break;
        case Route::Seq: seq.noteOff(played); break;
        default:         voices.noteOff(played); break;
    }
}

void SynthEngine::dispatchGenerated()
{
    for (int i = 0; i < generated.count; ++i)
    {
        const auto& g = generated.events[i];
        if (g.velocity > 0)
            voices.noteOn(g.note, g.velocity, g.glide);
        else
            voices.noteOff(g.note);
    }
    generated.clear();
}

// -----------------------------------------------------------------------------
void SynthEngine::runEffects(float* left, float* right, int n)
{
    // Global modulation of effect parameters (sources that exist outside voices).
    float src[(int) ModSource::Count] {};
    for (int l = 0; l < 2; ++l)
    {
        float v = shared.globalLfo[l];
        if (shared.lfo[l].unipolar)
            v = 0.5f * (v + 1.0f);
        src[(int) ModSource::Lfo1 + l] = v;
    }
    src[(int) ModSource::Velocity] = voices.lastVelocity();
    src[(int) ModSource::Key] = voices.lastNote() >= 0 ? ((float) voices.lastNote() - 60.0f) / 60.0f : 0.0f;
    src[(int) ModSource::ModWheel] = modWheel;
    src[(int) ModSource::PitchBend] = pitchBend;
    src[(int) ModSource::Aftertouch] = aftertouch;
    src[(int) ModSource::Expression] = expression;
    src[(int) ModSource::SampleHold] = sampleHoldValue;
    src[(int) ModSource::SeqStep] = seq.currentValue();
    src[(int) ModSource::Macro] = p(P::Macro);

    float fx[5] {};
    for (int r = 0; r < shared.numRoutings; ++r)
    {
        const auto& route = shared.routings[r];
        if (isGlobalDestination(route.destination))
            fx[route.destination - (int) ModDest::FxDrive] += src[route.source] * route.amount;
    }

    auto runFaded = [&](int slot, bool enabled, auto&& processFn, auto&& resetFn)
    {
        float& fade = fades[slot];
        if (! enabled && fade <= 0.0f)
            return;
        std::copy(left, left + n, fxTempL.begin());
        std::copy(right, right + n, fxTempR.begin());
        processFn(fxTempL.data(), fxTempR.data());
        const float step = 1.0f / (0.02f * (float) baseRate);
        for (int i = 0; i < n; ++i)
        {
            fade = enabled ? std::min(1.0f, fade + step) : std::max(0.0f, fade - step);
            left[i] += (fxTempL[(size_t) i] - left[i]) * fade;
            right[i] += (fxTempR[(size_t) i] - right[i]) * fade;
        }
        if (! enabled && fade <= 0.0f)
            resetFn();
    };

    runFaded(0, pb(P::DriveOn), [&](float* l, float* r) {
        drive.process(l, r, n, (DriveType) pi(P::DriveType), clampf(p(P::DriveAmount) + fx[0], 0.0f, 1.0f), p(P::DriveTone), p(P::DriveMix));
    }, [&] { drive.reset(); });

    runFaded(1, pb(P::PhaserOn), [&](float* l, float* r) {
        phaser.process(l, r, n, p(P::PhaserRate), p(P::PhaserDepth), p(P::PhaserFb), p(P::PhaserMix));
    }, [&] { phaser.reset(); });

    runFaded(2, pb(P::FlangerOn), [&](float* l, float* r) {
        flanger.process(l, r, n, p(P::FlangerRate), p(P::FlangerDepth), p(P::FlangerFb), p(P::FlangerMix));
    }, [&] { flanger.reset(); });

    runFaded(3, pb(P::ChorusOn), [&](float* l, float* r) {
        chorus.process(l, r, n, p(P::ChorusRate), p(P::ChorusDepth), clampf(p(P::ChorusMix) + fx[1], 0.0f, 1.0f));
    }, [&] { chorus.reset(); });

    runFaded(4, pb(P::EnsOn), [&](float* l, float* r) {
        ensemble.process(l, r, n, p(P::EnsRate), p(P::EnsDepth), p(P::EnsMix));
    }, [&] { ensemble.reset(); });

    runFaded(5, pb(P::DelayOn), [&](float* l, float* r) {
        const float seconds = pb(P::DelaySync) ? (float) (divisionBeats(pi(P::DelayDiv)) * 60.0 / bpm) : p(P::DelayTime) * 0.001f;
        delay.process(l, r, n, std::min(seconds, 2.0f), clampf(p(P::DelayFb) + fx[4], 0.0f, 0.95f), p(P::DelayTone),
                      pb(P::DelayPingPong), clampf(p(P::DelayMix) + fx[2], 0.0f, 1.0f));
    }, [&] { delay.reset(); });

    runFaded(6, pb(P::ReverbOn), [&](float* l, float* r) {
        reverb.process(l, r, n, p(P::ReverbSize), p(P::ReverbDamp), p(P::ReverbPredelay) * 0.001f,
                       clampf(p(P::ReverbMix) + fx[3], 0.0f, 1.0f));
    }, [&] { reverb.reset(); });
}

} // namespace zsynth::dsp
