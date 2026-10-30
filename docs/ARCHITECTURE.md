# ZYNTH architecture

## Layers

```
 ┌───────────────────────── Standalone app (Source/App) ─────────┐
 │  JUCE StandaloneFilterWindow: audio/MIDI devices, settings    │
 └───────────────┬───────────────────────────────────────────────┘
                 │ same AudioProcessor in every format
 ┌───────────────▼───────────── Plugin (Source/Plugin) ──────────┐   VST3 / AU wrappers
 │ ZSynthProcessor                                                │◄──(JUCE)
 │  • APVTS  ← built from the parameter table                     │
 │  • MidiKeyboardState (on-screen keyboard)  • MidiLearn         │
 │  • PresetManager     • state save/restore  • CPU/status        │
 └──────┬─────────────────────────────┬──────────────────────────┘
        │ float snapshot + MIDI events│ atomics (status only)
 ┌──────▼──────────────────┐   ┌──────▼──────────────────────────┐
 │ SynthEngine (Source/DSP)│   │ Editor + MainPanel (Source/UI)   │
 │ JUCE-free C++17         │   │ fixed 1920x1200 canvas, scaled   │
 └─────────────────────────┘   └──────────────────────────────────┘
```

* **Parameters** (`Source/Parameters/ParamTable.h`) — one X-macro table
  declares every parameter: string id, name, group, type, range, default,
  scaling, unit, choices and help text. It generates the dense `P::` enum that
  the engine indexes, the JUCE `AudioProcessorValueTreeState`, the preset
  serialisation, the tooltips and the panel bindings. Nothing else defines a
  parameter. Choice lists sit next to the enums the DSP uses
  (`ParamChoices.h`); a test checks the counts agree.
* **Engine** (`Source/DSP`) — has no JUCE dependency. The processor hands it a
  `float[294]` snapshot of real-unit values and the block's raw MIDI bytes.
  `zsynth_dsp` is a static library linked by the plug-in, the tests and the
  benchmark.
* **Plug-in** — the only host-facing object. Standalone, VST3 and AU all wrap
  the same `ZSynthProcessor`; nothing in it is standalone-specific.
* **UI** — never owns synthesis state. Every control is bound to one APVTS
  parameter through `juce::ParameterAttachment`; displays read parameter
  atomics and engine status atomics on a 30 Hz timer.

## Signal flow (per voice, then global)

```
OSC1 ─┐ (FM from OSC3, sub locked to OSC1)
OSC2 ─┤ (sync/FM/ring from OSC1)                     ┌─ ENV1 (amp, per sample)
OSC3 ─┤ (sync/FM/ring from OSC1)                     │
SUB  ─┼─► MIXER ─► overdrive ─► HPF 12 dB ─► FILTER ─► VCA ─► PAN ─► voice bus
NOISE─┤                          (Ladder/OTA/SEM/K35)
FM 6-op ┘ (Hybrid / FM engine)
          ▲ modulation per control block: ENV2/3, LFO1/2, matrix, glide, drift, vintage

voice bus (2x) ─► half-band decimator ─► DRIVE (2x) ─► PHASER ─► FLANGER ─► CHORUS
  ─► ENSEMBLE ─► DELAY ─► REVERB ─► DC block ─► WIDTH ─► CHARACTER ─► MASTER ─► LIMITER
```

## Rendering and timing

`SynthEngine::process` splits each host block into **segments** that end at

* the next incoming MIDI event (sample-accurate note timing),
* the next arpeggiator or sequencer event (sample-accurate generated notes),
* the end of a control block (32 internal samples).

At each segment start the engine smooths continuous parameters (one-pole,
15 ms; logarithmic parameters smoothed in the log domain) and derives a
`VoiceShared` structure once for all voices (envelope coefficients, operator
ratios, filter settings, matrix routings). Voices then compute modulation at
segment rate and interpolate within the segment: oscillator increments
linearly, filter coefficient `g` exponentially (so fast filter envelopes stay
smooth), and VCA/pan gains linearly. The amp envelope runs per sample.
Audio-rate modulation (oscillator FM, ring, sync, FM operators) is per sample.

Host blocks larger than the prepared size are processed in chunks, so the
engine never allocates in `process`.

## Anti-aliasing strategy

* **Oscillators**: 2-point PolyBLEP (steps) and PolyBLAMP (slope changes) at
  the exact sub-sample time of every edge, with one sample of lookahead so the
  sample *before* the edge is corrected too. Hard-sync resets use the same
  machinery. Measured in-band aliasing at the 2x rate: saw −54.5 dB, pulse
  −58 dB, triangle −85.6 dB, synced saw −57 dB (2.79 kHz fundamental).
* **Wavetables**: each frame stored at 11 octave-spaced band limits; the
  oscillator picks the richest mip below Nyquist.
* **Voice oversampling**: voices run at 2x below 80 kHz host rates (Quality:
  Auto) and are decimated by a 63-tap Kaiser half-band FIR (17 non-zero taps,
  ≥70 dB rejection of the folding band). Eco mode runs at 1x for CPU.
* **Drive effect**: its own 2x half-band up/down sampling around the shaper.
* Filters are zero-delay-feedback (TPT) designs, stable under audio-rate
  cutoff modulation; nonlinear stages use a bounded Padé `tanh`.

## Filters

| Model | Topology | Character |
|---|---|---|
| Ladder | 4 transistor stages, tanh per stage, solved ZDF by linearising each `tanh(x)/x` at the previous sample; Xpander-style pole mixing for BP/HP/notch | Warm, resonance thins the bass, self-oscillates |
| OTA | 4 linear one-poles, saturation at input and in the resonance path, resonance gain compensation | Brighter, punchier, keeps bass at high resonance |
| SEM | TPT state-variable, saturating band integrator; 24 dB = Butterworth + resonant section | Smooth, open multimode; stops short of self-oscillation |
| K35 | Sallen-Key with nonlinear feedback (after Pirkle's TPT form); 12 dB LP, resonant 6 dB HP | Aggressive, screaming resonance |

## Voice management

`VoiceManager` owns 32 voices. Poly mode allocates *unison × notes* physical
voices from the first *polyphony* slots: Rotate (round robin), Reuse (same
voice per note) or Lowest. Stealing prefers released voices, then
pedal-sustained voices, then held ones, oldest first; a stolen voice fades out
over ~3 ms before its new note starts. Mono/Legato modes keep a held-note
stack with Last/Low/High priority. Each voice has fixed per-slot "component
tolerances" scaled by the VINTAGE control, plus slow per-oscillator drift.

## Real-time safety

* All buffers, delay lines, wavetables and voices are allocated in
  `prepare()` / constructors. `process()` performs no allocation, locking or
  I/O. MIDI events are copied into a pre-reserved vector.
* GUI → audio: only through APVTS parameter atomics and the
  `MidiKeyboardState` (JUCE's lock-free path). Wheels use atomics.
* Audio → GUI: status atomics (voice mask, peaks, sequencer step, tempo,
  limiter gain, CPU load).
* MIDI learn: the audio thread writes controller values into a lock-free FIFO;
  a message-thread timer applies them through the APVTS (≈33 ms latency), so
  hosts and the GUI see learned changes like any other automation.
* Presets load on the message thread by setting parameters; the engine smooths
  the changes.
* Denormals: FTZ/DAZ for the whole block plus explicit flushing in feedback
  paths. Non-finite output is detected, replaced by silence, and the voices and
  effects are reset (counted on the OUTPUT display as "GUARD").

## State and presets

* **Plug-in state** (`getStateInformation`): XML containing a full preset
  (every parameter, including global settings) plus the MIDI-learn map.
* **Presets** recall the *sound* only. The Global group (master volume, tune,
  transpose, octave, tempo, quality, MIDI channel) belongs to the user's setup
  and is restored only with the plug-in state.
* **Format**: versioned XML with parameter ids and readable values (choice
  labels, `on`/`off`). See `PRESET_FORMAT.md`.
* **Factory library**: a plain-text bank embedded at build time.

## The fixed panel

`UI/Layout/PanelLayout.cpp` is the canonical specification: every section,
control and utility element has one rectangle on a 1920 x 1200 canvas, or on
one of three fixed-layout popup *pages* (modulation matrix, FM operators,
sequencer steps) that float over the canvas at fixed positions. The
`MainPanel` creates every control from that table and never computes
positions. `ZSynthEditor` applies a single uniform `AffineTransform::scale` to
the panel and locks the window's aspect ratio (0.5x to 1.5x). Controls that do
not apply to the current mode are dimmed with an explanatory tooltip by
`ActivityRules`, never moved or hidden. Tests verify: every parameter appears
exactly once across the main panel and popups, controls lie inside their
sections and never overlap on the same page, only the matrix / FM operator /
sequencer-step parameters live in popups, and no component moves across
presets, engine modes, window sizes or when the browser or a popup opens.

## Adding a parameter

1. Add one `X(...)` line to `ZS_PARAM_LIST` in `ParamTable.h` (new, never
   reused, string id).
2. Read it in `SynthEngine::computeShared` (or `updateBlockSettings`).
3. Place a control for it in `PanelLayout.cpp` (the layout test fails until
   you do).
4. Old presets load unchanged: missing parameters take their defaults.

If an id must ever change, add a mapping in `migrateParamId`
(`PresetFormat.cpp`) and bump `kPresetFormatVersion`.
