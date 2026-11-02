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

// SuperSaw: seven detuned saws (JP-8000 style). HyperSaw: nine saws with a
// sub an octave down (Virus style). ROM waves: single-cycle multi-frame waves
// voiced after the PCM ROMs of the 90s workstations; each plays its own
// built-in evolution (attack frames, then a slow loop) from every note-on.
#define ZS_CH_OSC_WAVE "Sine|Triangle|Saw|Square|Pulse|Wavetable|SuperSaw|HyperSaw|ROM Ahh Choir|ROM Ooh Voices|ROM Glass Pad|ROM Digital Bell|ROM Spectrum|ROM Digi Pad|ROM Strings|ROM E.Piano|ROM Brass|ROM Mallet|ROM Pluck|ROM Heaven|ROM Metal|ROM Bowed Glass|ROM Air Vox|ROM Synth Pad"
enum class OscWave : int
{
    Sine, Triangle, Saw, Square, Pulse, Wavetable, SuperSaw, HyperSaw,
    RomAhhChoir, RomOohVoices, RomGlassPad, RomDigitalBell, RomSpectrum, RomDigiPad, RomStrings, RomEPiano,
    RomBrass, RomMallet, RomPluck, RomHeaven, RomMetal, RomBowedGlass, RomAirVox, RomSynthPad, Count
};
constexpr int kNumRomWaves = (int) OscWave::Count - (int) OscWave::RomAhhChoir;
inline bool isRomWave(OscWave w) { return (int) w >= (int) OscWave::RomAhhChoir; }
inline bool isSuperWave(OscWave w) { return w == OscWave::SuperSaw || w == OscWave::HyperSaw; }

#define ZS_CH_OCTAVE "32'|16'|8'|4'|2'"
enum class OscOctave : int { Ft32, Ft16, Ft8, Ft4, Ft2, Count };

#define ZS_CH_SUB_WAVE "Square -1|Square -2|Sine -1|Pulse -1"
enum class SubWave : int { SquareMinus1, SquareMinus2, SineMinus1, PulseMinus1, Count };

#define ZS_CH_NOISE "White|Pink"
enum class NoiseType : int { White, Pink, Count };

// The "Seq" tables hold 32 distinct waves (not a smooth morph), made for wave
// sequencing (Wavestation style); they also scan.
#define ZS_CH_WAVETABLE "Analog Morph|Harmonic Sweep|Vocal Formant|PWM Scan|Digital Bell|Sync Sweep|Wavefolder|Drawbar Organ|Seq Harmonics|Seq Digital|Seq Vocal|Seq Bells|Spectral"
enum class WavetableId : int { AnalogMorph, HarmonicSweep, VocalFormant, PwmScan, DigitalBell, SyncSweep, Wavefolder, DrawbarOrgan,
                               SeqHarmonics, SeqDigital, SeqVocal, SeqBells, Spectral, Count };

#define ZS_CH_WAVE_SEQ "Off|Loop|Ping-Pong|One Shot"
enum class WaveSeqMode : int { Off, Loop, PingPong, OneShot, Count };

#define ZS_CH_FILTER_MODEL "Ladder|OTA|SEM|K35|JP-8000|Virus|Roland TVF|Korg Digital|Alesis QS"
enum class FilterModel : int { Ladder, Ota, Sem, K35, Jp8000, Virus, RolandTvf, KorgDigital, AlesisQs, Count };

#define ZS_CH_FILTER_TYPE "LP|BP|HP|Notch"
enum class FilterType : int { LowPass, BandPass, HighPass, Notch, Count };

#define ZS_CH_FILTER_SLOPE "12 dB|24 dB"
enum class FilterSlope : int { Pole2, Pole4, Count };

#define ZS_CH_LFO_WAVE "Sine|Triangle|Saw Up|Ramp Down|Square|S&H|Smooth Rnd"
enum class LfoWave : int { Sine, Triangle, SawUp, RampDown, Square, SampleHold, SmoothRandom, Count };

#define ZS_CH_LFO_MODE "Free|Retrig|One Shot"
enum class LfoMode : int { Free, Retrigger, OneShot, Count };

#define ZS_CH_LFO_DEST "Off|Pitch|Cutoff|PW|Amp|Pan|WT Pos|FM Depth|Osc2 Pitch|Resonance|Vector X|Vector Y|Detune"
enum class LfoDest : int { Off, Pitch, Cutoff, PulseWidth, Amp, Pan, WavetablePos, FmDepth, Osc2Pitch, Resonance, VectorX, VectorY, Detune, Count };

#define ZS_CH_ENV3_DEST "Off|Pitch|Osc2 Pitch|PW|Cutoff|WT Pos|FM Depth|Pan|Noise|Vector X|Vector Y|Detune"
enum class Env3Dest : int { Off, Pitch, Osc2Pitch, PulseWidth, Cutoff, WavetablePos, FmDepth, Pan, Noise, VectorX, VectorY, Detune, Count };

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

#define ZS_CH_FM_ALGO "1 Stack|2 Twin Stacks|3 Three Pairs|4 Pair+Stack|5 Y Branch|6 Fan Out|7 Triple Mod|8 Deep+Pair|9 Stack+Carriers|10 Dual Mod|11 Organ|12 Cascade Fan|13 Pair+Triple|14 Tree|15 Pair+Branch|16 Stack+Fan"
constexpr int kNumFmAlgorithms = 16;

#define ZS_CH_MOD_SRC "Off|LFO 1|LFO 2|Env 1|Env 2|Env 3|Velocity|Key|Mod Wheel|Pitch Bend|Aftertouch|Poly AT|Expression|Random|S&H|Seq Step|Macro"
enum class ModSource : int
{
    Off, Lfo1, Lfo2, Env1, Env2, Env3, Velocity, Key, ModWheel, PitchBend,
    Aftertouch, PolyAftertouch, Expression, Random, SampleHold, SeqStep, Macro, Count
};

#define ZS_CH_MOD_DST "Off|Pitch|Osc1 Pitch|Osc2 Pitch|Osc3 Pitch|Pulse Width|Osc1 Level|Osc2 Level|Osc3 Level|Sub Level|Noise Level|Osc FM|WT Position|FM Depth|FM Feedback|Cutoff|Resonance|Filter Drive|HPF Cutoff|Amp Level|Pan|LFO1 Rate|LFO2 Rate|LFO1 Amount|LFO2 Amount|FX Drive|Chorus Mix|Delay Mix|Reverb Mix|Delay Feedback|Vector X|Vector Y|Detune"
enum class ModDest : int
{
    Off, Pitch, Osc1Pitch, Osc2Pitch, Osc3Pitch, PulseWidth, Osc1Level, Osc2Level, Osc3Level,
    SubLevel, NoiseLevel, OscFm, WavetablePos, FmDepth, FmFeedback, Cutoff, Resonance,
    FilterDrive, HpfCutoff, AmpLevel, Pan, Lfo1Rate, Lfo2Rate, Lfo1Amount, Lfo2Amount,
    FxDrive, ChorusMix, DelayMix, ReverbMix, DelayFeedback, VectorX, VectorY, Detune, Count
};

#define ZS_CH_ARP_MODE "Up|Down|Up/Down|Random|As Played|Chord"
enum class ArpMode : int { Up, Down, UpDown, Random, AsPlayed, Chord, Count };

#define ZS_CH_SEQ_MODE "Off|Mod|Note"
enum class SeqMode : int { Off, Mod, Note, Count };

#define ZS_CH_DELAY_TYPE "Digital|Tape RE-201|Tape EP-3|Analog DM-2|Diffuse|Lo-Fi"
enum class DelayType : int { Digital, TapeRE201, TapeEP3, AnalogDM2, Diffuse, LoFi, Count };

#define ZS_CH_XFMR_TYPE "Neve LO1166|API 2503|UTC A-24|Haufe V76|Jensen JT-11"
enum class XfmrType : int { NeveLo1166, Api2503, UtcA24, HaufeV76, JensenJt11, Count };

#define ZS_CH_EQ_MODEL "Pristine|Pultec EQP-1A|Neve 1073|API 550A|API 550B|API 560"
enum class EqModel : int { Pristine, Pultec, Neve1073, Api550A, Api550B, Api560, Count };
#define ZS_CH_PUL_LF "20|30|60|100"
#define ZS_CH_PUL_HF "3k|4k|5k|8k|10k|12k|16k"
#define ZS_CH_PUL_ATT "5k|10k|20k"
#define ZS_CH_NEVE_HPF "Off|50|80|160|300"
#define ZS_CH_NEVE_LF "Off|35|60|110|220"
#define ZS_CH_NEVE_MF "360|700|1.6k|3.2k|4.8k|7.2k"
#define ZS_CH_A550_LF "50|100|200|300|400"
#define ZS_CH_A550_MF "400|800|1.5k|3k|5k"
#define ZS_CH_A550_HF "2.5k|5k|7.5k|10k|12.5k"
#define ZS_CH_A550_GAIN "-12|-9|-6|-4|-2|0|+2|+4|+6|+9|+12"
#define ZS_CH_B550_LF "30|40|50|100|200|300|400"
#define ZS_CH_B550_LMF "75|150|180|240|500|700|1k"
#define ZS_CH_B550_HMF "800|1.5k|3k|5k|8k|10k|12.5k"
#define ZS_CH_B550_HF "2.5k|5k|7k|10k|12.5k|15k|20k"
#define ZS_CH_B550_GAIN "-15|-12|-10|-8|-6|-4|-2|0|+2|+4|+6|+8|+10|+12|+15"

#define ZS_CH_TAPE_MODEL "Studer A800|Ampex ATR-102|Studer J37|Otari MTR-90|Cassette 4-Track"
enum class TapeModel : int { StuderA800, AmpexAtr102, StuderJ37, OtariMtr90, Cassette, Count };
#define ZS_CH_TAPE_SPEED "7.5 ips|15 ips|30 ips"

#define ZS_CH_TRK_RANGE "Guitar|Bass"
#define ZS_CH_TRK_INPUT "Input 1|Input 2|Inputs 1+2"

#define ZS_CH_CHORUS_TYPE "Classic BBD|Roland Digital|Korg Stereo|Virus"
enum class ChorusType : int { ClassicBbd, RolandDigital, KorgStereo, Virus, Count };

#define ZS_CH_REVERB_TYPE "Hall|Room|EMT 140 Plate|Echo Chamber|Spring|Lexicon 224|Lexicon 480L|Bricasti M7|Shimmer|AMS Nonlin|AMS Reverse|Yamaha REV7|Eventide SP2016|Quantec QRS|Lexicon PCM70"
enum class ReverbType : int { Hall, Room, Plate, Chamber, Spring, Lex224, Lex480, BricastiM7, Shimmer,
                              AmsNonlin, AmsReverse, YamahaRev7, EventideSP2016, QuantecQRS, LexPCM70, Count };

#define ZS_CH_COMP_MODEL "LA-2A Opto|SSL Bus|1176 FET|Fairchild 670"
enum class CompModel : int { LA2A, SSLBus, FET1176, Fairchild, Count };
#define ZS_CH_COMP_RATIO "1.5:1|2:1|3:1|4:1|8:1|12:1|20:1|All"

#define ZS_CH_VALVE_TYPE "12AX7 Preamp|12AU7 Clean|EL84 Push-Pull|6L6 Power"
enum class ValveType : int { Preamp12AX7, Clean12AU7, PushPullEL84, Power6L6, Count };

#define ZS_CH_DRIVE_TYPE "Overdrive|Distortion|Fold"
enum class DriveType : int { Overdrive, Distortion, Fold, Count };

#define ZS_CH_OVERSAMPLING "Auto|Eco 1x|High 2x"
enum class OversamplingMode : int { Auto, Eco, High, Count };

} // namespace zsynth
