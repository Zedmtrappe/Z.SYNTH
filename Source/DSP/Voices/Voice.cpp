#include "Voice.h"

namespace zsynth::dsp
{

void Voice::prepare(float internalSampleRate, int voiceIndex)
{
    sampleRate = internalSampleRate;
    index = voiceIndex;
    rng.setSeed(0x9E3779B9u ^ (uint32_t) (voiceIndex * 7919 + 17));

    // Fixed per-voice "component tolerances" for the vintage control. They are
    // tied to the voice slot (as in hardware), not re-rolled per note.
    for (auto& t : vintageTune)
        t = rng.bipolar();
    vintageCutoff = rng.bipolar();
    vintageLevel = rng.bipolar();
    vintagePan = rng.bipolar();

    noise.setSeed((uint32_t) (voiceIndex * 31 + 5));
    lfo[0].setSeed((uint32_t) (voiceIndex * 131 + 7));
    lfo[1].setSeed((uint32_t) (voiceIndex * 257 + 11));

    ampEnv.setFastReleaseCoeff(sampleRate);
    env2.setFastReleaseCoeff(sampleRate);
    env3.setFastReleaseCoeff(sampleRate);
    fm.setFastRelease(sampleRate);
    kill();
}

void Voice::begin(const NoteStart& ns)
{
    note = ns.note;
    velocity = ns.velocity;
    age = ns.age;
    unisonDetune = ns.unisonDetune;
    unisonPan = ns.unisonPan;
    targetPitch = (float) ns.note;
    notePitch = ns.glideFrom >= 0.0f ? ns.glideFrom : targetPitch;
    glideTime = ns.glideTime;
    randomValue = rng.bipolar();
    polyAftertouch = 0.0f;

    for (int i = 0; i < 3; ++i)
        osc[i].reset(ns.randomPhase ? rng.unipolar() : 0.0f);
    sub.reset(0.0f);
    lastOsc3 = 0.0f;

    filter.reset();
    hpf.reset();
    ampEnv.reset();
    env2.reset();
    env3.reset();
    ampEnv.noteOn();
    env2.noteOn();
    env3.noteOn();
    fm.noteOn(velocity, true);

    for (int i = 0; i < 3; ++i)
        wtFrame[i] = -1;

    active = true;
    firstBlock = true;
}

void Voice::start(const NoteStart& ns)
{
    hasPending = false;
    begin(ns);
}

void Voice::stealAndStart(const NoteStart& ns)
{
    if (! active || ampEnv.getLevel() < 1.0e-3f)
    {
        kill();
        begin(ns);
        return;
    }
    ampEnv.forceFastRelease();
    pending = ns;
    hasPending = true;
}

void Voice::legatoTo(int newNote, float glideSeconds, uint32_t newAge)
{
    if (hasPending)
    {
        pending.note = newNote;
        pending.age = newAge;
        return;
    }
    note = newNote;
    age = newAge;
    targetPitch = (float) newNote;
    glideTime = glideSeconds;
    if (glideSeconds <= 0.0f)
        notePitch = targetPitch;
}

void Voice::retrigger(int newNote, float vel, float glideSeconds, uint32_t newAge)
{
    if (hasPending)
    {
        pending.note = newNote;
        pending.velocity = vel;
        pending.age = newAge;
        return;
    }
    note = newNote;
    velocity = vel;
    age = newAge;
    targetPitch = (float) newNote;
    glideTime = glideSeconds;
    if (glideSeconds <= 0.0f)
        notePitch = targetPitch;
    ampEnv.noteOn();
    env2.noteOn();
    env3.noteOn();
    fm.noteOn(velocity, false);
    randomValue = rng.bipolar();
    firstBlockLfoReset = true;
}

void Voice::release()
{
    if (hasPending)
    {
        // The pending note was released before it began: drop it, let the fade finish.
        hasPending = false;
        return;
    }
    ampEnv.noteOff();
    env2.noteOff();
    env3.noteOff();
    fm.noteOff();
}

void Voice::kill()
{
    active = false;
    hasPending = false;
    keyHeld = false;
    sustained = false;
    ampEnv.reset();
    env2.reset();
    env3.reset();
    fm.kill();
}

float Voice::pitchForOsc(int i, const VoiceShared& s) const
{
    return notePitch + s.osc[i].semis + s.pitchBendSemis + unisonDetune + s.masterTuneSemis
         + vintageTune[i] * s.vintage * 0.08f // up to +/-8 cents of fixed component spread
         + drift[i] * s.drift * 0.12f;        // up to +/-12 cents of slow wander
}

void Voice::updateControl(int n, const VoiceShared& s)
{
    const float dt = (float) n * s.invSampleRate;

    if (firstBlock || firstBlockLfoReset)
    {
        for (int l = 0; l < 2; ++l)
        {
            if (s.lfo[l].mode != LfoMode::Free)
                lfo[l].reset(s.lfo[l].phase);
            lfoFade[l] = s.lfo[l].delaySec > 0.0f ? 0.0f : 1.0f;
        }
        firstBlockLfoReset = false;
    }

    // Glide: exponential (RC-like) approach, ~95 % complete after glideTime.
    if (glideTime > 0.0f && notePitch != targetPitch)
    {
        const float coeff = std::exp(-dt * 3.0f / glideTime);
        notePitch = targetPitch + (notePitch - targetPitch) * coeff;
        if (std::abs(notePitch - targetPitch) < 1.0e-3f)
            notePitch = targetPitch;
    }
    else
    {
        notePitch = targetPitch;
    }

    // Oscillator drift: a slow, smoothed random walk per oscillator.
    driftTimer -= dt;
    if (driftTimer <= 0.0f)
    {
        for (auto& d : driftTarget)
            d = rng.bipolar();
        driftTimer = 0.25f + 0.5f * rng.unipolar();
    }
    const float driftSmooth = std::min(1.0f, dt * 1.5f);
    for (int i = 0; i < 3; ++i)
        drift[i] += (driftTarget[i] - drift[i]) * driftSmooth;

    // Envelopes 2 and 3 run at block resolution; the amp envelope runs per sample.
    const float e1 = ampEnv.getLevel();
    const float e2 = env2.processBlock(s.env[1], n) * (1.0f - s.env2Velocity + s.env2Velocity * velocity);
    const float e3 = env3.processBlock(s.env[2], n);

    for (int l = 0; l < 2; ++l)
    {
        const auto& L = s.lfo[l];
        float v;
        if (L.mode == LfoMode::Free)
            v = s.globalLfo[l];
        else
            v = lfo[l].advance(L.rateHz * fastExp2(lfoRateMod[l] * ModRange::kRateOctaves), dt, L.wave, L.mode == LfoMode::OneShot);
        if (L.unipolar)
            v = 0.5f * (v + 1.0f);
        if (L.delaySec > 0.0f)
            lfoFade[l] = std::min(1.0f, lfoFade[l] + dt / L.delaySec);
        else
            lfoFade[l] = 1.0f;
        lfoValue[l] = v * lfoFade[l];
    }

    // ---- modulation matrix
    float src[(int) ModSource::Count];
    src[(int) ModSource::Off] = 0.0f;
    src[(int) ModSource::Lfo1] = lfoValue[0];
    src[(int) ModSource::Lfo2] = lfoValue[1];
    src[(int) ModSource::Env1] = e1;
    src[(int) ModSource::Env2] = e2;
    src[(int) ModSource::Env3] = e3;
    src[(int) ModSource::Velocity] = velocity;
    src[(int) ModSource::Key] = ((float) note - 60.0f) / 60.0f;
    src[(int) ModSource::ModWheel] = s.modWheel;
    src[(int) ModSource::PitchBend] = s.pitchBend;
    src[(int) ModSource::Aftertouch] = s.aftertouch;
    src[(int) ModSource::PolyAftertouch] = polyAftertouch;
    src[(int) ModSource::Expression] = s.expression;
    src[(int) ModSource::Random] = randomValue;
    src[(int) ModSource::SampleHold] = s.sampleHold;
    src[(int) ModSource::SeqStep] = s.seqValue;
    src[(int) ModSource::Macro] = s.macro;

    ModAccumulator m;
    m.clear();
    for (int r = 0; r < s.numRoutings; ++r)
    {
        const auto& route = s.routings[r];
        if (! isGlobalDestination(route.destination))
            m.value[route.destination] += src[route.source] * route.amount;
    }
    for (int l = 0; l < 2; ++l)
    {
        if (s.lfo[l].dest != ModDest::Off)
        {
            const float amount = clampf(s.lfo[l].amount + m[l == 0 ? ModDest::Lfo1Amount : ModDest::Lfo2Amount], 0.0f, 1.0f);
            m.add(s.lfo[l].dest, lfoValue[l] * amount);
        }
    }
    if (s.env3Dest != ModDest::Off)
        m.add(s.env3Dest, e3 * s.env3Amount);

    lfoRateMod[0] = m[ModDest::Lfo1Rate];
    lfoRateMod[1] = m[ModDest::Lfo2Rate];

    // ---- pitch
    const float globalPitchMod = m[ModDest::Pitch] * ModRange::kPitchSemis;
    const ModDest oscPitchDest[3] = { ModDest::Osc1Pitch, ModDest::Osc2Pitch, ModDest::Osc3Pitch };
    const ModDest oscLevelDest[3] = { ModDest::Osc1Level, ModDest::Osc2Level, ModDest::Osc3Level };
    for (int i = 0; i < 3; ++i)
    {
        const float p = pitchForOsc(i, s) + globalPitchMod + m[oscPitchDest[i]] * ModRange::kPitchSemis;
        incEnd[i] = clampf(midiNoteToHz(p) * s.invSampleRate, 0.0f, 0.45f);
    }
    const float fmPitch = notePitch + s.pitchBendSemis + unisonDetune + s.masterTuneSemis + globalPitchMod
                        + vintageTune[0] * s.vintage * 0.08f + drift[0] * s.drift * 0.12f;
    baseIncEnd = clampf(midiNoteToHz(fmPitch) * s.invSampleRate, 0.0f, 0.45f);

    // ---- oscillator settings
    const float levelTolerance = 1.0f + vintageLevel * s.vintage * 0.12f;
    const float spreadFactor[3] = { 0.0f, 0.5f, 1.0f };
    for (int i = 0; i < 3; ++i)
    {
        const auto& o = s.osc[i];
        osc[i].setWave(o.wave);
        osc[i].setPulseWidth(o.pulseWidth + m[ModDest::PulseWidth] * ModRange::kPulseWidth);
        level[i] = o.on ? clampf(o.level + m[oscLevelDest[i]], 0.0f, 1.0f) * levelTolerance : 0.0f;
        fmAmt[i] = clampf(o.fm + m[ModDest::OscFm], 0.0f, 1.0f);
        ring[i] = o.ring;

        if (o.wave == OscWave::Wavetable && s.wavetable != nullptr)
        {
            const float pos = clampf(s.wtPos + m[ModDest::WavetablePos] + s.wtSpread * spreadFactor[i], 0.0f, 1.0f);
            const float fpos = pos * (float) (Wavetable::kFrames - 1);
            const int f0 = std::min((int) fpos, Wavetable::kFrames - 2);
            const float mix = fpos - (float) f0;
            const int mip = Wavetable::mipForIncrement(incEnd[i] * (1.0f + 2.0f * fmAmt[i]));
            osc[i].setWavetableFrames(s.wavetable->frame(f0, mip), s.wavetable->frame(f0 + 1, mip));
            wtMixStart[i] = (f0 == wtFrame[i]) ? wtMixEnd[i] : mix;
            wtMixEnd[i] = mix;
            wtFrame[i] = f0;
        }
    }
    sub.setWave(s.subWave);
    subLevel = clampf(s.subLevel + m[ModDest::SubLevel], 0.0f, 1.0f);
    noiseLevel = clampf(s.noiseLevel + m[ModDest::NoiseLevel], 0.0f, 1.0f);

    if (s.fmActive)
        fm.prepareBlock(s.fm, m[ModDest::FmDepth], m[ModDest::FmFeedback]);
    fmGain = s.fmLevel;

    // ---- filter
    const float cutoffNote = s.cutoffNote
                           + ((float) note - 60.0f) * s.keyTrack
                           + e2 * s.filterEnvSemis
                           + velocity * s.filterVelSemis
                           + lfoValue[0] * s.filterLfoSemis
                           + m[ModDest::Cutoff] * ModRange::kCutoffSemis
                           + vintageCutoff * s.vintage * 1.5f;
    gEnd = prewarp(midiNoteToHz(clampf(cutoffNote, -20.0f, 150.0f)), sampleRate);
    res = clampf(s.resonance + m[ModDest::Resonance], 0.0f, 1.0f);
    fltDrive = clampf(s.filterDrive + m[ModDest::FilterDrive], 0.0f, 1.0f);
    filter.setModel(s.filterModel);

    const float hpfHz = s.hpfHz * semitonesToRatio(clampf(m[ModDest::HpfCutoff] * ModRange::kCutoffSemis, -96.0f, 96.0f));
    hpfOn = hpfHz > 12.0f;
    hpfG = prewarp(hpfHz, sampleRate);

    // ---- amplifier and pan
    const float velGain = 1.0f - s.ampVelocity + s.ampVelocity * velocity;
    gainEnd = s.ampLevel * velGain * std::max(0.0f, 1.0f + m[ModDest::AmpLevel]);

    const float pan = clampf(s.pan + unisonPan * s.stereoSpread + m[ModDest::Pan] + vintagePan * s.vintage * 0.15f, -1.0f, 1.0f);
    const float angle = (pan + 1.0f) * (kPi * 0.25f);
    panLEnd = std::cos(angle) * 1.41421356f;
    panREnd = std::sin(angle) * 1.41421356f;

    if (firstBlock)
    {
        for (int i = 0; i < 3; ++i)
        {
            incStart[i] = incEnd[i];
            wtMixStart[i] = wtMixEnd[i];
        }
        baseIncStart = baseIncEnd;
        gStart = gEnd;
        gainStart = gainEnd;
        panLStart = panLEnd;
        panRStart = panREnd;
        firstBlock = false;
    }
}

void Voice::render(float* left, float* right, int n, const VoiceShared& s)
{
    if (! active)
        return;

    if (hasPending && ! ampEnv.isActive())
    {
        hasPending = false;
        begin(pending);
    }

    updateControl(n, s);

    const float invN = 1.0f / (float) n;
    const float gRatio = std::pow(gEnd / gStart, invN);
    float g = gStart;

    const bool analog = s.analogActive;
    const bool fmOn = s.fmActive;
    const bool osc2On = level[1] > 0.0f;
    const bool osc3On = level[2] > 0.0f || fmAmt[0] > 0.0f;
    const bool wt[3] = { s.osc[0].wave == OscWave::Wavetable, s.osc[1].wave == OscWave::Wavetable, s.osc[2].wave == OscWave::Wavetable };
    const FilterType type = s.filterType;
    const FilterSlope slope = s.filterSlope;
    // Mixer overdrive. The makeup is chosen so a typical full-level mix (0.5
    // after the mixer scaling) leaves the stage at the same level at any drive:
    // drive adds density and harmonics, not loudness.
    const float driveGain = 1.0f + s.mixDrive * s.mixDrive * 9.0f;
    const float driveMakeup = fastTanh(0.5f) / fastTanh(0.5f * driveGain);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) (i + 1) * invN;
        float sum = 0.0f;

        if (analog)
        {
            for (int k = 0; k < 3; ++k)
                if (wt[k])
                    osc[k].setWavetableMix(lerp(wtMixStart[k], wtMixEnd[k], t));

            const float inc1 = lerp(incStart[0], incEnd[0], t) * (1.0f + fmAmt[0] * 4.0f * lastOsc3);
            const float o1 = osc[0].process(inc1, -1.0f);
            const float wrap = osc[0].wrapAfter();
            sum += o1 * level[0];

            if (osc2On)
            {
                float o2 = osc[1].process(lerp(incStart[1], incEnd[1], t) * (1.0f + fmAmt[1] * 4.0f * o1),
                                          s.osc[1].sync ? wrap : -1.0f);
                o2 += (o2 * o1 - o2) * ring[1];
                sum += o2 * level[1];
            }
            if (osc3On)
            {
                float o3 = osc[2].process(lerp(incStart[2], incEnd[2], t) * (1.0f + fmAmt[2] * 4.0f * o1),
                                          s.osc[2].sync ? wrap : -1.0f);
                lastOsc3 = o3;
                o3 += (o3 * o1 - o3) * ring[2];
                sum += o3 * level[2];
            }
            if (subLevel > 0.0f)
                sum += sub.process(wrap, inc1) * subLevel;
        }

        if (noiseLevel > 0.0f)
            sum += noise.process(s.noiseType) * noiseLevel;

        if (fmOn)
            sum += fm.process(lerp(baseIncStart, baseIncEnd, t)) * fmGain;

        // Mixer: gentle tanh overload with level compensation.
        float x = fastTanh(sum * 0.5f * driveGain) * driveMakeup;

        if (hpfOn)
            x = hpf.process(x, hpfG);

        g *= gRatio;
        x = filter.process(x, g, res, fltDrive, type, slope);

        const float amp = ampEnv.process(s.env[0]) * lerp(gainStart, gainEnd, t);
        const float out = x * amp;
        left[i]  += out * lerp(panLStart, panLEnd, t);
        right[i] += out * lerp(panRStart, panREnd, t);
    }

    for (int k = 0; k < 3; ++k)
    {
        incStart[k] = incEnd[k];
        wtMixStart[k] = wtMixEnd[k];
    }
    baseIncStart = baseIncEnd;
    gStart = gEnd;
    gainStart = gainEnd;
    panLStart = panLEnd;
    panRStart = panREnd;

    if (! ampEnv.isActive() && ! hasPending)
        kill();
}

} // namespace zsynth::dsp
