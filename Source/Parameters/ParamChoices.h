#pragma once

// Choice lists used by the parameter table, kept side by side with the enums the
// DSP engine uses to interpret them. The order of each "|"-separated list MUST
// match its enum; Tests/StateTests.cpp verifies the counts match.
//
// This header is deliberately free of JUCE so the DSP engine can use it.

namespace zsynth
{

#define ZS_CH_ENGINE "Analog|Hybrid|FM"
enum class EngineMode : int { Analog, Hybrid, FM, Count };

#define ZS_CH_OSC_WAVE "Sine|Triangle|Saw|Square|Pulse|Wavetable"
enum class OscWave : int { Sine, Triangle, Saw, Square, Pulse, Wavetable, Count };

#define ZS_CH_OCTAVE "32'|16'|8'|4'|2'"
enum class OscOctave : int { Ft32, Ft16, Ft8, Ft4, Ft2, Count };

#define ZS_CH_SUB_WAVE "Square -1|Square -2|Sine -1|Pulse -1"
enum class SubWave : int { SquareMinus1, SquareMinus2, SineMinus1, PulseMinus1, Count };

#define ZS_CH_NOISE "White|Pink"
enum class NoiseType : int { White, Pink, Count };

#define ZS_CH_WAVETABLE "Analog Morph|Harmonic Sweep|Vocal Formant|PWM Scan|Digital Bell|Sync Sweep|Wavefolder|Drawbar Organ"
enum class WavetableId : int { AnalogMorph, HarmonicSweep, VocalFormant, PwmScan, DigitalBell, SyncSweep, Wavefolder, DrawbarOrgan, Count };

#define ZS_CH_FILTER_MODEL "Ladder|OTA|SEM|K35"
enum class FilterModel : int { Ladder, Ota, Sem, K35, Count };

#define ZS_CH_FILTER_TYPE "LP|BP|HP|Notch"
enum class FilterType : int { LowPass, BandPass, HighPass, Notch, Count };

#define ZS_CH_FILTER_SLOPE "12 dB|24 dB"
enum class FilterSlope : int { Pole2, Pole4, Count };

#define ZS_CH_LFO_WAVE "Sine|Triangle|Saw Up|Ramp Down|Square|S&H|Smooth Rnd"
enum class LfoWave : int { Sine, Triangle, SawUp, RampDown, Square, SampleHold, SmoothRandom, Count };

#define ZS_CH_LFO_MODE "Free|Retrig|One Shot"
enum class LfoMode : int { Free, Retrigger, OneShot, Count };

#define ZS_CH_LFO_DEST "Off|Pitch|Cutoff|PW|Amp|Pan|WT Pos|FM Depth|Osc2 Pitch|Resonance"
enum class LfoDest : int { Off, Pitch, Cutoff, PulseWidth, Amp, Pan, WavetablePos, FmDepth, Osc2Pitch, Resonance, Count };

#define ZS_CH_ENV3_DEST "Off|Pitch|Osc2 Pitch|PW|Cutoff|WT Pos|FM Depth|Pan|Noise"
enum class Env3Dest : int { Off, Pitch, Osc2Pitch, PulseWidth, Cutoff, WavetablePos, FmDepth, Pan, Noise, Count };

// Tempo divisions shared by LFOs, arpeggiator, sequencer and delay.
// The beat lengths live in DSP/Common/TempoDivisions.h.
#define ZS_CH_DIVISION "4/1|2/1|1/1|1/2D|1/2|1/2T|1/4D|1/4|1/4T|1/8D|1/8|1/8T|1/16D|1/16|1/16T|1/32"
constexpr int kNumDivisions = 16;

#define ZS_CH_VOICE_MODE "Poly|Mono|Legato"
enum class VoiceMode : int { Poly, Mono, Legato, Count };

#define ZS_CH_PRIORITY "Last|Low|High"
enum class NotePriority : int { Last, Low, High, Count };

#define ZS_CH_ALLOC "Rotate|Reuse|Lowest"
enum class VoiceAlloc : int { Rotate, Reuse, Lowest, Count };

#define ZS_CH_GLIDE_MODE "Always|Legato"
enum class GlideMode : int { Always, LegatoOnly, Count };

#define ZS_CH_VEL_CURVE "Soft|Linear|Hard|Fixed"
enum class VelocityCurve : int { Soft, Linear, Hard, Fixed, Count };

#define ZS_CH_FM_ALGO "1 Stack|2 Twin Stacks|3 Three Pairs|4 Pair+Stack|5 Y Branch|6 Fan Out|7 Triple Mod|8 Deep+Pair|9 Stack+Carriers|10 Dual Mod|11 Organ|12 Cascade Fan"
constexpr int kNumFmAlgorithms = 12;

#define ZS_CH_MOD_SRC "Off|LFO 1|LFO 2|Env 1|Env 2|Env 3|Velocity|Key|Mod Wheel|Pitch Bend|Aftertouch|Poly AT|Expression|Random|S&H|Seq Step|Macro"
enum class ModSource : int
{
    Off, Lfo1, Lfo2, Env1, Env2, Env3, Velocity, Key, ModWheel, PitchBend,
    Aftertouch, PolyAftertouch, Expression, Random, SampleHold, SeqStep, Macro, Count
};

#define ZS_CH_MOD_DST "Off|Pitch|Osc1 Pitch|Osc2 Pitch|Osc3 Pitch|Pulse Width|Osc1 Level|Osc2 Level|Osc3 Level|Sub Level|Noise Level|Osc FM|WT Position|FM Depth|FM Feedback|Cutoff|Resonance|Filter Drive|HPF Cutoff|Amp Level|Pan|LFO1 Rate|LFO2 Rate|LFO1 Amount|LFO2 Amount|FX Drive|Chorus Mix|Delay Mix|Reverb Mix|Delay Feedback"
enum class ModDest : int
{
    Off, Pitch, Osc1Pitch, Osc2Pitch, Osc3Pitch, PulseWidth, Osc1Level, Osc2Level, Osc3Level,
    SubLevel, NoiseLevel, OscFm, WavetablePos, FmDepth, FmFeedback, Cutoff, Resonance,
    FilterDrive, HpfCutoff, AmpLevel, Pan, Lfo1Rate, Lfo2Rate, Lfo1Amount, Lfo2Amount,
    FxDrive, ChorusMix, DelayMix, ReverbMix, DelayFeedback, Count
};

#define ZS_CH_ARP_MODE "Up|Down|Up/Down|Random|As Played|Chord"
enum class ArpMode : int { Up, Down, UpDown, Random, AsPlayed, Chord, Count };

#define ZS_CH_SEQ_MODE "Off|Mod|Note"
enum class SeqMode : int { Off, Mod, Note, Count };

#define ZS_CH_DRIVE_TYPE "Overdrive|Distortion|Fold"
enum class DriveType : int { Overdrive, Distortion, Fold, Count };

#define ZS_CH_OVERSAMPLING "Auto|Eco 1x|High 2x"
enum class OversamplingMode : int { Auto, Eco, High, Count };

} // namespace zsynth
