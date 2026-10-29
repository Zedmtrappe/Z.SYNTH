# Z.SYNTH user manual

## Getting started

1. Launch **Z.SYNTH**. It opens on the first factory preset, ready to play.
2. Choose your audio output in **Options > Audio/MIDI Settings** (top-left of
   the window). On Linux pick ALSA or JACK.
3. Connected MIDI keyboards are opened automatically. You can also play the
   on-screen keyboard with the mouse, or with the computer keyboard after
   clicking on it (A, W, S, E, D... starting from C).
4. Turn any knob and listen. Hover a control for its name, value and purpose.
5. Browse sounds with **<** and **>** or **BROWSE**.
6. Save your own sound with **SAVE AS**. Your sound and settings are also
   restored automatically the next time you start Z.SYNTH.

### Working with controls

| Action | Result |
|---|---|
| Drag up/down | Change value (hold **Shift** for fine control) |
| Mouse wheel | Step the value |
| Double-click | Reset to default |
| Right-click | MIDI Learn / clear mapping / reset |
| Click then arrow keys | Step; **Shift** = x10, **Home** = default |
| Click a selector (LCD box) | Choose from a list |

A dimmed control is **inactive** in the current configuration (for example the
FM section while ENGINE is ANALOG). It stays where it is and can still be
adjusted; its tooltip says why it is inactive. The panel never rearranges.

The window can be resized; the panel scales uniformly between half and 1.5x
its 1920 x 1200 design size.

## Header

| Control | |
|---|---|
| Preset display | Category, number and name. Click to open the browser. |
| < > | Previous / next preset |
| BROWSE | Floating browser: search, category filter, favourites, rename, delete |
| SAVE | Overwrite the current user preset (or ask for a name for factory presets) |
| SAVE AS | Save as a new user preset (name and category) |
| INIT | Load the basic starting patch |
| TUNE, TRANSP, OCTAVE | Global tuning (cents), transpose (semitones), octave shift |
| TEMPO | Internal tempo, used when no host tempo is available |
| QUALITY | Auto (2x voice oversampling below 80 kHz), Eco 1x, High 2x |
| MIDI CH | Omni or a single receive channel |
| MIDI IN / AUDIO | Activity lights (AUDIO is dark when no audio device is running) |
| POLYPHONY | Sounding voices / voice limit |
| MASTER | Output level |
| PANIC | Silence all voices and effect tails immediately |

Global settings are *not* changed by loading presets.

## Oscillators

Three oscillators with **ON**, **WAVE** (sine, triangle, saw, square, pulse,
wavetable), **RANGE** (32' to 2'), **SEMI**, **FINE** (cents), **PW** (pulse
width, Pulse wave only) and **LEVEL**.

* **FM 3>1, FM 1>2, FM 1>3** — audio-rate frequency modulation.
* **SYNC** (osc 2 and 3) — hard sync to oscillator 1. Sweep the synced
  oscillator's pitch (e.g. with ENV 3 or the mod wheel) for classic sync leads.
* **RING** (osc 2 and 3) — blends into ring modulation with oscillator 1.
* **ENGINE** — ANALOG (oscillators), FM (operators only), HYBRID (both).
* **SUB** level and **SUB WAVE** (square −1/−2 octaves, sine, 25 % pulse),
  locked to oscillator 1.
* **NOISE** level and **COLOUR** (white/pink).
* **DRIFT** — slow independent pitch wander per oscillator.
* **DRIVE** — mixer overdrive: warmth at low settings, dense saturation high.

### Wavetable

Oscillators set to **Wavetable** play the selected **TABLE**: Analog Morph,
Harmonic Sweep, Vocal Formant, PWM Scan, Digital Bell, Sync Sweep,
Wavefolder, Drawbar Organ. **POSITION** scans through the table (modulate it
from LFOs, envelopes or the matrix for movement). **SPREAD** offsets
oscillator 2 and 3 positions.

## Filter

* **MODEL** — Ladder (warm 4-pole transistor ladder), OTA (punchy cascade),
  SEM (smooth state-variable multimode), K35 (aggressive, screaming).
* **TYPE** LP/BP/HP/Notch, **SLOPE** 12/24 dB (K35 has a fixed slope).
* **CUTOFF**, **RESONANCE** (Ladder, OTA and K35 self-oscillate near maximum).
* **ENV AMT** — envelope 2 modulation (up to 8 octaves); **INVERT** flips it.
* **HPF** — gentle high-pass before the main filter (thins basses, cleans pads).
* **DRIVE**, **KEY** (100 % = cutoff follows the keyboard), **VELO**,
  **LFO 1** (direct LFO 1 depth).
* The curve shows the approximate response of the current settings.

## Envelopes

1 AMP (amplifier), 2 FILTER (filter and any matrix destination), 3 MOD
(direct route via **DEST/AMOUNT**, plus matrix source). All are ADSR with
analogue-style curves: times from 0.5 ms to 20 s. **VELO** sets velocity
sensitivity of the amplifier and of envelope 2.

## LFOs

Two LFOs with **WAVE** (sine, triangle, saw up, ramp down, square, S&H, smooth
random), **TRIGGER** (Free: all voices share one phase; Retrig: restart per
note; One Shot: a single cycle), **DEST** + **AMOUNT** (direct route),
**SYNC** (use **DIVISION** instead of **RATE**), **UNIPOLAR**, **PHASE** and
**DELAY** (fade-in after each note). Both LFOs are also matrix sources.

Tip: for vibrato on the mod wheel set DEST = Pitch, AMOUNT = 0, and route
*Mod Wheel → LFO1 Amount* at about +2 % in the matrix.

## Voice & amp

* **MODE** Poly / Mono (retriggers) / Legato (no retrigger while notes overlap).
* **PRIORITY** (mono): last, lowest or highest note.
* **ALLOCATE** (poly): Rotate (vintage round-robin), Reuse, Lowest.
* **VEL CURVE**, **VOICES** (1–32 physical voices), **GLIDE** + **GLIDE MODE**,
  **BEND** range, **AMP** level, **PAN**.
* **UNISON** with **STACK** (2–8 voices per note), **DETUNE** and **SPREAD**
  (also spreads poly voices across the stereo field).
* **VINTAGE** — per-voice differences in tuning, cutoff, level and position.
* The LED row shows which of the 32 voices are sounding.

## FM operators

Active when ENGINE is FM or HYBRID. **ALGORITHM** chooses how the six
operators connect (the diagram shows modulators above carriers; the loop on
OP6 is its feedback). **DEPTH** scales all modulation, **FDBK** is operator-6
feedback, **LEVEL** is the FM output into the mixer, **VELO** makes playing
harder brighter. Per operator: **RATIO** (0.5, 1, 2 ... 16), **FINE**
(multiplies the ratio by 1 + fine, for inharmonic tones), **LEVEL** (output
for carriers, modulation index for modulators) and an ADSR.

## Modulation matrix

Eight slots of **SOURCE → DESTINATION × AMOUNT** (negative amounts invert).
Sources: LFO 1/2, Env 1/2/3, Velocity, Key, Mod Wheel, Pitch Bend,
Aftertouch, Poly AT, Expression (CC 11), Random (per note), S&H (new value each
sequencer step), Seq Step, Macro. Destinations include pitch (all or per
oscillator), pulse width, levels, FM amount, wavetable position, FM depth and
feedback, cutoff, resonance, drive, HPF, amp, pan, LFO rates and amounts, and
effect amounts (drive, chorus mix, delay mix/feedback, reverb mix).

## Arpeggiator and sequencer

**Arpeggiator**: **ARP ON**, **HOLD** (latch), **MODE** (up, down, up/down,
random, as played, chord), **RATE** (tempo division), **OCTAVES**, **GATE**,
**SWING**. Follows the host's tempo and position when available.

**Step sequencer** (**MODE**):

* **MOD** — the 16 step values become the *Seq Step* matrix source; steps
  with SLIDE glide to the next value.
* **NOTE** — hold a key and the pattern plays monophonically, each step
  transposed ±12 semitones by its value. GATE off = rest, ACC = accent
  (full velocity), SLIDE = glide legato into the next step. NOTE mode
  takes over from the arpeggiator.

**LENGTH**, **RATE**, **GATE** and the shared **SWING** apply. The lit LED
marks the current step.

## Effects

Each module has an **ON** switch and fades in/out smoothly.

| Module | Controls |
|---|---|
| DRIVE | TYPE (overdrive, distortion, fold), DRIVE, TONE, MIX |
| CHORUS | RATE, DEPTH, MIX |
| ENSEMBLE | RATE, DEPTH, MIX (three-phase string ensemble) |
| PHASER | RATE, DEPTH, FDBK, MIX |
| FLANGER | RATE, DEPTH, FDBK (negative = hollow), MIX |
| DELAY | SYNC, P.PONG, TIME or DIVISION, FDBK, TONE, MIX |
| REVERB | SIZE, DAMP, PRE-DLY, MIX |
| STEREO | WIDTH (0 = mono, 100 % = unchanged, 200 % = wide), CHAR (output warmth) |

## Output

**MACRO** is a performance control available as a matrix source (one knob can
move several things). **LIMITER** keeps peaks below 0 dBFS. The display shows
sample rate, quality, CPU load, voices, peak level, limiter gain reduction and
the output guard (counts automatic recoveries from invalid audio). The meter
shows both channels with peak hold.

## MIDI

Note on/off with velocity, pitch bend, mod wheel (CC 1), expression (CC 11),
sustain (CC 64), channel and polyphonic aftertouch, all-sound-off (CC 120),
reset controllers (CC 121), all-notes-off (CC 123).

**MIDI Learn**: right-click a control, choose *MIDI Learn*, move a knob on
your controller. Right-click again to clear. CC 1, 11, 64 and 120+ are
reserved. Mappings are saved with the session.

## Presets

Factory presets are read-only; save a copy with SAVE AS. In the browser:
type to search, filter by category, star favourites, single-click to audition,
double-click to load and close, RENAME and DELETE (with confirmation) for user
presets. User presets are plain files (see `PRESET_FORMAT.md`) and can be
copied between computers.

## Troubleshooting

* **No sound** — check the AUDIO light. If it is dark, no audio device is
  running: open Options > Audio/MIDI Settings. Check MASTER, and that MIDI CH
  is Omni or matches your keyboard.
* **Keyboard does nothing** — open Options > Audio/MIDI Settings and enable
  your MIDI input (it is enabled automatically when present at start-up).
* **Clicks or dropouts** — increase the audio buffer size, or set QUALITY to
  Eco 1x; the OUTPUT display shows CPU load.
* **Stuck notes** — press PANIC.
