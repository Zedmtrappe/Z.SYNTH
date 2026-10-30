# ZYNTH preset format

## User presets (`.zspreset`)

Stored one per file in the user preset folder:

| Platform | Folder |
|---|---|
| Linux | `~/.config/ZYNTH/Presets` |
| macOS | `~/Library/Application Support/ZYNTH/Presets` |
| Windows | `%APPDATA%\ZYNTH\Presets` |

Favourites are listed in `favourites.txt` in the parent folder.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ZSynthPreset formatVersion="1" name="My Bass" category="Bass" author="Dan" comment="optional">
  <Param id="osc1.wave" value="Saw"/>
  <Param id="osc1.octave" value="16'"/>
  <Param id="flt.cutoff" value="260"/>
  <Param id="voice.unison" value="off"/>
  ...
</ZSynthPreset>
```

* `formatVersion` — integer, currently `1`. Required.
* Every parameter is written. Values are in **real units** (Hz, seconds,
  cents, 0..1 amounts). Choice parameters are stored by **label**
  (e.g. `Saw`, `Ladder`, `1/16`); booleans as `on`/`off`.
* Files are written to a temporary file first and then swapped in, so a
  failed write never destroys an existing preset.

### Loading rules (robustness)

| Situation | Behaviour |
|---|---|
| Wrong root element, missing/invalid `formatVersion`, unparseable XML, file > 4 MB | Rejected with an error message; current sound unchanged |
| `formatVersion` newer than this build | Loaded best-effort with a warning |
| Unknown parameter id | Ignored (warning) |
| Missing parameter | Takes its default |
| Out-of-range number | Clamped |
| Invalid value (bad label, NaN, text) | Default used (warning) |
| Renamed parameter id | Mapped by `migrateParamId()` before lookup |

### What a preset recalls

Loading a preset sets every parameter **except** the Global group:
`master.volume`, `master.tune`, `master.transpose`, `master.octave`,
`master.tempo`, `master.oversampling`, `midi.channel`. Those belong to the
user's setup. (The plug-in's full session state does include them.)

## Factory banks

| Bank | File | Folders |
|---|---|---|
| ZYNTH Originals | `Resources/Presets/FactoryBank.zsbank` | sound categories |
| Classic Machines | `Resources/Presets/ClassicsBank.zsbank` | one per instrument |

Each bank is a plain-text file embedded into the binary at build time; the
`category` of a preset is its folder within the bank. Each preset starts
from the **init patch** (all defaults) and lists only what differs:

```
# comment
[Preset Name]
category = Bass
comment = One line describing the sound.
inherit = Some Earlier Preset      # optional: copy all values first
osc1.wave = Saw
osc2.on = on
flt.cutoff = 260
amp.level = 0.75
```

* Keys are parameter ids, values use the same text forms as `.zspreset`.
* `inherit` copies a preset defined *earlier* in the file; put it before the
  lines that change it.
* The test suite rejects the bank if any line fails to parse, names repeat,
  a preset lacks a category, two presets differ in fewer than three
  parameters, or any preset renders silent, non-finite, clipped or outside the
  loudness window.

### Growing the library

1. Design a sound in the app and **SAVE AS** — it appears in the browser.
2. To promote it to the factory bank, copy the differing values into a new
   `[Name]` block (or keep it as a user preset; no code change is needed).
3. Run `ZSynthStateTests --preset-report /tmp/wavs` to check its level and
   listen to the rendered WAV. Target loudness is about −18 dB (RMS of the
   loudest 400 ms) with peaks below −3 dBFS; percussion is judged by peak.

## Parameter ids

The complete list, with ranges, defaults and help text, is the
`ZS_PARAM_LIST` table in `Source/Parameters/ParamTable.h`. Ids are permanent:
they are also the host automation identifiers.
