#pragma once

// Model faceplates for the output compressor and the reverb: each compressor
// model and most reverb types get the panel finish, knobs, fader caps, meter
// and display colours of the hardware they are modelled on (style cues only:
// no logos). The house types (Hall, Room, Shimmer) keep the ZYNTH look.

#include "../../Parameters/ParamChoices.h"
#include "Theme.h"

namespace zsynth::ui
{

enum class KnobStyle
{
    Machined,    // ZYNTH: knurled skirt, spun cap, back-lit ring
    BlackRound,  // large glossy black knob, white line (LA-2A)
    Chrome,      // black knob on a polished skirt (1176)
    Ribbed,      // grey ribbed body with a coloured cap (SSL)
    ChickenHead, // bakelite pointer knob (EMT, spring units)
    BlackSkirt,  // big black knob with a flared skirt (Fairchild, PCM70)
    Silver       // solid aluminium knob with a dot (Bricasti, REV7)
};

enum class PlateFinish
{
    Brushed,    // brushed aluminium / steel
    Hammertone, // mottled hammer paint
    Paint,      // flat stove enamel
    Anodised    // black anodised
};

struct Skin
{
    const char* nameplate;   // engraved descriptor, e.g. "LEVELING AMPLIFIER"
    juce::Colour plate;
    PlateFinish finish;
    juce::Colour ink;        // printed legends and titles
    juce::Colour accent;     // fader light, highlights
    KnobStyle knob;
    juce::Colour knobColour; // body / cap colour
    juce::Colour faderCap;   // fader cap moulding
    juce::Colour faderInsert;
    juce::Colour lcdBack, lcdText;
    // Gain-reduction meter (compressors).
    juce::Colour meterFace, meterInk, meterBezel;
    bool meterLamp;
    const char* meterLabel;

    bool lightPlate() const { return plate.getPerceivedBrightness() > 0.45f; }
    /** Per-parameter knob cap colour (SSL colour coding); knobColour otherwise. */
    juce::Colour capFor(const juce::String& paramId) const;
};

/** The faceplate of a compressor model. */
const Skin& compSkin(CompModel model);
/** The faceplate of a reverb type, or nullptr for the house (ZYNTH) look. */
const Skin* reverbSkin(ReverbType type);

/** The faceplate of a classic EQ model, or nullptr for PRISTINE (the house look). */
const Skin* eqSkin(EqModel model);
/** The faceplate of a tape machine. */
const Skin& tapeSkin(TapeModel model);

/** The pattern sequencer's faceplate: black anodised, white legends, black
    skirted knobs and red LED readouts, after the 1970s analog sequencers. */
const Skin& sequencerSkin();
/** The same faceplate for the sequencer's rows (no nameplate of their own). */
const Skin& sequencerRowSkin();

/** Paints a faceplate: finish, screws, title and nameplate. `inset` = a plate
    fitted into a recessed module bay (main panel); otherwise a full page. */
void paintFaceplate(juce::Graphics&, juce::Rectangle<float>, const Skin&, const juce::String& title, bool inset);

} // namespace zsynth::ui
