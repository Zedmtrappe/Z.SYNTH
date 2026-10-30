#pragma once

// Physical materials for the ZYNTH panel: brushed and anodised steel plates,
// textured plastic, bevels, cast shadows, recesses, screws, glass windows and
// engraved, back-lit legends.
//
// LIGHTING MODEL. One soft light sits above the panel, slightly to the top
// left: upward-facing bevels catch it (bright top/left edges), downward faces
// fall into shadow, raised parts cast shadows down and slightly right, and
// recesses show a dark upper wall and a lit lower lip. Every helper follows
// this so the parts read as one object.
//
// The textures are generated procedurally once (no image assets) and tiled.

#include "Theme.h"

namespace zsynth::ui::material
{

/** Warm white of the back-lit legends. */
const juce::Colour kLegend { 0xfff3e9d6 };

/** Fills a rounded rectangle with dark anodised, horizontally brushed steel. */
void brushedMetal(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour base, float grain = 1.0f);

/** Fills a rounded rectangle with satin, finely speckled plastic. */
void texturedPlastic(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour base, float grain = 1.0f);

/** Light top/left and dark bottom/right edges of a raised part. */
void raisedEdge(juce::Graphics&, juce::Rectangle<float>, float radius, float strength = 1.0f);

/** Sunken area: darker fill, a shadowed upper wall and a lit lower lip. */
void recess(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour fill, float depth = 1.0f);

/** Soft shadow cast by a raised shape onto the surface below it. */
void castShadow(juce::Graphics&, const juce::Path&, float blurRadius, juce::Point<int> offset, float alpha);
void castShadow(juce::Graphics&, juce::Rectangle<float>, float radius, float blurRadius, juce::Point<int> offset, float alpha);

/** Countersunk Phillips screw head. */
void screw(juce::Graphics&, juce::Point<float> centre, float radius, float angle);

/** Text as an outline, laid out like Graphics::drawFittedText. */
juce::Path textPath(const juce::String&, juce::Rectangle<float>, const juce::Font&, juce::Justification, int maxLines = 1,
                    float minScale = 0.5f);

/** Legend cut through the panel and lit from behind: dark cut walls, a lit
    lower lip, a diffuse bloom around the letters and the lit letters. */
void engravedText(juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&, juce::Justification,
                  juce::Colour light = kLegend, float glow = 1.0f, int maxLines = 1);

/** Text etched into metal without back-lighting (scale marks, small print). */
void etchedText(juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&, juce::Justification,
                float brightness = 0.55f);

/** Engraved line (scale marks, dividers). */
void engravedLine(juce::Graphics&, juce::Line<float>, float thickness = 1.0f);

/** Recessed display window: bezel, dark glass, reflection. */
void glassWindow(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour glass);

/** Reflection across a glass surface (draw last, over the contents). */
void glassSheen(juce::Graphics&, juce::Rectangle<float>, float radius, float strength = 1.0f);

/** LCD/VFD text with a soft phosphor glow. */
void glowText(juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&, juce::Colour,
              juce::Justification, float glow = 1.0f, int maxLines = 1);

/** Caches an engraved, back-lit legend as an image at the device pixel scale,
    so components that repaint often draw it with a single blit. */
class LegendCache
{
public:
    void draw(juce::Graphics&, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
              juce::Justification just, juce::Colour light = kLegend, float glow = 1.0f, int maxLines = 1);
    void clear() { image = {}; }

private:
    juce::Image image;
    juce::String key;
    juce::Rectangle<float> imageArea;
};

} // namespace zsynth::ui::material
