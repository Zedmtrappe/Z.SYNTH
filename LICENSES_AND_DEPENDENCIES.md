# Licences and dependencies

## Third-party code

| Dependency | Version | How it is used | Licence |
|---|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.9 (git tag) | Plug-in wrappers, standalone app, GUI, parameter state, audio/MIDI devices | Dual: **AGPLv3** or the commercial **JUCE 8 licence** |
| Steinberg VST3 SDK | bundled with JUCE 8.0.9 | VST3 wrapper | See `JUCE/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt` |
| ALSA, JACK, X11, FreeType, fontconfig (Linux) | system | Audio/MIDI I/O and windowing via JUCE | System libraries (LGPL/MIT-style); dynamically linked |

No other libraries are used. The DSP engine (`Source/DSP`, `Source/Parameters`)
depends only on the C++17 standard library.

## What this means for distribution

* **JUCE**: you must either release your product under the AGPLv3 (source
  available to every user) or hold a commercial JUCE licence appropriate to
  your revenue. This decision has not been made for Z.SYNTH; it must be made
  before any binary is distributed.
* **VST3**: read the licence file shipped inside the JUCE checkout at the path
  above and the Steinberg usage guidelines next to it before distributing VST3
  binaries or using the VST logo.
* **AU (macOS)**: uses Apple's system frameworks; no extra licence step.

## Original content

* All factory presets (`Resources/Presets/FactoryBank.zsbank`) are original.
* All wavetables are generated at start-up from mathematical descriptions in
  `Source/DSP/Wavetable/WavetableBank.cpp`. No sampled or commercial wavetables
  are included.
* The panel artwork is drawn in code; no images, fonts or trademarks of other
  manufacturers are used. Historical instruments are referred to only
  descriptively (e.g. "SEM-style") in documentation and comments.
* Fonts: the system default sans-serif and monospaced fonts are used; no font
  files are bundled.
