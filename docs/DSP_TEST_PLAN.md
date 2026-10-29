# Z.SYNTH test plan

All automated tests are deterministic (fixed seeds, synthetic input) and run
with `ctest --test-dir build --output-on-failure`.

## 1. Automated DSP tests (`ZSynthDspTests`, JUCE-free)

| Test | What it verifies | Measured result (Linux x86-64, Release) |
|---|---|---|
| oscillator_frequency_accuracy | Wrap count over 4 s for saw/pulse/tri/sine at 1234.5 Hz | ±0.5 Hz |
| oscillator_waveforms_bounded_and_dc_free | Peak < 1.2, mean < 0.01 for every waveform | pass |
| oscillator_aliasing_polyblep_vs_naive | In-band inharmonic energy, saw at 2.79 kHz, 48 kHz, 1x | naive −12.5 dB, PolyBLEP −34.1 dB |
| oscillator_aliasing_at_2x_internal_rate | Same at the 2x internal rate, 20 Hz–20 kHz | saw −54.5, pulse −58.0, triangle −85.6 dB |
| oscillator_hard_sync_is_band_limited | Synced saw, ratio 2.71, 2x | −57.1 dB |
| sub_oscillator_tracks_octave_below | Sub at exactly half of osc 1 | 220.0 Hz for 440 Hz |
| wavetables_normalised_and_band_limited | Every table: frame peak 1.0, wrap sample, mip 5 has no energy above harmonic 34 | pass |
| wavetable_mip_selection | Mip choice keeps harmonics below Nyquist | pass |
| filter_responses_all_models | 1 kHz LP/HP: passband and stopband one decade away | Ladder/OTA/SEM 24 dB: −73…−80 dB; K35 LP −36.6 dB, HP −20 dB (resonant 6 dB/oct by design) |
| filter_models_sound_different | Pairwise difference between models ≥ −20 dB | pass |
| filter_stability_under_abuse | +12 dB noise, audio-rate cutoff sweep 20 Hz–20 kHz, max resonance and drive, all models/types/slopes, 2 s | finite, no growth |
| filter_self_oscillation | Ladder, OTA, K35 ring at the cutoff with resonance at max | 995 / 1000 / 999 Hz |
| envelope_timing_and_levels | Attack time ±0.5 ms, sustain level, release −60 dB in the set time ±1 dB | pass |
| envelope_retrigger_has_no_jump | Retrigger during release starts from current level | pass |
| voice_allocation_and_stealing | 6 notes into 4 voices: newest 4 sound, all release to silence | pass |
| voice_note_off_correctness_overlapping_and_repeated | Overlapping and repeated notes release correctly | pass |
| sustain_pedal | Notes held while the pedal is down, released on pedal up | pass |
| unison_uses_stacked_voices | 2 notes x 5 unison = 10 voices | pass |
| mono_priority_and_legato | Low-note priority, return to held note | pass |
| engine_render_known_midi | Chord render: silent before, sound during, silent after release, peak < 1 | pass |
| engine_no_discontinuities_on_note_events | 8 short sine notes: max sample step < 0.08 | 0.0075 |
| engine_pitch_accuracy_and_bend | A4 at 44.1/48/96 kHz; full bend = +2 st | 440.00 / 440.01 / 440.01 Hz |
| engine_midi_channel_filter | Channel 2 only | pass |
| engine_sustain_pedal_cc64 | CC 64 through the engine | pass |
| engine_parameter_smoothing | Abrupt level change produces no step | pass |
| engine_all_sample_rates_and_block_sizes | 44.1, 48, 88.2, 96, 192 kHz x block sizes 1, 17, 64, 512, 2048 | finite, non-silent |
| engine_extreme_parameters_stay_finite | 40 random patches with parameters at min/max/random, 6 notes at velocity 127 | finite, limiter holds peaks ≤ 1 |
| engine_effects_stability_max_feedback | All effects, maximum feedback, 8 s | finite, bounded |
| engine_high_polyphony_and_cpu | 32 voices: 16 notes x 2 unison, 3 osc + 6-op FM, 2x, FX | ≈54 % of one 2.8 GHz Xeon core |
| engine_dense_midi_no_stuck_notes | 2000 random notes in 2 s, polyphony 8 | no voices left, silent tail |
| engine_spectrum_of_saw | Harmonic 2 at −6 dB, 4 at −12 dB; inharmonic < −40 dB | pass |
| fm_algorithms_respect_processing_order | Modulators always have higher index than targets | pass |
| fm_engine_produces_sidebands | HF energy rises with FM depth | 0.0000 → 0.0005 (>20x) |
| arpeggiator_timing | 1/16 at 120 BPM: notes every 6000 samples, correct order | exact |
| sequencer_note_mode_pattern_and_slide | Step pitches, rest step silent | pass |
| halfband_response | Passband flat to 0.4 x base rate; folding band < −70 dB | pass |

## 2. Automated state, preset and UI tests (`ZSynthStateTests`)

| Test | What it verifies |
|---|---|
| param_table_matches_apvts | 294 parameters, unique ids, defaults inside range and round-tripping |
| choice_lists_match_dsp_enums | Every choice list length equals its DSP enum |
| value_text_round_trip | Display text → value and preset text → value for min/mid/max of every parameter |
| preset_xml_round_trip | Random patch → XML → patch and through a file |
| invalid_presets_are_handled_safely | Wrong root, no version, newer version, unknown ids, bad/NaN/out-of-range values, truncated XML, binary junk, missing file |
| processor_state_round_trip_including_midi_learn | Full plug-in state including the MIDI-learn map; garbage state ignored |
| midi_learn_drives_parameter | Learn, then the controller moves the parameter; reserved CCs refused |
| preset_manager_user_workflow | Save, no silent overwrite, recall, rename, favourite, delete, factory protection, globals untouched, persistence across a new manager |
| factory_bank_parses_cleanly | 160 presets, no parse errors, unique names, categories, no near-duplicates |
| factory_presets_render_cleanly_and_level_matched | Every preset with a category phrase: finite, not silent, peak ≤ 0 dBFS, loudness −30…−9 dB (percussion: peak −14…0 dBFS) |
| factory_presets_at_other_sample_rates | Every 9th preset at 44.1 and 96 kHz |
| layout_every_parameter_exactly_once | Each parameter has exactly one control |
| layout_controls_inside_sections_and_not_overlapping | Containment and no overlaps (controls, utilities, sections) |
| activity_rules_examples | Inactive-state rules |
| editor_layout_is_fixed_across_presets_and_modes | No component moves across presets, engine modes, min/max window size, browser overlay; pure uniform scaling |
| processor_renders_midi_and_onscreen_keyboard | processBlock with MIDI, on-screen keyboard path, panic, mono bus |

Library measurements after the mastering pass: median loudness −18.0 dB,
80 % of presets between −22.8 and −18.0 dB, maximum peak −2.0 dBFS.

## 3. VST3 host test (`ZSynthVst3HostTest`)

Loads the built `Z.SYNTH.vst3` through JUCE's VST3 hosting code: discovery
metadata (name, instrument category), instantiation, parameter exposure,
rendering from MIDI, automation + state recall, editor creation.

## 4. Manual checks performed

* Standalone launched under Xvfb with ALSA (null device): panel renders,
  on-screen keyboard produces voices and output level (−18 dB peak on the
  meter), knob drags update value, display and response curve.
* Quit and relaunch restores preset and edited parameters.
* Panel inspected at default and minimum (960 x 600) size.

**Not performed** (no audio output or physical MIDI hardware in the
development environment): listening, external MIDI keyboard, real DAW
hosting, macOS/Windows builds.

## 5. Manual listening test plan (to run before release)

Use good headphones or monitors at a moderate, fixed level; 48 kHz, 256-sample
buffer. For each item, note pass/fail and comments.

**General**
1. Play every factory preset with a few notes and a chord; note any preset
   that is much louder/quieter than its neighbours, clicks, or sounds broken.
2. PANIC during a long reverb tail: silence within one buffer.
3. Change QUALITY between Eco and High on a bright saw lead at C7: High must
   have noticeably less inharmonic "whistle".

**Bass** (Classic Saw Bass, Fat Model Bass, Acid Line, Reese Growl, FM Slap Bass)
4. Fast repeated 16ths at C1–C2: no clicks at note starts, tight attack.
5. Legato glides: smooth, no zipper noise.
6. Acid Line: accents audibly brighter/louder, slides glide.

**Leads** (Solo Saw Lead, Sync Scream, Model Lead)
7. Mod wheel vibrato depth musical; aftertouch opens filter smoothly.
8. Sync sweep across the keyboard: no aliasing "birdies" in High quality.

**Brass and strings** (Analog Brass, Ensemble Strings, String Machine)
9. Brass attack "blat" with the filter envelope; chords sound broad.
10. Ensemble: rich movement without pitch seasickness.

**Pads** (Warm Analog Pad, Evolving Spectrum, Glass Choir)
11. Long held chords: smooth evolution, no zipper noise on LFO-swept cutoff
    or wavetable position.
12. Reverb tails decay naturally, no metallic ringing.

**FM** (FM Electric Piano, FM Tubular Bell, Crystal Bells, Metallic Feedback)
13. Electric piano: velocity changes the bark; no aliasing at the top octave.
14. Bells: clear inharmonic partials, long clean decay.

**Heavily modulated** (Sample and Hold Bleeps, Glitch Arp, Industrial Grind, Mod Sequence Pad)
15. Tempo sync locks to the internal tempo; no runaway levels.

**Filters** — on Init with a saw, sweep CUTOFF with RESONANCE at 0, 0.5, 0.9
for each MODEL:
16. Models are clearly distinct in character.
17. Self-oscillation (Ladder/OTA/K35) is a clean sine-like tone tracking KEY.
18. No crackle when sweeping fast.

**Robustness**
19. Mash 10 keys with sustain pedal down, release the pedal: everything stops.
20. Turn every effect on with maximum feedback: loud but contained by the
    limiter, recovers when switched off.
