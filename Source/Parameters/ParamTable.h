#pragma once

// =============================================================================
//  ZYNTH — the single authoritative parameter table.
//
//  Every synthesis parameter is declared exactly once, below. From this table:
//    * the DSP engine gets a dense enum (ParamIndex) to index a float snapshot,
//    * the plug-in layer builds the JUCE AudioProcessorValueTreeState,
//    * the UI binds its controls by string id,
//    * the preset system serialises values by string id.
//
//  Values held by the engine are always in "real" units (Hz, seconds, cents,
//  0..1 amounts, choice index, 0/1 for booleans).
//
//  STABILITY RULE: the string ids are the persistent identity used by presets,
//  host automation and MIDI-learn maps. Never rename or remove an id once it
//  has shipped; add an entry to the migration table in PresetFormat instead.
//
//  This header is JUCE-free.
// =============================================================================

#include "ParamChoices.h"

namespace zsynth
{

enum class ParamType : unsigned char { Float, Int, Choice, Bool };

// Scale codes (stored in ParamDef::scale):
//   LIN   (0)  linear mapping
//   LOG   (-1) exponential mapping (equal ratio per knob turn); min must be > 0
//   >0         power-law skew such that this value sits at the knob's centre
#define LIN 0.0f
#define LOG -1.0f

// X(Enum, "id", "Display Name", "Group", Type, min, max, default, scale, "unit", choices, "help")
// For Choice params min/max are derived from the list; pass 0,0.
#define ZS_PARAM_LIST(X) \
/* ---------------------------------------------------------------- Global */ \
X(MasterVolume,  "master.volume",  "Master Volume", "Global", Float, -60.f, 6.f, -6.f, LIN, "dB", nullptr, "Final output level after all effects.") \
X(MasterTune,    "master.tune",    "Master Tune",   "Global", Float, -100.f, 100.f, 0.f, LIN, "ct", nullptr, "Global fine tuning in cents (A4 = 440 Hz at 0).") \
X(Transpose,     "master.transpose","Transpose",    "Global", Int, -24.f, 24.f, 0.f, LIN, "st", nullptr, "Transposes all incoming notes by semitones.") \
X(OctaveShift,   "master.octave",  "Octave Shift",  "Global", Int, -3.f, 3.f, 0.f, LIN, "oct", nullptr, "Shifts all incoming notes by whole octaves.") \
X(Tempo,         "master.tempo",   "Tempo",         "Global", Float, 40.f, 240.f, 120.f, LIN, "BPM", nullptr, "Internal tempo, used when no host tempo is available.") \
X(Oversampling,  "master.oversampling","Quality",   "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_OVERSAMPLING, "Voice oversampling. Auto = 2x below 80 kHz, 1x above.") \
X(MidiChannel,   "midi.channel",   "MIDI Channel",  "Global", Int, 0.f, 16.f, 0.f, LIN, "", nullptr, "MIDI receive channel. 0 = Omni (all channels).") \
/* ------------------------------------------------------------ Oscillators */ \
X(Engine,        "osc.engine",     "Engine",        "Oscillators", Choice, 0, 0, 0.f, LIN, "", ZS_CH_ENGINE, "Voice engine: Analog oscillators, FM operators, or both (Hybrid).") \
X(Osc1On,        "osc1.on",        "Osc 1 On",      "Oscillators", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Enables oscillator 1.") \
X(Osc1Wave,      "osc1.wave",      "Osc 1 Wave",    "Oscillators", Choice, 0, 0, 2.f, LIN, "", ZS_CH_OSC_WAVE, "Oscillator 1 waveform. Wavetable uses the WAVE section table.") \
X(Osc1Octave,    "osc1.octave",    "Osc 1 Range",   "Oscillators", Choice, 0, 0, 2.f, LIN, "", ZS_CH_OCTAVE, "Oscillator 1 footage (octave).") \
X(Osc1Semi,      "osc1.semi",      "Osc 1 Semi",    "Oscillators", Int, -12.f, 12.f, 0.f, LIN, "st", nullptr, "Oscillator 1 coarse tuning in semitones.") \
X(Osc1Fine,      "osc1.fine",      "Osc 1 Fine",    "Oscillators", Float, -100.f, 100.f, 0.f, LIN, "ct", nullptr, "Oscillator 1 fine tuning in cents.") \
X(Osc1Pw,        "osc1.pw",        "Osc 1 PW",      "Oscillators", Float, 5.f, 95.f, 50.f, LIN, "%", nullptr, "Pulse width (Pulse wave only).") \
X(Osc1Level,     "osc1.level",     "Osc 1 Level",   "Oscillators", Float, 0.f, 1.f, 0.8f, LIN, "", nullptr, "Oscillator 1 level into the mixer.") \
X(Osc1Fm,        "osc1.fm",        "Osc 1 FM",      "Oscillators", Float, 0.f, 1.f, 0.f, 0.2f, "", nullptr, "Audio-rate frequency modulation of osc 1 by osc 3.") \
X(Osc2On,        "osc2.on",        "Osc 2 On",      "Oscillators", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables oscillator 2.") \
X(Osc2Wave,      "osc2.wave",      "Osc 2 Wave",    "Oscillators", Choice, 0, 0, 2.f, LIN, "", ZS_CH_OSC_WAVE, "Oscillator 2 waveform.") \
X(Osc2Octave,    "osc2.octave",    "Osc 2 Range",   "Oscillators", Choice, 0, 0, 2.f, LIN, "", ZS_CH_OCTAVE, "Oscillator 2 footage (octave).") \
X(Osc2Semi,      "osc2.semi",      "Osc 2 Semi",    "Oscillators", Int, -12.f, 12.f, 0.f, LIN, "st", nullptr, "Oscillator 2 coarse tuning in semitones.") \
X(Osc2Fine,      "osc2.fine",      "Osc 2 Fine",    "Oscillators", Float, -100.f, 100.f, 0.f, LIN, "ct", nullptr, "Oscillator 2 fine tuning in cents.") \
X(Osc2Pw,        "osc2.pw",        "Osc 2 PW",      "Oscillators", Float, 5.f, 95.f, 50.f, LIN, "%", nullptr, "Pulse width (Pulse wave only).") \
X(Osc2Level,     "osc2.level",     "Osc 2 Level",   "Oscillators", Float, 0.f, 1.f, 0.8f, LIN, "", nullptr, "Oscillator 2 level into the mixer.") \
X(Osc2Sync,      "osc2.sync",      "Osc 2 Sync",    "Oscillators", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Hard-syncs oscillator 2 to oscillator 1.") \
X(Osc2Fm,        "osc2.fm",        "Osc 2 FM",      "Oscillators", Float, 0.f, 1.f, 0.f, 0.2f, "", nullptr, "Audio-rate frequency modulation of osc 2 by osc 1.") \
X(Osc2Xmod,      "osc2.xmod",      "Osc 2 Ring",    "Oscillators", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Cross-fades osc 2 into ring modulation with osc 1.") \
X(Osc3On,        "osc3.on",        "Osc 3 On",      "Oscillators", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables oscillator 3.") \
X(Osc3Wave,      "osc3.wave",      "Osc 3 Wave",    "Oscillators", Choice, 0, 0, 3.f, LIN, "", ZS_CH_OSC_WAVE, "Oscillator 3 waveform.") \
X(Osc3Octave,    "osc3.octave",    "Osc 3 Range",   "Oscillators", Choice, 0, 0, 1.f, LIN, "", ZS_CH_OCTAVE, "Oscillator 3 footage (octave).") \
X(Osc3Semi,      "osc3.semi",      "Osc 3 Semi",    "Oscillators", Int, -12.f, 12.f, 0.f, LIN, "st", nullptr, "Oscillator 3 coarse tuning in semitones.") \
X(Osc3Fine,      "osc3.fine",      "Osc 3 Fine",    "Oscillators", Float, -100.f, 100.f, 0.f, LIN, "ct", nullptr, "Oscillator 3 fine tuning in cents.") \
X(Osc3Pw,        "osc3.pw",        "Osc 3 PW",      "Oscillators", Float, 5.f, 95.f, 50.f, LIN, "%", nullptr, "Pulse width (Pulse wave only).") \
X(Osc3Level,     "osc3.level",     "Osc 3 Level",   "Oscillators", Float, 0.f, 1.f, 0.8f, LIN, "", nullptr, "Oscillator 3 level into the mixer.") \
X(Osc3Sync,      "osc3.sync",      "Osc 3 Sync",    "Oscillators", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Hard-syncs oscillator 3 to oscillator 1.") \
X(Osc3Fm,        "osc3.fm",        "Osc 3 FM",      "Oscillators", Float, 0.f, 1.f, 0.f, 0.2f, "", nullptr, "Audio-rate frequency modulation of osc 3 by osc 1.") \
X(Osc3Xmod,      "osc3.xmod",      "Osc 3 Ring",    "Oscillators", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Cross-fades osc 3 into ring modulation with osc 1.") \
X(SubLevel,      "osc.sub.level",  "Sub Level",     "Oscillators", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Sub-oscillator level (locked to osc 1).") \
X(SubWave,       "osc.sub.wave",   "Sub Wave",      "Oscillators", Choice, 0, 0, 0.f, LIN, "", ZS_CH_SUB_WAVE, "Sub-oscillator waveform and octave.") \
X(NoiseLevel,    "osc.noise.level","Noise Level",   "Oscillators", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Noise generator level.") \
X(NoiseType,     "osc.noise.type", "Noise Type",    "Oscillators", Choice, 0, 0, 0.f, LIN, "", ZS_CH_NOISE, "Noise colour.") \
X(Drift,         "osc.drift",      "Drift",         "Oscillators", Float, 0.f, 1.f, 0.15f, LIN, "", nullptr, "Slow per-voice oscillator pitch wander.") \
X(MixDrive,      "mix.drive",      "Mixer Drive",   "Oscillators", Float, 0.f, 1.f, 0.1f, LIN, "", nullptr, "Mixer overload: from gentle warmth to dense saturation.") \
/* -------------------------------------------------------------- Wavetable */ \
X(WtTable,       "wt.table",       "Wavetable",     "Wavetable", Choice, 0, 0, 0.f, LIN, "", ZS_CH_WAVETABLE, "Wavetable used by oscillators set to the Wavetable wave.") \
X(WtPos,         "wt.pos",         "WT Position",   "Wavetable", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Scan position within the wavetable.") \
X(WtSpread,      "wt.spread",      "WT Spread",     "Wavetable", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Offsets the scan position of osc 2 (+) and osc 3 (++).") \
/* ----------------------------------------------------------------- Filter */ \
X(FltModel,      "flt.model",      "Filter Model",  "Filter", Choice, 0, 0, 0.f, LIN, "", ZS_CH_FILTER_MODEL, "Ladder: transistor ladder. OTA: cascade with soft feedback. SEM: state-variable. K35: aggressive Sallen-Key.") \
X(FltType,       "flt.type",       "Filter Type",   "Filter", Choice, 0, 0, 0.f, LIN, "", ZS_CH_FILTER_TYPE, "Filter response.") \
X(FltSlope,      "flt.slope",      "Filter Slope",  "Filter", Choice, 0, 0, 1.f, LIN, "", ZS_CH_FILTER_SLOPE, "Filter slope (not used by K35).") \
X(FltCutoff,     "flt.cutoff",     "Cutoff",        "Filter", Float, 20.f, 20000.f, 6000.f, LOG, "Hz", nullptr, "Filter cutoff frequency.") \
X(FltRes,        "flt.res",        "Resonance",     "Filter", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Filter resonance. Self-oscillates near maximum on Ladder, OTA and K35.") \
X(FltDrive,      "flt.drive",      "Filter Drive",  "Filter", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Drives the filter's nonlinear stages.") \
X(FltEnv,        "flt.env",        "Env Amount",    "Filter", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Envelope 2 modulation of cutoff (up to 8 octaves).") \
X(FltEnvInvert,  "flt.envinv",     "Env Invert",    "Filter", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Inverts the envelope polarity.") \
X(FltKey,        "flt.key",        "Key Track",     "Filter", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Cutoff keyboard tracking (100% = 1 octave per octave).") \
X(FltVel,        "flt.vel",        "Velocity",      "Filter", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Velocity modulation of cutoff (up to 4 octaves).") \
X(FltLfo,        "flt.lfo",        "LFO 1 Amount",  "Filter", Float, 0.f, 1.f, 0.f, 0.25f, "", nullptr, "LFO 1 modulation of cutoff (up to 4 octaves).") \
X(FltHpf,        "flt.hpf",        "HPF",           "Filter", Float, 10.f, 4000.f, 10.f, LOG, "Hz", nullptr, "Non-resonant 12 dB high-pass before the main filter.") \
/* -------------------------------------------------------------- Envelopes */ \
X(Env1A,         "env1.a",         "Amp Attack",    "Envelopes", Float, 0.0005f, 20.f, 0.002f, LOG, "s", nullptr, "Amplifier envelope attack time.") \
X(Env1D,         "env1.d",         "Amp Decay",     "Envelopes", Float, 0.001f, 20.f, 0.4f, LOG, "s", nullptr, "Amplifier envelope decay time.") \
X(Env1S,         "env1.s",         "Amp Sustain",   "Envelopes", Float, 0.f, 1.f, 0.8f, LIN, "", nullptr, "Amplifier envelope sustain level.") \
X(Env1R,         "env1.r",         "Amp Release",   "Envelopes", Float, 0.001f, 20.f, 0.25f, LOG, "s", nullptr, "Amplifier envelope release time.") \
X(Env1Vel,       "env1.vel",       "Amp Velocity",  "Envelopes", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Velocity sensitivity of the amplifier.") \
X(Env2A,         "env2.a",         "Filter Attack", "Envelopes", Float, 0.0005f, 20.f, 0.002f, LOG, "s", nullptr, "Envelope 2 attack time.") \
X(Env2D,         "env2.d",         "Filter Decay",  "Envelopes", Float, 0.001f, 20.f, 0.5f, LOG, "s", nullptr, "Envelope 2 decay time.") \
X(Env2S,         "env2.s",         "Filter Sustain","Envelopes", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "Envelope 2 sustain level.") \
X(Env2R,         "env2.r",         "Filter Release","Envelopes", Float, 0.001f, 20.f, 0.3f, LOG, "s", nullptr, "Envelope 2 release time.") \
X(Env2Vel,       "env2.vel",       "Env 2 Velocity","Envelopes", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Velocity scaling of envelope 2's output.") \
X(Env3A,         "env3.a",         "Mod Attack",    "Envelopes", Float, 0.0005f, 20.f, 0.001f, LOG, "s", nullptr, "Modulation envelope attack time.") \
X(Env3D,         "env3.d",         "Mod Decay",     "Envelopes", Float, 0.001f, 20.f, 0.2f, LOG, "s", nullptr, "Modulation envelope decay time.") \
X(Env3S,         "env3.s",         "Mod Sustain",   "Envelopes", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Modulation envelope sustain level.") \
X(Env3R,         "env3.r",         "Mod Release",   "Envelopes", Float, 0.001f, 20.f, 0.2f, LOG, "s", nullptr, "Modulation envelope release time.") \
X(Env3Amt,       "env3.amt",       "Mod Env Amount","Envelopes", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Depth of envelope 3's direct route.") \
X(Env3Dest,      "env3.dest",      "Mod Env Dest",  "Envelopes", Choice, 0, 0, 0.f, LIN, "", ZS_CH_ENV3_DEST, "Direct destination for envelope 3 (it is also a matrix source).") \
/* ------------------------------------------------------------------- LFOs */ \
ZS_LFO_PARAMS(X, 1, 0.f) \
ZS_LFO_PARAMS(X, 2, 0.f) \
/* ------------------------------------------------------------------ Voice */ \
X(AmpLevel,      "amp.level",      "Amp Level",     "Voice", Float, 0.f, 1.f, 0.8f, LIN, "", nullptr, "Voice amplifier level.") \
X(AmpPan,        "amp.pan",        "Pan",           "Voice", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Voice pan position.") \
X(VoiceMode,     "voice.mode",     "Voice Mode",    "Voice", Choice, 0, 0, 0.f, LIN, "", ZS_CH_VOICE_MODE, "Poly, Mono (retriggers), or Legato (mono, no retrigger when notes overlap).") \
X(VoicePriority, "voice.priority", "Note Priority", "Voice", Choice, 0, 0, 0.f, LIN, "", ZS_CH_PRIORITY, "Which held note sounds in mono modes.") \
X(VoiceAlloc,    "voice.alloc",    "Allocation",    "Voice", Choice, 0, 0, 0.f, LIN, "", ZS_CH_ALLOC, "Rotate cycles voices (vintage behaviour); Reuse re-plays the same voice per note; Lowest always fills from voice 1.") \
X(Polyphony,     "voice.poly",     "Polyphony",     "Voice", Int, 1.f, 64.f, 16.f, LIN, "", nullptr, "Maximum number of physical voices (unison voices count individually).") \
X(Unison,        "voice.unison",   "Unison",        "Voice", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Stacks several detuned voices per note.") \
X(UnisonVoices,  "voice.univoices","Unison Voices", "Voice", Int, 2.f, 8.f, 4.f, LIN, "", nullptr, "Voices stacked per note when unison is on.") \
X(UnisonDetune,  "voice.detune",   "Detune",        "Voice", Float, 0.f, 1.f, 0.25f, LIN, "", nullptr, "Unison detune spread (up to +/-50 cents).") \
X(UnisonSpread,  "voice.spread",   "Stereo Spread", "Voice", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Stereo spread of voices.") \
X(GlideTime,     "voice.glide",    "Glide",         "Voice", Float, 0.f, 5.f, 0.f, 0.3f, "s", nullptr, "Portamento time. 0 = off.") \
X(GlideMode,     "voice.glidemode","Glide Mode",    "Voice", Choice, 0, 0, 0.f, LIN, "", ZS_CH_GLIDE_MODE, "Glide on every note, or only between overlapping (legato) notes.") \
X(VelCurve,      "voice.velcurve", "Velocity Curve","Voice", Choice, 0, 0, 1.f, LIN, "", ZS_CH_VEL_CURVE, "Velocity response curve.") \
X(Vintage,       "voice.vintage",  "Vintage",       "Voice", Float, 0.f, 1.f, 0.2f, LIN, "", nullptr, "Per-voice variation of tuning, filter, level and phase.") \
X(BendRange,     "voice.bend",     "Bend Range",    "Voice", Int, 0.f, 24.f, 2.f, LIN, "st", nullptr, "Pitch-bend range in semitones.") \
/* --------------------------------------------------------------------- FM */ \
X(FmAlgo,        "fm.algo",        "FM Algorithm",  "FM", Choice, 0, 0, 2.f, LIN, "", ZS_CH_FM_ALGO, "Operator routing algorithm.") \
X(FmDepth,       "fm.depth",       "FM Depth",      "FM", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Global modulation index scaling of all modulators.") \
X(FmFeedback,    "fm.feedback",    "FM Feedback",   "FM", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Operator 6 self-feedback.") \
X(FmLevel,       "fm.level",       "FM Level",      "FM", Float, 0.f, 1.f, 0.8f, LIN, "", nullptr, "FM engine output level into the mixer.") \
X(FmVel,         "fm.vel",         "FM Velocity",   "FM", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Velocity sensitivity of the modulation index (brighter when played harder).") \
X(FmDx,          "fm.dx",          "FM DX Mode",    "FM", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "DX-style operator maths: logarithmic levels (0.75 dB steps), envelopes that move in decibels, DX index and feedback scaling.") \
ZS_OP_PARAMS(X, 1, 1.f, 1.0f) \
ZS_OP_PARAMS(X, 2, 1.f, 0.5f) \
ZS_OP_PARAMS(X, 3, 1.f, 1.0f) \
ZS_OP_PARAMS(X, 4, 1.f, 0.5f) \
ZS_OP_PARAMS(X, 5, 1.f, 1.0f) \
ZS_OP_PARAMS(X, 6, 1.f, 0.5f) \
/* ------------------------------------------------------------- Mod matrix */ \
ZS_MOD_PARAMS(X, 1) ZS_MOD_PARAMS(X, 2) ZS_MOD_PARAMS(X, 3) ZS_MOD_PARAMS(X, 4) \
ZS_MOD_PARAMS(X, 5) ZS_MOD_PARAMS(X, 6) ZS_MOD_PARAMS(X, 7) ZS_MOD_PARAMS(X, 8) \
/* ------------------------------------------------------------ Arpeggiator */ \
X(ArpOn,         "arp.on",         "Arp On",        "Arpeggiator", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the arpeggiator.") \
X(ArpMode,       "arp.mode",       "Arp Mode",      "Arpeggiator", Choice, 0, 0, 0.f, LIN, "", ZS_CH_ARP_MODE, "Arpeggio note order.") \
X(ArpDiv,        "arp.div",        "Arp Rate",      "Arpeggiator", Choice, 0, 0, 13.f, LIN, "", ZS_CH_DIVISION, "Arpeggiator step length (tempo synchronised).") \
X(ArpOctaves,    "arp.oct",        "Arp Octaves",   "Arpeggiator", Int, 1.f, 4.f, 1.f, LIN, "", nullptr, "Octave range of the arpeggio.") \
X(ArpGate,       "arp.gate",       "Arp Gate",      "Arpeggiator", Float, 0.05f, 1.f, 0.5f, LIN, "", nullptr, "Note length as a fraction of the step.") \
X(ArpSwing,      "arp.swing",      "Swing",         "Arpeggiator", Float, 0.f, 0.75f, 0.f, LIN, "", nullptr, "Delays every second step (arpeggiator and sequencer).") \
X(ArpHold,       "arp.hold",       "Arp Hold",      "Arpeggiator", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Latches the chord after the keys are released.") \
/* -------------------------------------------------------------- Sequencer */ \
X(SeqMode,       "seq.mode",       "Seq Mode",      "Sequencer", Choice, 0, 0, 0.f, LIN, "", ZS_CH_SEQ_MODE, "Mod: steps are a modulation source. Note: held key plays a transposing mono pattern with accent and slide.") \
X(SeqDiv,        "seq.div",        "Seq Rate",      "Sequencer", Choice, 0, 0, 13.f, LIN, "", ZS_CH_DIVISION, "Sequencer step length (tempo synchronised).") \
X(SeqLength,     "seq.len",        "Seq Length",    "Sequencer", Int, 1.f, 16.f, 16.f, LIN, "", nullptr, "Number of active steps.") \
X(SeqGate,       "seq.gate",       "Seq Gate",      "Sequencer", Float, 0.05f, 1.f, 0.6f, LIN, "", nullptr, "Note length in Note mode.") \
ZS_SEQ_STEPS(X) \
/* ---------------------------------------------------------------- Effects */ \
X(DriveOn,       "fx.drive.on",    "Drive On",      "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the drive effect.") \
X(DriveType,     "fx.drive.type",  "Drive Type",    "Effects", Choice, 0, 0, 0.f, LIN, "", ZS_CH_DRIVE_TYPE, "Overdrive (asymmetric soft), Distortion (hard), Fold (wavefolder).") \
X(DriveAmount,   "fx.drive.amt",   "Drive",         "Effects", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "Drive amount.") \
X(DriveTone,     "fx.drive.tone",  "Drive Tone",    "Effects", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "Post-drive tone (dark to bright).") \
X(DriveMix,      "fx.drive.mix",   "Drive Mix",     "Effects", Float, 0.f, 1.f, 1.f, LIN, "", nullptr, "Drive wet/dry balance.") \
X(ChorusOn,      "fx.cho.on",      "Chorus On",     "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the stereo chorus.") \
X(ChorusRate,    "fx.cho.rate",    "Chorus Rate",   "Effects", Float, 0.05f, 8.f, 0.5f, LOG, "Hz", nullptr, "Chorus modulation rate.") \
X(ChorusDepth,   "fx.cho.depth",   "Chorus Depth",  "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Chorus modulation depth.") \
X(ChorusMix,     "fx.cho.mix",     "Chorus Mix",    "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Chorus wet/dry balance.") \
X(EnsOn,         "fx.ens.on",      "Ensemble On",   "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the three-phase string ensemble.") \
X(EnsRate,       "fx.ens.rate",    "Ensemble Rate", "Effects", Float, 0.1f, 3.f, 0.6f, LOG, "Hz", nullptr, "Ensemble slow modulation rate.") \
X(EnsDepth,      "fx.ens.depth",   "Ensemble Depth","Effects", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "Ensemble modulation depth.") \
X(EnsMix,        "fx.ens.mix",     "Ensemble Mix",  "Effects", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "Ensemble wet/dry balance.") \
X(PhaserOn,      "fx.pha.on",      "Phaser On",     "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the 6-stage stereo phaser.") \
X(PhaserRate,    "fx.pha.rate",    "Phaser Rate",   "Effects", Float, 0.02f, 8.f, 0.3f, LOG, "Hz", nullptr, "Phaser sweep rate.") \
X(PhaserDepth,   "fx.pha.depth",   "Phaser Depth",  "Effects", Float, 0.f, 1.f, 0.7f, LIN, "", nullptr, "Phaser sweep range.") \
X(PhaserFb,      "fx.pha.fb",      "Phaser Feedback","Effects", Float, 0.f, 0.95f, 0.5f, LIN, "", nullptr, "Phaser resonance.") \
X(PhaserMix,     "fx.pha.mix",     "Phaser Mix",    "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Phaser wet/dry balance.") \
X(FlangerOn,     "fx.fla.on",      "Flanger On",    "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the stereo flanger.") \
X(FlangerRate,   "fx.fla.rate",    "Flanger Rate",  "Effects", Float, 0.02f, 8.f, 0.2f, LOG, "Hz", nullptr, "Flanger sweep rate.") \
X(FlangerDepth,  "fx.fla.depth",   "Flanger Depth", "Effects", Float, 0.f, 1.f, 0.7f, LIN, "", nullptr, "Flanger sweep range.") \
X(FlangerFb,     "fx.fla.fb",      "Flanger Feedback","Effects", Float, -0.95f, 0.95f, 0.5f, LIN, "", nullptr, "Flanger feedback (negative = hollow).") \
X(FlangerMix,    "fx.fla.mix",     "Flanger Mix",   "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Flanger wet/dry balance.") \
X(DelayOn,       "fx.dly.on",      "Delay On",      "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the stereo delay.") \
X(DelaySync,     "fx.dly.sync",    "Delay Sync",    "Effects", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Uses the tempo division instead of milliseconds.") \
X(DelayTime,     "fx.dly.time",    "Delay Time",    "Effects", Float, 1.f, 2000.f, 350.f, LOG, "ms", nullptr, "Delay time when sync is off.") \
X(DelayDiv,      "fx.dly.div",     "Delay Division","Effects", Choice, 0, 0, 9.f, LIN, "", ZS_CH_DIVISION, "Delay time when sync is on.") \
X(DelayFb,       "fx.dly.fb",      "Delay Feedback","Effects", Float, 0.f, 0.95f, 0.35f, LIN, "", nullptr, "Delay feedback.") \
X(DelayTone,     "fx.dly.tone",    "Delay Tone",    "Effects", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "Darkens (left) or thins (right) the repeats.") \
X(DelayPingPong, "fx.dly.pp",      "Ping-Pong",     "Effects", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Alternates repeats between left and right.") \
X(DelayMix,      "fx.dly.mix",     "Delay Mix",     "Effects", Float, 0.f, 1.f, 0.25f, LIN, "", nullptr, "Delay wet/dry balance.") \
X(DelayType,     "fx.dly.type",    "Delay Type",    "Effects", Choice, 0, 0, 0.f, LIN, "", ZS_CH_DELAY_TYPE, "Delay character: clean digital, RE-201 / EP-3 tape echo, DM-2 bucket-brigade, diffused or lo-fi.") \
X(DelayWow,      "fx.dly.wow",     "Delay Wow",     "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Slow pitch drift of the repeats (0.5 = the type's natural amount).") \
X(DelayFlutter,  "fx.dly.flutter", "Delay Flutter", "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Fast pitch wobble of the repeats (0.5 = the type's natural amount).") \
X(DelayAge,      "fx.dly.age",     "Delay Age",     "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Wear: saturation, top-end loss and noise of the repeats (0.5 = the type's natural amount).") \
X(DelaySpread,   "fx.dly.spread",  "Delay Spread",  "Effects", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Lengthens the right-hand delay for a wider stereo image.") \
X(DelayDuck,     "fx.dly.duck",    "Delay Ducking", "Effects", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Turns the echoes down while you play and lets them bloom in the gaps.") \
X(ReverbOn,      "fx.rev.on",      "Reverb On",     "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Enables the stereo reverb.") \
X(ReverbSize,    "fx.rev.size",    "Reverb Size",   "Effects", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "Reverb size and decay time.") \
X(ReverbDamp,    "fx.rev.damp",    "Reverb Damping","Effects", Float, 0.f, 1.f, 0.4f, LIN, "", nullptr, "High-frequency damping of the reverb tail.") \
X(ReverbPredelay,"fx.rev.pre",     "Pre-Delay",     "Effects", Float, 0.f, 200.f, 20.f, LIN, "ms", nullptr, "Delay before the reverb onset.") \
X(ReverbMix,     "fx.rev.mix",     "Reverb Mix",    "Effects", Float, 0.f, 1.f, 0.25f, LIN, "", nullptr, "Reverb wet/dry balance.") \
X(ReverbType,    "fx.rev.type",    "Reverb Type",   "Effects", Choice, 0, 0, 0.f, LIN, "", ZS_CH_REVERB_TYPE, "Reverb algorithm and character.") \
X(ReverbDecay,   "fx.rev.decay",   "Reverb Decay",  "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Decay time trim (0.5 = as set by SIZE; x0.35 .. x2.8).") \
X(ReverbLowCut,  "fx.rev.lowcut",  "Reverb Low Cut","Effects", Float, 20.f, 1000.f, 20.f, LOG, "Hz", nullptr, "Reverb EQ high-pass, 12 dB/oct: keeps the low end clear (fully left = off).") \
X(ReverbDiff,    "fx.rev.diff",    "Reverb Diffusion","Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Echo density: low is grainier, high is smoother.") \
X(ReverbMod,     "fx.rev.mod",     "Reverb Modulation","Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Chorusing of the tail (0.5 = the type's natural amount).") \
X(ReverbEarly,   "fx.rev.early",   "Reverb Early/Late","Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Balance of early reflections (left) and tail (right).") \
X(ReverbWidth,   "fx.rev.width",   "Reverb Width",  "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Stereo width of the reverb (0.5 = natural, 1 = extra wide).") \
X(RevEqLowFreq,  "fx.rev.eq.lsf",  "Reverb EQ Low Freq",  "Effects", Float, 30.f, 500.f, 120.f, LOG, "Hz", nullptr, "Reverb EQ low-shelf frequency.") \
X(RevEqLowGain,  "fx.rev.eq.lsg",  "Reverb EQ Low Gain",  "Effects", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Reverb EQ low-shelf gain (wet signal only).") \
X(RevEqB1Freq,   "fx.rev.eq.p1f",  "Reverb EQ Bell 1 Freq","Effects", Float, 80.f, 2000.f, 400.f, LOG, "Hz", nullptr, "Reverb EQ bell 1 frequency.") \
X(RevEqB1Gain,   "fx.rev.eq.p1g",  "Reverb EQ Bell 1 Gain","Effects", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Reverb EQ bell 1 gain.") \
X(RevEqB1Q,      "fx.rev.eq.p1q",  "Reverb EQ Bell 1 Q",  "Effects", Float, 0.3f, 8.f, 1.f, LOG, "Q", nullptr, "Reverb EQ bell 1 bandwidth (higher Q = narrower).") \
X(RevEqB2Freq,   "fx.rev.eq.p2f",  "Reverb EQ Bell 2 Freq","Effects", Float, 500.f, 12000.f, 2500.f, LOG, "Hz", nullptr, "Reverb EQ bell 2 frequency.") \
X(RevEqB2Gain,   "fx.rev.eq.p2g",  "Reverb EQ Bell 2 Gain","Effects", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Reverb EQ bell 2 gain.") \
X(RevEqB2Q,      "fx.rev.eq.p2q",  "Reverb EQ Bell 2 Q",  "Effects", Float, 0.3f, 8.f, 1.f, LOG, "Q", nullptr, "Reverb EQ bell 2 bandwidth (higher Q = narrower).") \
X(RevEqHighFreq, "fx.rev.eq.hsf",  "Reverb EQ High Freq", "Effects", Float, 1500.f, 16000.f, 8000.f, LOG, "Hz", nullptr, "Reverb EQ high-shelf frequency.") \
X(RevEqHighGain, "fx.rev.eq.hsg",  "Reverb EQ High Gain", "Effects", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Reverb EQ high-shelf gain (wet signal only).") \
X(RevEqLpf,      "fx.rev.eq.lpf",  "Reverb EQ LPF",       "Effects", Float, 1000.f, 20000.f, 20000.f, LOG, "Hz", nullptr, "Reverb EQ low-pass, 12 dB/oct (fully right = off).") \
/* ----------------------------------------------------------------- Output */ \
X(Width,         "out.width",      "Stereo Width",  "Output", Float, 0.f, 2.f, 1.f, LIN, "", nullptr, "Mid/side stereo width (0 = mono, 1 = unchanged, 2 = wide).") \
X(Character,     "out.character",  "Character",     "Output", Float, 0.f, 1.f, 0.1f, LIN, "", nullptr, "Analogue-style output stage saturation and warmth.") \
X(Macro,         "out.macro",      "Macro",         "Output", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Performance macro: a modulation-matrix source.") \
X(Limiter,       "out.limiter",    "Limiter",       "Output", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Output safety limiter (keeps peaks below 0 dBFS).") \
X(Smooth,        "out.smooth",     "Smooth",        "Output", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "SMOOTH: fixed 2.2 kHz dynamic compressor (4:1) that tames harshness only when it appears. AMOUNT lowers the threshold; 0 = off.") \
/* ------------------------------------- Super saws, wave sequence, vector */ \
X(SmoothOn,      "out.smooth.on",  "Smooth On",     "Output", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Switches the SMOOTH dynamic compressor in or out.") \
X(Osc1Detune,    "osc1.detune",    "Osc 1 Detune",  "Oscillators", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "SuperSaw / HyperSaw: spread of the detuned saws.") \
X(Osc1SuperMix,  "osc1.smix",      "Osc 1 Saw Mix", "Oscillators", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "SuperSaw / HyperSaw: level of the side saws against the centre one.") \
X(Osc2Detune,    "osc2.detune",    "Osc 2 Detune",  "Oscillators", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "SuperSaw / HyperSaw: spread of the detuned saws.") \
X(Osc2SuperMix,  "osc2.smix",      "Osc 2 Saw Mix", "Oscillators", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "SuperSaw / HyperSaw: level of the side saws against the centre one.") \
X(Osc3Detune,    "osc3.detune",    "Osc 3 Detune",  "Oscillators", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "SuperSaw / HyperSaw: spread of the detuned saws.") \
X(Osc3SuperMix,  "osc3.smix",      "Osc 3 Saw Mix", "Oscillators", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "SuperSaw / HyperSaw: level of the side saws against the centre one.") \
X(WtSeqMode,     "wt.seq.mode",    "Wave Seq",      "Wavetable", Choice, 0, 0, 0.f, LIN, "", ZS_CH_WAVE_SEQ, "Wave sequence: steps through the wavetable's frames from each note-on (Wavestation style).") \
X(WtSeqSteps,    "wt.seq.steps",   "Seq Steps",     "Wavetable", Int, 2.f, 32.f, 8.f, LIN, "", nullptr, "Number of waves in the sequence, spread evenly through the table from POS.") \
X(WtSeqTime,     "wt.seq.time",    "Seq Step Time", "Wavetable", Float, 0.02f, 4.f, 0.25f, LOG, "s", nullptr, "Duration of each step (when not synced).") \
X(WtSeqSync,     "wt.seq.sync",    "Seq Sync",      "Wavetable", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Locks the step duration to the tempo.") \
X(WtSeqDiv,      "wt.seq.div",     "Seq Division",  "Wavetable", Choice, 0, 0, 13.f, LIN, "", ZS_CH_DIVISION, "Step duration when synced.") \
X(WtSeqXfade,    "wt.seq.xfade",   "Seq Crossfade", "Wavetable", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "Crossfade between steps: 0 = hard switching (rhythmic), 1 = continuous blending (pads).") \
X(VecOn,         "vec.on",         "Vector On",     "Vector", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Vector mixing: the joystick balances osc 1 (A), osc 2 (B), osc 3 (C) and the FM part (D).") \
X(VecX,          "vec.x",          "Vector X",      "Vector", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Joystick left (A) to right (C).") \
X(VecY,          "vec.y",          "Vector Y",      "Vector", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Joystick bottom (D) to top (B).") \
X(ChorusType,    "fx.cho.type",    "Chorus Type",   "Effects", Choice, 0, 0, 0.f, LIN, "", ZS_CH_CHORUS_TYPE, "Chorus voicing: classic analogue BBD, or the digital choruses of the 90s workstations.") \
X(XfmrOn,        "out.xfmr.on",    "Transformer On","Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Output transformer stage after the valve.") \
X(XfmrType,      "out.xfmr.type",  "Transformer",   "Output", Choice, 0, 0, 0.f, LIN, "", ZS_CH_XFMR_TYPE, "Neve LO1166 (Marinair, 1073 output): thick lows, soft top. API 2503: punchy, wide. UTC A-24: tube-era, warm, earliest to saturate. Haufe V76: German broadcast, rich lows, a little 2nd harmonic. Jensen JT-11: nickel core, clean and open.") \
X(XfmrAmount,    "out.xfmr.amt",   "Transformer Amount","Output", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Level into the transformer: 50 % = nominal (+12 dBu for -6 dBFS), up to +18 dB hotter. Level-compensated.") \
X(XfmrHot,       "out.xfmr.hot",   "Transformer +HOT","Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "+HOT: beyond real-world levels. AMOUNT drives up to +36 dB (bass and low mids saturate and compress, as when a console is slammed) and each type's voicing (low-end bump, midrange, top) is exaggerated.") \
X(ValveOn,       "out.valve.on",   "Valve On",      "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Valve output stage (oversampled triode / power-valve saturation).") \
X(ValveType,     "out.valve.type", "Valve Type",    "Output", Choice, 0, 0, 0.f, LIN, "", ZS_CH_VALVE_TYPE, "12AX7: high-gain preamp triode (even harmonics). 12AU7: low-gain, clean warmth. EL84: push-pull power pair (odd harmonics, early compression). 6L6: big, smooth power valve with more headroom.") \
X(ValveDrive,    "out.valve.drive","Valve Drive",   "Output", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "How hard the valve is driven: warmth and gentle peak compression up to about 80 %, audible distortion from about 90 %.") \
X(ValveBias,     "out.valve.bias", "Valve Bias",    "Output", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Operating point: cold (low) is cleaner and more symmetric, hot (high) adds even harmonics and grit.") \
X(ValveSag,      "out.valve.sag",  "Valve Sag",     "Output", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "Power-supply sag: the valve compresses and blooms on loud notes.") \
X(ValveWarmth,   "out.valve.warmth","Valve Warmth", "Output", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Output transformer: rounds off the top end and adds low-end weight.") \
X(ValveMix,      "out.valve.mix",  "Valve Mix",     "Output", Float, 0.f, 1.f, 1.f, LIN, "", nullptr, "Dry/valve blend.") \
X(CompOn,        "comp.on",        "Comp On",       "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Output compressor (after the valve stage, before the master).") \
X(CompPre,       "comp.pre",       "Comp Pre FX",   "Output", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "PRE FX: compress before the delay and reverb (the dry synth only), instead of the whole mix at the end.") \
X(CompModel,     "comp.model",     "Comp Model",    "Output", Choice, 0, 0, 1.f, LIN, "", ZS_CH_COMP_MODEL, "LA-2A optical leveller, SSL-style VCA bus compressor, 1176-style FET limiter or Fairchild-style variable-mu.") \
X(CompThresh,    "comp.thresh",    "Comp Threshold","Output", Float, -40.f, 0.f, -12.f, LIN, "dB", nullptr, "Level where compression starts (on the LA-2A this is PEAK REDUCTION, on the 1176 the INPUT drive).") \
X(CompMakeup,    "comp.makeup",    "Comp Makeup",   "Output", Float, 0.f, 24.f, 0.f, LIN, "dB", nullptr, "Output gain after compression (the LA-2A's GAIN, the 1176's OUTPUT).") \
X(CompMix,       "comp.mix",       "Comp Mix",      "Output", Float, 0.f, 1.f, 1.f, LIN, "", nullptr, "Parallel (New York) compression: blends the dry signal back in.") \
X(CompRatio,     "comp.ratio",     "Comp Ratio",    "Output", Choice, 0, 0, 3.f, LIN, "", ZS_CH_COMP_RATIO, "Compression ratio. LA-2A: up to 4:1 = COMPRESS, 8:1 and above = LIMIT. 1176: All = all buttons in.") \
X(CompAttack,    "comp.attack",    "Comp Attack",   "Output", Float, 0.02f, 100.f, 10.f, LOG, "ms", nullptr, "Attack time (SSL 0.1-30 ms, 1176 0.02-0.8 ms, Fairchild 0.2-2 ms; the LA-2A sets its own).") \
X(CompRelease,   "comp.release",   "Comp Release",  "Output", Float, 50.f, 3000.f, 300.f, LOG, "ms", nullptr, "Release time (SSL 0.1-1.2 s, 1176 50-1100 ms, Fairchild 0.3-3 s; the LA-2A's is program dependent).") \
X(CompAutoRel,   "comp.autorel",   "Comp Auto Release","Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "SSL AUTO release: fast after transients, slow after sustained compression.") \
X(CompKnee,      "comp.knee",      "Comp Knee",     "Output", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "Knee softness (SSL).") \
X(CompEmph,      "comp.emph",      "Comp Emphasis", "Output", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "LA-2A only: side-chain HF emphasis, so bright and harsh sounds are compressed harder.") \
X(CompColour,    "comp.color",     "Comp Colour",   "Output", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Analogue colour of the model (valve / FET / VCA saturation): gentle at 0.5, clearly driven at full.") \
X(PeqOn,         "peq.on",         "Output EQ On",  "Output", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Parametric output EQ in/out (flat with both cuts off = completely dry).") \
X(PeqHpf,        "peq.hpf",        "Output EQ Low Cut", "Output", Float, 20.f, 2000.f, 20.f, LOG, "Hz", nullptr, "12 dB/oct low cut (high-pass). Fully left = off.") \
X(PeqLpf,        "peq.lpf",        "Output EQ High Cut", "Output", Float, 1000.f, 20000.f, 20000.f, LOG, "Hz", nullptr, "12 dB/oct high cut (low-pass). Fully right = off.") \
X(PeqB1Freq,     "peq.b1.f",       "Output EQ 1 Freq", "Output", Float, 20.f, 20000.f, 80.f, LOG, "Hz", nullptr, "Output EQ band 1 centre frequency.") \
X(PeqB1Gain,     "peq.b1.g",       "Output EQ 1 Gain", "Output", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Output EQ band 1 boost / cut.") \
X(PeqB1Q,        "peq.b1.q",       "Output EQ 1 Q", "Output", Float, 0.3f, 10.f, 0.9f, LOG, "", nullptr, "Output EQ band 1 width: low = broad, high = narrow.") \
X(PeqB2Freq,     "peq.b2.f",       "Output EQ 2 Freq", "Output", Float, 20.f, 20000.f, 250.f, LOG, "Hz", nullptr, "Output EQ band 2 centre frequency.") \
X(PeqB2Gain,     "peq.b2.g",       "Output EQ 2 Gain", "Output", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Output EQ band 2 boost / cut.") \
X(PeqB2Q,        "peq.b2.q",       "Output EQ 2 Q", "Output", Float, 0.3f, 10.f, 0.9f, LOG, "", nullptr, "Output EQ band 2 width: low = broad, high = narrow.") \
X(PeqB3Freq,     "peq.b3.f",       "Output EQ 3 Freq", "Output", Float, 20.f, 20000.f, 1000.f, LOG, "Hz", nullptr, "Output EQ band 3 centre frequency.") \
X(PeqB3Gain,     "peq.b3.g",       "Output EQ 3 Gain", "Output", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Output EQ band 3 boost / cut.") \
X(PeqB3Q,        "peq.b3.q",       "Output EQ 3 Q", "Output", Float, 0.3f, 10.f, 0.9f, LOG, "", nullptr, "Output EQ band 3 width: low = broad, high = narrow.") \
X(PeqB4Freq,     "peq.b4.f",       "Output EQ 4 Freq", "Output", Float, 20.f, 20000.f, 3500.f, LOG, "Hz", nullptr, "Output EQ band 4 centre frequency.") \
X(PeqB4Gain,     "peq.b4.g",       "Output EQ 4 Gain", "Output", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Output EQ band 4 boost / cut.") \
X(PeqB4Q,        "peq.b4.q",       "Output EQ 4 Q", "Output", Float, 0.3f, 10.f, 0.9f, LOG, "", nullptr, "Output EQ band 4 width: low = broad, high = narrow.") \
X(PeqB5Freq,     "peq.b5.f",       "Output EQ 5 Freq", "Output", Float, 20.f, 20000.f, 10000.f, LOG, "Hz", nullptr, "Output EQ band 5 centre frequency.") \
X(PeqB5Gain,     "peq.b5.g",       "Output EQ 5 Gain", "Output", Float, -15.f, 15.f, 0.f, LIN, "dB", nullptr, "Output EQ band 5 boost / cut.") \
X(PeqB5Q,        "peq.b5.q",       "Output EQ 5 Q", "Output", Float, 0.3f, 10.f, 0.9f, LOG, "", nullptr, "Output EQ band 5 width: low = broad, high = narrow.") \
X(PeqOutput,     "peq.out",        "Output EQ Output", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "Output EQ level trim.") \
X(PeqPre,        "peq.pre",        "Output EQ Pre FX", "Output", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Shown as POST FX on the panel. Off (the default, PRE): the EQ works on the synth before the delay and reverb (and before the compressor when that is PRE FX too). POST FX: the EQ works on the whole output.") \
/* --------------------------------------------- Pitch track (audio in) */ \
X(TrkOn,         "trk.on",         "Pitch Track",   "Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "PITCH TRACK: play ZYNTH from a guitar or bass on the audio input (one note at a time). The keyboards are disabled while it is on.") \
X(TrkInput,      "trk.input",      "Track Input",   "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_TRK_INPUT, "Which audio input the instrument is plugged into.") \
X(TrkGain,       "trk.gain",       "Track Input Gain", "Global", Float, -12.f, 36.f, 0.f, LIN, "dB", nullptr, "Input level into the tracker. Aim for strong picking to light the meter well above the gate.") \
X(TrkGate,       "trk.gate",       "Track Gate",    "Global", Float, -80.f, -10.f, -50.f, LIN, "dB", nullptr, "GATE: the level that starts a note; notes end 6 dB below it. Raise it if hum or string noise triggers notes.") \
X(TrkThresh,     "trk.thresh",     "Track Threshold", "Global", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "TRACKING: how sure the pitch detector must be before it plays. Loose = fastest, may glitch; strict = steadier, slightly slower.") \
X(TrkRange,      "trk.range",      "Track Range",   "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_TRK_RANGE, "Guitar (70 Hz - 1.4 kHz) or Bass (28 - 450 Hz).") \
X(TrkBend,       "trk.bend",       "Track Bend",    "Global", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "BEND FOLLOW: string bends and vibrato become pitch bend (within the synth's BEND range). Off = notes snap to semitones.") \
X(TrkVel,        "trk.vel",        "Track Velocity", "Global", Float, 0.f, 1.f, 0.7f, LIN, "", nullptr, "VELOCITY: how much your picking strength sets the note velocity (0 = always 100).") \
X(TrkDry,        "trk.dry",        "Track Dry",     "Global", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "DRY: blends the instrument's own sound in with the synth.") \
/* ---------------------------------------- classic EQs and tape */ \
X(EqModel,       "eq.model",      "EQ Model", "Output", Choice, 0, 0, 0.f, LIN, "", ZS_CH_EQ_MODEL, "EQ MODEL: Pristine (clean parametric) or a classic hardware EQ.") \
X(EqDrive,       "eq.drive",      "EQ Drive", "Output", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "DRIVE: pushes the classic EQ's amplifier and transformers (level-compensated). Not on Pristine.") \
X(PulLowFreq,    "pul.lf",        "Pultec Low Freq", "Output", Choice, 0, 0, 2.f, LIN, "", ZS_CH_PUL_LF, "Pultec LOW FREQUENCY (cps).") \
X(PulLowBoost,   "pul.lb",        "Pultec Low Boost", "Output", Float, 0.f, 10.f, 0.f, LIN, "", nullptr, "Pultec LOW BOOST (0-10).") \
X(PulLowAtten,   "pul.la",        "Pultec Low Atten", "Output", Float, 0.f, 10.f, 0.f, LIN, "", nullptr, "Pultec LOW ATTEN (0-10). With BOOST: the sub lift and low-mid dip of the Pultec trick.") \
X(PulHighFreq,   "pul.hf",        "Pultec High Freq", "Output", Choice, 0, 0, 3.f, LIN, "", ZS_CH_PUL_HF, "Pultec HIGH FREQUENCY (kcs) of the boost.") \
X(PulHighBoost,  "pul.hb",        "Pultec High Boost", "Output", Float, 0.f, 10.f, 0.f, LIN, "", nullptr, "Pultec HIGH BOOST (0-10).") \
X(PulBandwidth,  "pul.bw",        "Pultec Bandwidth", "Output", Float, 0.f, 10.f, 5.f, LIN, "", nullptr, "Pultec BANDWIDTH of the high boost: sharp to broad.") \
X(PulAttenFreq,  "pul.af",        "Pultec Atten Sel", "Output", Choice, 0, 0, 1.f, LIN, "", ZS_CH_PUL_ATT, "Pultec ATTEN SEL: high attenuation frequency (kcs).") \
X(PulHighAtten,  "pul.ha",        "Pultec High Atten", "Output", Float, 0.f, 10.f, 0.f, LIN, "", nullptr, "Pultec HIGH ATTEN (0-10).") \
X(NeveHpf,       "nev.hpf",       "Neve HPF", "Output", Choice, 0, 0, 0.f, LIN, "", ZS_CH_NEVE_HPF, "Neve 1073 high-pass filter, 18 dB/oct.") \
X(NeveLowFreq,   "nev.lf",        "Neve Low Freq", "Output", Choice, 0, 0, 2.f, LIN, "", ZS_CH_NEVE_LF, "Neve 1073 low shelf frequency.") \
X(NeveLowGain,   "nev.lg",        "Neve Low Gain", "Output", Float, -16.f, 16.f, 0.f, LIN, "dB", nullptr, "Neve 1073 low shelf gain.") \
X(NeveMidFreq,   "nev.mf",        "Neve Mid Freq", "Output", Choice, 0, 0, 2.f, LIN, "", ZS_CH_NEVE_MF, "Neve 1073 mid frequency (kHz).") \
X(NeveMidGain,   "nev.mg",        "Neve Mid Gain", "Output", Float, -18.f, 18.f, 0.f, LIN, "dB", nullptr, "Neve 1073 mid gain.") \
X(NeveHighGain,  "nev.hg",        "Neve High Gain", "Output", Float, -16.f, 16.f, 0.f, LIN, "dB", nullptr, "Neve 1073 12 kHz high shelf gain.") \
X(A5LowFreq,     "a5.lf",         "API 550A Low Freq", "Output", Choice, 0, 0, 1.f, LIN, "", ZS_CH_A550_LF, "API 550A low band frequency.") \
X(A5LowGain,     "a5.lg",         "API 550A Low Gain", "Output", Choice, 0, 0, 5.f, LIN, "", ZS_CH_A550_GAIN, "API 550A low band boost / cut (stepped, dB).") \
X(A5MidFreq,     "a5.mf",         "API 550A Mid Freq", "Output", Choice, 0, 0, 2.f, LIN, "", ZS_CH_A550_MF, "API 550A mid band frequency.") \
X(A5MidGain,     "a5.mg",         "API 550A Mid Gain", "Output", Choice, 0, 0, 5.f, LIN, "", ZS_CH_A550_GAIN, "API 550A mid band boost / cut (stepped, dB).") \
X(A5HighFreq,    "a5.hf",         "API 550A High Freq", "Output", Choice, 0, 0, 2.f, LIN, "", ZS_CH_A550_HF, "API 550A high band frequency.") \
X(A5HighGain,    "a5.hg",         "API 550A High Gain", "Output", Choice, 0, 0, 5.f, LIN, "", ZS_CH_A550_GAIN, "API 550A high band boost / cut (stepped, dB).") \
X(A5LowShelf,    "a5.ls",         "API 550A Low Shelf", "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "API 550A low band: SHELF instead of PEAK.") \
X(A5HighShelf,   "a5.hs",         "API 550A High Shelf", "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "API 550A high band: SHELF instead of PEAK.") \
X(A5Bandpass,    "a5.bp",         "API 550A Band Pass", "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "API 550A BAND-PASS filter (50 Hz - 15 kHz).") \
X(B5LowFreq,     "b5.lf",         "API 550B Low Freq", "Output", Choice, 0, 0, 2.f, LIN, "", ZS_CH_B550_LF, "API 550B Low band frequency.") \
X(B5LowGain,     "b5.lg",         "API 550B Low Gain", "Output", Choice, 0, 0, 7.f, LIN, "", ZS_CH_B550_GAIN, "API 550B Low band boost / cut (stepped, dB).") \
X(B5LowMidFreq,  "b5.lmf",        "API 550B LowMid Freq", "Output", Choice, 0, 0, 4.f, LIN, "", ZS_CH_B550_LMF, "API 550B LowMid band frequency.") \
X(B5LowMidGain,  "b5.lmg",        "API 550B LowMid Gain", "Output", Choice, 0, 0, 7.f, LIN, "", ZS_CH_B550_GAIN, "API 550B LowMid band boost / cut (stepped, dB).") \
X(B5HighMidFreq, "b5.hmf",        "API 550B HighMid Freq", "Output", Choice, 0, 0, 2.f, LIN, "", ZS_CH_B550_HMF, "API 550B HighMid band frequency.") \
X(B5HighMidGain, "b5.hmg",        "API 550B HighMid Gain", "Output", Choice, 0, 0, 7.f, LIN, "", ZS_CH_B550_GAIN, "API 550B HighMid band boost / cut (stepped, dB).") \
X(B5HighFreq,    "b5.hf",         "API 550B High Freq", "Output", Choice, 0, 0, 3.f, LIN, "", ZS_CH_B550_HF, "API 550B High band frequency.") \
X(B5HighGain,    "b5.hg",         "API 550B High Gain", "Output", Choice, 0, 0, 7.f, LIN, "", ZS_CH_B550_GAIN, "API 550B High band boost / cut (stepped, dB).") \
X(B5LowShelf,    "b5.ls",         "API 550B Low Shelf", "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "API 550B low band: SHELF instead of PEAK.") \
X(B5HighShelf,   "b5.hs",         "API 550B High Shelf", "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "API 550B high band: SHELF instead of PEAK.") \
X(G560B1,        "g560.b1",       "API 560 31", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 31 Hz band.") \
X(G560B2,        "g560.b2",       "API 560 63", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 63 Hz band.") \
X(G560B3,        "g560.b3",       "API 560 125", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 125 Hz band.") \
X(G560B4,        "g560.b4",       "API 560 250", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 250 Hz band.") \
X(G560B5,        "g560.b5",       "API 560 500", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 500 Hz band.") \
X(G560B6,        "g560.b6",       "API 560 1k", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 1k Hz band.") \
X(G560B7,        "g560.b7",       "API 560 2k", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 2k Hz band.") \
X(G560B8,        "g560.b8",       "API 560 4k", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 4k Hz band.") \
X(G560B9,        "g560.b9",       "API 560 8k", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 8k Hz band.") \
X(G560B10,       "g560.b10",      "API 560 16k", "Output", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "API 560 16k Hz band.") \
X(TapeOn,        "tape.on",       "Tape On", "Effects", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "TAPE: record the mix to a classic tape machine (after the delay and reverb).") \
X(TapeModelP,    "tape.model",    "Tape Machine", "Effects", Choice, 0, 0, 0.f, LIN, "", ZS_CH_TAPE_MODEL, "Tape machine.") \
X(TapeSpeed,     "tape.speed",    "Tape Speed", "Effects", Choice, 0, 0, 1.f, LIN, "", ZS_CH_TAPE_SPEED, "Tape speed: slower = more head bump in the bass, darker top, more wow (cassette: 1 7/8 / 3 3/4 ips).") \
X(TapeDrive,     "tape.drive",    "Tape Drive", "Effects", Float, -12.f, 24.f, 0.f, LIN, "dB", nullptr, "DRIVE: level into the tape (level-compensated). Higher = more saturation and softer highs.") \
X(TapeBias,      "tape.bias",     "Tape Bias", "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "BIAS: 0.5 = calibrated. Under = brighter and dirtier, over = cleaner and darker.") \
X(TapeWow,       "tape.wow",      "Tape Wow/Flutter", "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "WOW / FLUTTER: 0 = none, 0.5 = the machine as specified, 1 = a worn transport.") \
X(TapeHiss,      "tape.hiss",     "Tape Hiss", "Effects", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "HISS: 0 = off, 0.5 = the machine's noise floor.") \
X(TapeOut,       "tape.out",      "Tape Output", "Effects", Float, -12.f, 12.f, 0.f, LIN, "dB", nullptr, "Tape output level.") \
/* ------------------------------------------- Wavetable engine (per osc, motion) */ \
ZS_OSC_WT_PARAMS(X, 1) ZS_OSC_WT_PARAMS(X, 2) ZS_OSC_WT_PARAMS(X, 3) \
X(WtEvoMode,     "wt.evo.mode",   "WT Evolve", "Wavetable", Choice, 0, 0, 0.f, LIN, "", ZS_CH_WT_EVOLVE, "EVOLVE: the scan position travels from every note-on. Sweep = once (then holds), Ping-Pong = back and forth.") \
X(WtEvoAmt,      "wt.evo.amt",    "WT Evolve Amount", "Wavetable", Float, -1.f, 1.f, 0.5f, LIN, "", nullptr, "How far EVOLVE moves the position (negative = backwards).") \
X(WtEvoTime,     "wt.evo.time",   "WT Evolve Time", "Wavetable", Float, 0.1f, 60.f, 8.f, LOG, "s", nullptr, "Time of one EVOLVE sweep (up to a minute, for slowly evolving pads).") \
X(WtDrift,       "wt.drift",      "WT Drift", "Wavetable", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "DRIFT: the position wanders at random, differently for every voice and stack voice (organic movement).") \
X(WtDriftRate,   "wt.drate",      "WT Drift Rate", "Wavetable", Float, 0.02f, 4.f, 0.2f, LOG, "Hz", nullptr, "Speed of the DRIFT wander.") \
/* ------------------------------------ Keyboard split / layer (two complete sounds) */ \
X(SplitMode,     "split.mode",    "Keyboard Mode", "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_SPLIT_MODE, "SINGLE: one sound. SPLIT: the LOWER sound below the split point, the UPPER sound from it up. LAYER: both sounds on every key.") \
X(SplitPoint,    "split.point",   "Split Point", "Global", Int, 12.f, 120.f, 60.f, LIN, "note", nullptr, "The lowest key of the UPPER zone (LEARN: play a key to set it).") \
X(SplitLowOct,   "split.lo.oct",  "Lower Octave", "Global", Int, -3.f, 3.f, 0.f, LIN, "oct", nullptr, "Octave shift of the LOWER sound.") \
X(SplitUpOct,    "split.up.oct",  "Upper Octave", "Global", Int, -3.f, 3.f, 0.f, LIN, "oct", nullptr, "Octave shift of the UPPER sound.") \
X(SplitLowVol,   "split.lo.vol",  "Lower Volume", "Global", Float, -40.f, 6.f, 0.f, LIN, "dB", nullptr, "Level of the LOWER sound.") \
X(SplitUpVol,    "split.up.vol",  "Upper Volume", "Global", Float, -40.f, 6.f, 0.f, LIN, "dB", nullptr, "Level of the UPPER sound.") \
X(TrkClean,      "trk.clean",     "Track Clean Up", "Global", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "CLEAN UP: suppresses tracking glitches (short stray low notes). Only doubtful readings wait to be confirmed: picked, clean notes are never delayed.") \
/* ------------------------------------------------------------- Harmoniser */ \
X(HarmOn,        "harm.on",       "Harmony", "Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "HARMONY: adds up to three notes in key to every note you play (keyboard, MIDI or PITCH TRACK).") \
X(HarmKey,       "harm.key",      "Harmony Key", "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_KEY, "KEY of the harmony (DETECT can find it from what you play).") \
X(HarmScale,     "harm.scale",    "Harmony Scale", "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_HARM_SCALE, "SCALE: the harmony stays in it. FIXED: exact intervals, no key.") \
X(HarmVoices,    "harm.voices",   "Harmony Voices", "Global", Int, 1.f, 3.f, 2.f, LIN, "", nullptr, "VOICES: how many harmony notes (1-3).") \
X(HarmV1Int,     "harm.v1.int",   "Harmony 1 Interval", "Global", Choice, 0, 0, 1.f, LIN, "", ZS_CH_HARM_INTERVAL, "Interval of harmony voice 1 (in scale steps).") \
X(HarmV1Dir,     "harm.v1.dir",   "Harmony 1 Direction", "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_UP_DOWN, "Voice 1 above or below the played note.") \
X(HarmV2Int,     "harm.v2.int",   "Harmony 2 Interval", "Global", Choice, 0, 0, 3.f, LIN, "", ZS_CH_HARM_INTERVAL, "Interval of harmony voice 2 (in scale steps).") \
X(HarmV2Dir,     "harm.v2.dir",   "Harmony 2 Direction", "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_UP_DOWN, "Voice 2 above or below the played note.") \
X(HarmV3Int,     "harm.v3.int",   "Harmony 3 Interval", "Global", Choice, 0, 0, 6.f, LIN, "", ZS_CH_HARM_INTERVAL, "Interval of harmony voice 3 (in scale steps).") \
X(HarmV3Dir,     "harm.v3.dir",   "Harmony 3 Direction", "Global", Choice, 0, 0, 1.f, LIN, "", ZS_CH_UP_DOWN, "Voice 3 above or below the played note.") \
X(HarmOutKey,    "harm.outkey",   "Harmony Out of Key", "Global", Choice, 0, 0, 1.f, LIN, "", ZS_CH_HARM_OUTKEY, "Played notes outside the scale: snap them to it, harmonise them anyway, or play them alone.") \
X(HarmMode,      "harm.mode",     "Harmonise", "Global", Choice, 0, 0, 1.f, LIN, "", ZS_CH_HARM_MODE, "EVERY NOTE, or the TOP NOTE only (the melody of a chord: much cleaner on keyboard chords).") \
X(HarmLevel,     "harm.level",    "Harmony Level", "Global", Float, 0.f, 1.f, 0.8f, LIN, "", nullptr, "LEVEL: velocity of the harmony notes against the played note.") \
X(HarmStrum,     "harm.strum",    "Harmony Strum", "Global", Float, 0.f, 50.f, 0.f, LIN, "ms", nullptr, "STRUM: each harmony note follows the one before by this much (played, not stacked).") \
X(HarmLow,       "harm.lo",       "Harmony Lowest", "Global", Int, 12.f, 120.f, 36.f, LIN, "note", nullptr, "RANGE: harmony notes below this fold up by octaves.") \
X(HarmHigh,      "harm.hi",       "Harmony Highest", "Global", Int, 12.f, 120.f, 96.f, LIN, "note", nullptr, "RANGE: harmony notes above this fold down by octaves.") \
X(HarmPart,      "harm.part",     "Harmony Applies To", "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_HARM_PART, "With SPLIT / LAYER: harmonise both sounds, or only UPPER or LOWER.") \
X(GlideType,     "voice.glidetype","Glide Type",    "Voice", Choice, 0, 0, 0.f, LIN, "", ZS_CH_GLIDE_TYPE, "ANALOG: the classic slide, quick at first then easing in (each slide takes about GLIDE). CONSTANT TIME: a straight slide that always takes GLIDE. CONSTANT RATE: a straight slide at GLIDE per octave, so bigger leaps take longer.") \
X(TrkSustain,    "trk.sus",       "Track Sustain", "Global", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "SUSTAIN: lets tracked notes ring on as the string dies away: while the tracker still clearly hears the note it holds, below the GATE. Full = HOLD: every note holds until you play the next one (a muted pick, with no pitch, stops it). Changes only when notes end: no added latency.") \
/* ------------------------------------------- Pattern sequencer (global: not in presets) */ \
X(PseqRun,       "pseq.run",      "Sequencer Run", "Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "RUN: starts and stops the pattern sequencer.") \
X(PseqKeyTr,     "pseq.keytr",    "Sequencer Key Transpose", "Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "KEY TRANSPOSE: while the sequencer runs, a key you play transposes both rows (C4 = no change) instead of sounding.") \
X(PseqSync,      "pseq.sync",     "Sequencer Host Sync", "Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "HOST SYNC: in a host, the sequencer starts and stops with the host's transport and locks to its bars.") \
X(PseqSwing,     "pseq.swing",    "Sequencer Swing", "Global", Float, 50.f, 75.f, 50.f, LIN, "%", nullptr, "SWING: 50 % = straight; higher pushes every second step late (shuffle).") \
ZS_PSEQ_ROW(X, A, "a", "A", 0.f, 60.f) ZS_PSEQ_ROW(X, B, "b", "B", 1.f, 36.f) \
X(PseqSongOn,    "pseq.song",     "Sequencer Song Mode", "Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "SONG: plays the song slots in order instead of the pattern on the panel.") \
ZS_PSEQ_SLOT(X, 1, 1.f)  ZS_PSEQ_SLOT(X, 2, 0.f)  ZS_PSEQ_SLOT(X, 3, 0.f)  ZS_PSEQ_SLOT(X, 4, 0.f) \
ZS_PSEQ_SLOT(X, 5, 0.f)  ZS_PSEQ_SLOT(X, 6, 0.f)  ZS_PSEQ_SLOT(X, 7, 0.f)  ZS_PSEQ_SLOT(X, 8, 0.f) \
ZS_PSEQ_SLOT(X, 9, 0.f)  ZS_PSEQ_SLOT(X, 10, 0.f) ZS_PSEQ_SLOT(X, 11, 0.f) ZS_PSEQ_SLOT(X, 12, 0.f) \
ZS_PSEQ_SLOT(X, 13, 0.f) ZS_PSEQ_SLOT(X, 14, 0.f) ZS_PSEQ_SLOT(X, 15, 0.f) ZS_PSEQ_SLOT(X, 16, 0.f) \
ZS_PSEQ_REP(X, 1)  ZS_PSEQ_REP(X, 2)  ZS_PSEQ_REP(X, 3)  ZS_PSEQ_REP(X, 4) \
ZS_PSEQ_REP(X, 5)  ZS_PSEQ_REP(X, 6)  ZS_PSEQ_REP(X, 7)  ZS_PSEQ_REP(X, 8) \
ZS_PSEQ_REP(X, 9)  ZS_PSEQ_REP(X, 10) ZS_PSEQ_REP(X, 11) ZS_PSEQ_REP(X, 12) \
ZS_PSEQ_REP(X, 13) ZS_PSEQ_REP(X, 14) ZS_PSEQ_REP(X, 15) ZS_PSEQ_REP(X, 16)


#define ZS_PSEQ_STEP(X, R, r, n, def) \
X(Pseq##R##Pitch##n, "pseq." r ".s" #n ".pitch", "Seq " r " Step " #n " Note", "Global", Int, 21.f, 108.f, def, LIN, "note", nullptr, "NOTE: the note the step plays (A0 to C8; KEY TRANSPOSE moves it).") \
X(Pseq##R##Gate##n,  "pseq." r ".s" #n ".gate",  "Seq " r " Step " #n " Gate",  "Global", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "GATE: the step plays (on) or rests (off).") \
X(Pseq##R##Acc##n,   "pseq." r ".s" #n ".acc",   "Seq " r " Step " #n " Accent","Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "ACCENT: the step plays at full velocity.") \
X(Pseq##R##Slide##n, "pseq." r ".s" #n ".slide", "Seq " r " Step " #n " Slide", "Global", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "SLIDE: the step is held into the next one (legato: mono and legato voices glide).")

#define ZS_PSEQ_SLOT(X, n, def) \
X(PseqSlot##n, "pseq.song." #n, "Song Slot " #n, "Global", Choice, 0, 0, def, LIN, "", ZS_CH_PSEQ_SLOT, "SONG slot: a pattern (played once through: its longest row's full cycle), LOOP (back to slot 1) or END (stop). Empty slots are skipped.")

#define ZS_PSEQ_REP(X, n) \
X(PseqRep##n, "pseq.song." #n ".rep", "Song Slot " #n " Repeats", "Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_PSEQ_REPEAT, "REPEATS: how many times the slot's pattern plays before the song moves on.")

#define ZS_PSEQ_ROW(X, R, r, label, target, root) \
X(Pseq##R##On,     "pseq." r ".on",     "Seq " label " On",       "Global", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Row on (off = it keeps time silently).") \
X(Pseq##R##Target, "pseq." r ".target", "Seq " label " Plays",    "Global", Choice, 0, 0, target, LIN, "", ZS_CH_PSEQ_TARGET, "Which sound the row plays with SPLIT / LAYER (with SINGLE, both rows play the sound on the panel).") \
X(Pseq##R##Len,    "pseq." r ".len",    "Seq " label " Length",   "Global", Int, 1.f, 16.f, 16.f, LIN, "", nullptr, "LENGTH: steps before the row repeats (rows of different lengths drift against each other).") \
X(Pseq##R##Dir,    "pseq." r ".dir",    "Seq " label " Direction","Global", Choice, 0, 0, 0.f, LIN, "", ZS_CH_PSEQ_DIR, "DIRECTION: forward, backward, pendulum (back and forth) or random.") \
X(Pseq##R##Div,    "pseq." r ".div",    "Seq " label " Step Size","Global", Choice, 0, 0, 2.f, LIN, "", ZS_CH_PSEQ_DIV, "STEP: the length of one step, against the tempo.") \
X(Pseq##R##GateLen,"pseq." r ".glen",   "Seq " label " Gate Time","Global", Float, 5.f, 100.f, 50.f, LIN, "%", nullptr, "GATE TIME: how much of each step a note lasts.") \
X(Pseq##R##Root,   "pseq." r ".root",   "Seq " label " Root",     "Global", Int, 12.f, 108.f, root, LIN, "note", nullptr, "Unused: kept so older projects load (steps now hold their own notes).") \
X(Pseq##R##Vel,    "pseq." r ".vel",    "Seq " label " Velocity", "Global", Int, 1.f, 127.f, 96.f, LIN, "", nullptr, "VELOCITY of the row's notes (accented steps play at 127).") \
ZS_PSEQ_STEP(X, R, r, 1, root)  ZS_PSEQ_STEP(X, R, r, 2, root)  ZS_PSEQ_STEP(X, R, r, 3, root)  ZS_PSEQ_STEP(X, R, r, 4, root) \
ZS_PSEQ_STEP(X, R, r, 5, root)  ZS_PSEQ_STEP(X, R, r, 6, root)  ZS_PSEQ_STEP(X, R, r, 7, root)  ZS_PSEQ_STEP(X, R, r, 8, root) \
ZS_PSEQ_STEP(X, R, r, 9, root)  ZS_PSEQ_STEP(X, R, r, 10, root) ZS_PSEQ_STEP(X, R, r, 11, root) ZS_PSEQ_STEP(X, R, r, 12, root) \
ZS_PSEQ_STEP(X, R, r, 13, root) ZS_PSEQ_STEP(X, R, r, 14, root) ZS_PSEQ_STEP(X, R, r, 15, root) ZS_PSEQ_STEP(X, R, r, 16, root)

#define ZS_OSC_WT_PARAMS(X, n) \
X(Osc##n##WtTable, "osc" #n ".wt.table", "Osc " #n " Table", "Wavetable", Choice, 0, 0, 0.f, LIN, "", ZS_CH_OSC_TABLE, "Wavetable for this oscillator (Wavetable wave): SECTION = the WAVETABLE section's TABLE.") \
X(Osc##n##WtPos,   "osc" #n ".wt.pos",   "Osc " #n " WT Pos", "Wavetable", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Scan position offset of this oscillator (added to the section's POS).") \
X(Osc##n##WtWarp,  "osc" #n ".wt.warp",  "Osc " #n " Warp", "Wavetable", Choice, 0, 0, 0.f, LIN, "", ZS_CH_WT_WARP, "WARP: reshapes how the table is read (bend, sync, formant, mirror, squeeze, fold, self-FM, quantize).") \
X(Osc##n##WtWarpAmt,"osc" #n ".wt.wamt", "Osc " #n " Warp Amount", "Wavetable", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Amount of the WARP.") \
X(Osc##n##WtVoices,"osc" #n ".wt.voices","Osc " #n " Stack", "Wavetable", Int, 1.f, 8.f, 1.f, LIN, "", nullptr, "STACK: number of detuned copies of the oscillator (wavetable unison).") \
X(Osc##n##WtDetune,"osc" #n ".wt.det",   "Osc " #n " Stack Detune", "Wavetable", Float, 0.f, 1.f, 0.25f, LIN, "", nullptr, "Detune spread of the stack (up to +/- 70 cents).") \
X(Osc##n##WtSpread,"osc" #n ".wt.pspr",  "Osc " #n " Stack Pos Spread", "Wavetable", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Spreads the stack voices through the table (each plays a different frame).") \
X(Osc##n##WtWidth, "osc" #n ".wt.width", "Osc " #n " Stack Width", "Wavetable", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "Stereo width of the stack.")

#define ZS_LFO_PARAMS(X, n, unused) \
X(Lfo##n##Wave,  "lfo" #n ".wave",  "LFO " #n " Wave",  "LFO", Choice, 0, 0, 0.f, LIN, "", ZS_CH_LFO_WAVE, "LFO waveform.") \
X(Lfo##n##Rate,  "lfo" #n ".rate",  "LFO " #n " Rate",  "LFO", Float, 0.02f, 50.f, 4.f, LOG, "Hz", nullptr, "Free-running LFO rate.") \
X(Lfo##n##Sync,  "lfo" #n ".sync",  "LFO " #n " Sync",  "LFO", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Locks the LFO rate to tempo divisions.") \
X(Lfo##n##Div,   "lfo" #n ".div",   "LFO " #n " Division","LFO", Choice, 0, 0, 7.f, LIN, "", ZS_CH_DIVISION, "LFO cycle length when synced.") \
X(Lfo##n##Phase, "lfo" #n ".phase", "LFO " #n " Phase", "LFO", Float, 0.f, 360.f, 0.f, LIN, "deg", nullptr, "Start phase on retrigger (offset when free).") \
X(Lfo##n##Delay, "lfo" #n ".delay", "LFO " #n " Delay", "LFO", Float, 0.f, 5.f, 0.f, 0.5f, "s", nullptr, "Fade-in time after each note starts.") \
X(Lfo##n##Amount,"lfo" #n ".amt",   "LFO " #n " Amount","LFO", Float, 0.f, 1.f, 0.f, 0.2f, "", nullptr, "Depth of the LFO's direct route.") \
X(Lfo##n##Mode,  "lfo" #n ".mode",  "LFO " #n " Trigger","LFO", Choice, 0, 0, 0.f, LIN, "", ZS_CH_LFO_MODE, "Free: all voices share one phase. Retrig: restarts per note. One Shot: one cycle per note.") \
X(Lfo##n##Unipolar,"lfo" #n ".uni", "LFO " #n " Unipolar","LFO", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Unipolar (0..1) instead of bipolar (-1..1) output.") \
X(Lfo##n##Dest,  "lfo" #n ".dest",  "LFO " #n " Dest",  "LFO", Choice, 0, 0, 1.f, LIN, "", ZS_CH_LFO_DEST, "Direct destination (the LFO is also a matrix source).")

#define ZS_OP_PARAMS(X, n, ratio, level) \
X(Op##n##Ratio,  "fm.op" #n ".ratio","Op " #n " Ratio", "FM", Float, 0.5f, 16.f, ratio, 4.f, "x", nullptr, "Operator coarse frequency ratio (snaps to 0.5 and whole numbers).") \
X(Op##n##Fine,   "fm.op" #n ".fine", "Op " #n " Fine",  "FM", Float, 0.f, 0.99f, 0.f, LIN, "", nullptr, "Operator fine ratio: multiplies the ratio by (1 + fine).") \
X(Op##n##Level,  "fm.op" #n ".level","Op " #n " Level", "FM", Float, 0.f, 1.f, level, LIN, "", nullptr, "Operator output level (modulation index for modulators).") \
X(Op##n##A,      "fm.op" #n ".a",    "Op " #n " Attack","FM", Float, 0.0005f, 20.f, 0.001f, LOG, "s", nullptr, "Operator envelope attack.") \
X(Op##n##D,      "fm.op" #n ".d",    "Op " #n " Decay", "FM", Float, 0.001f, 20.f, 1.5f, LOG, "s", nullptr, "Operator envelope decay.") \
X(Op##n##S,      "fm.op" #n ".s",    "Op " #n " Sustain","FM", Float, 0.f, 1.f, 0.6f, LIN, "", nullptr, "Operator envelope sustain.") \
X(Op##n##R,      "fm.op" #n ".r",    "Op " #n " Release","FM", Float, 0.001f, 20.f, 0.4f, LOG, "s", nullptr, "Operator envelope release.") \
X(Op##n##Vel,    "fm.op" #n ".vel",  "Op " #n " Velocity","FM", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Operator velocity sensitivity: soft notes lower this operator's level (up to -30 dB).") \
X(Op##n##Krs,    "fm.op" #n ".krs",  "Op " #n " Rate Scale","FM", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Keyboard rate scaling: the operator envelope runs faster above C4 and slower below (x2 per octave at full).") \
X(Op##n##Kls,    "fm.op" #n ".kls",  "Op " #n " Level Scale","FM", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Keyboard level scaling: lowers the operator by up to 9 dB per octave above C4.")

#define ZS_MOD_PARAMS(X, n) \
X(Mod##n##Src,   "mod" #n ".src",   "Mod " #n " Source","Mod Matrix", Choice, 0, 0, 0.f, LIN, "", ZS_CH_MOD_SRC, "Modulation source.") \
X(Mod##n##Dst,   "mod" #n ".dst",   "Mod " #n " Dest",  "Mod Matrix", Choice, 0, 0, 0.f, LIN, "", ZS_CH_MOD_DST, "Modulation destination.") \
X(Mod##n##Amt,   "mod" #n ".amt",   "Mod " #n " Amount","Mod Matrix", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Modulation amount (negative inverts).")

#define ZS_SEQ_STEP(X, n) \
X(SeqVal##n,     "seq.s" #n ".val", "Step " #n " Value","Sequencer", Float, -1.f, 1.f, 0.f, LIN, "", nullptr, "Step value: modulation level, or +/-12 semitones in Note mode.") \
X(SeqGate##n,    "seq.s" #n ".gate","Step " #n " Gate", "Sequencer", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Step plays (on) or rests (off).") \
X(SeqAcc##n,     "seq.s" #n ".acc", "Step " #n " Accent","Sequencer", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Accented step (louder, brighter).") \
X(SeqSlide##n,   "seq.s" #n ".slide","Step " #n " Slide","Sequencer", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Slides legato into the next step.")

#define ZS_SEQ_STEPS(X) \
ZS_SEQ_STEP(X, 1)  ZS_SEQ_STEP(X, 2)  ZS_SEQ_STEP(X, 3)  ZS_SEQ_STEP(X, 4) \
ZS_SEQ_STEP(X, 5)  ZS_SEQ_STEP(X, 6)  ZS_SEQ_STEP(X, 7)  ZS_SEQ_STEP(X, 8) \
ZS_SEQ_STEP(X, 9)  ZS_SEQ_STEP(X, 10) ZS_SEQ_STEP(X, 11) ZS_SEQ_STEP(X, 12) \
ZS_SEQ_STEP(X, 13) ZS_SEQ_STEP(X, 14) ZS_SEQ_STEP(X, 15) ZS_SEQ_STEP(X, 16)

struct ParamDef
{
    const char* id;
    const char* name;
    const char* group;
    ParamType type;
    float min, max, def;
    float scale;        // LIN, LOG or skew centre (see above)
    const char* unit;
    const char* choices; // "|"-separated, Choice params only
    const char* help;
};

namespace P
{
#define ZS_ENUM_ENTRY(e, ...) e,
    enum Index : int
    {
        ZS_PARAM_LIST(ZS_ENUM_ENTRY)
        Count
    };
#undef ZS_ENUM_ENTRY
}

constexpr int kNumParams = P::Count;

/** The table, indexed by P::Index. Choice min/max are filled in from their lists. */
const ParamDef& paramDef(int index);

/** Returns the index for a string id, or -1. Linear search: not for the audio thread. */
int findParamIndex(const char* id);

/** Number of entries in a Choice param's list (0 for non-choice params). */
int choiceCount(int index);

/** Copies the n-th label of a choice list into buf (null-terminated). */
void choiceLabel(int index, int choice, char* buf, int bufSize);

/** Fills a snapshot array with every parameter's default value. */
void fillDefaults(float* values);

// Strided accessors for repeated blocks (indexes are contiguous per block).
constexpr int kOpParamStride  = P::Op2Ratio - P::Op1Ratio;
constexpr int kModParamStride = P::Mod2Src  - P::Mod1Src;
constexpr int kSeqParamStride = P::SeqVal2  - P::SeqVal1;
constexpr int kLfoParamStride = P::Lfo2Wave - P::Lfo1Wave;
constexpr int kNumOperators   = 6;
constexpr int kNumModSlots    = 8;
constexpr int kNumSeqSteps    = 16;
// Pattern sequencer: per-step blocks (pitch, gate, accent, slide) and the two rows.
constexpr int kPseqStepStride = P::PseqAPitch2 - P::PseqAPitch1;
constexpr int kPseqRowStride  = P::PseqBOn - P::PseqAOn;
constexpr int kPseqRowParams  = kPseqRowStride; // every parameter of one row (a pattern row)
constexpr int kPseqSteps      = 16;

} // namespace zsynth
