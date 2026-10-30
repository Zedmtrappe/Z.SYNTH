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
X(Polyphony,     "voice.poly",     "Polyphony",     "Voice", Int, 1.f, 32.f, 16.f, LIN, "", nullptr, "Maximum number of physical voices (unison voices count individually).") \
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
X(ReverbLowCut,  "fx.rev.lowcut",  "Reverb Low Cut","Effects", Float, 20.f, 1000.f, 20.f, LOG, "Hz", nullptr, "High-pass on the reverb (keeps the low end clear).") \
X(ReverbDiff,    "fx.rev.diff",    "Reverb Diffusion","Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Echo density: low is grainier, high is smoother.") \
X(ReverbMod,     "fx.rev.mod",     "Reverb Modulation","Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Chorusing of the tail (0.5 = the type's natural amount).") \
X(ReverbEarly,   "fx.rev.early",   "Reverb Early/Late","Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Balance of early reflections (left) and tail (right).") \
X(ReverbWidth,   "fx.rev.width",   "Reverb Width",  "Effects", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Stereo width of the reverb (0.5 = natural, 1 = extra wide).") \
/* ----------------------------------------------------------------- Output */ \
X(Width,         "out.width",      "Stereo Width",  "Output", Float, 0.f, 2.f, 1.f, LIN, "", nullptr, "Mid/side stereo width (0 = mono, 1 = unchanged, 2 = wide).") \
X(Character,     "out.character",  "Character",     "Output", Float, 0.f, 1.f, 0.1f, LIN, "", nullptr, "Analogue-style output stage saturation and warmth.") \
X(Macro,         "out.macro",      "Macro",         "Output", Float, 0.f, 1.f, 0.f, LIN, "", nullptr, "Performance macro: a modulation-matrix source.") \
X(Limiter,       "out.limiter",    "Limiter",       "Output", Bool, 0.f, 1.f, 1.f, LIN, "", nullptr, "Output safety limiter (keeps peaks below 0 dBFS).") \
X(ValveOn,       "out.valve.on",   "Valve On",      "Output", Bool, 0.f, 1.f, 0.f, LIN, "", nullptr, "Valve output stage (oversampled triode / power-valve saturation).") \
X(ValveType,     "out.valve.type", "Valve Type",    "Output", Choice, 0, 0, 0.f, LIN, "", ZS_CH_VALVE_TYPE, "12AX7: high-gain preamp triode (even harmonics). 12AU7: low-gain, clean warmth. EL84: push-pull power pair (odd harmonics, early compression). 6L6: big, smooth power valve with more headroom.") \
X(ValveDrive,    "out.valve.drive","Valve Drive",   "Output", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "How hard the valve is driven (level-compensated).") \
X(ValveBias,     "out.valve.bias", "Valve Bias",    "Output", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Operating point: cold (low) is cleaner and more symmetric, hot (high) adds even harmonics and grit.") \
X(ValveSag,      "out.valve.sag",  "Valve Sag",     "Output", Float, 0.f, 1.f, 0.3f, LIN, "", nullptr, "Power-supply sag: the valve compresses and blooms on loud notes.") \
X(ValveWarmth,   "out.valve.warmth","Valve Warmth", "Output", Float, 0.f, 1.f, 0.5f, LIN, "", nullptr, "Output transformer: rounds off the top end and adds low-end weight.") \
X(ValveMix,      "out.valve.mix",  "Valve Mix",     "Output", Float, 0.f, 1.f, 1.f, LIN, "", nullptr, "Dry/valve blend.")

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

} // namespace zsynth
