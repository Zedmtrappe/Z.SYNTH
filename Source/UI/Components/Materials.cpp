#include "Materials.h"

namespace zsynth::ui::material
{

namespace
{
    /** Deterministic white noise in [-1, 1]. */
    struct Noise
    {
        uint32_t state;
        explicit Noise(uint32_t seed) : state(seed) {}
        float next()
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state & 0xffffff) / (float) 0x7fffff - 1.0f;
        }
    };

    juce::Colour signedPixel(float v, float amount)
    {
        const float a = juce::jlimit(0.0f, 1.0f, std::abs(v) * amount);
        return (v >= 0.0f ? juce::Colours::white : juce::Colours::black).withAlpha(a);
    }

    /** Horizontally brushed grain: each row is noise smeared along x (wrapping,
        so the tile repeats seamlessly), with its own brightness offset. */
    const juce::Image& brushedTile()
    {
        static const juce::Image tile = [] {
            constexpr int w = 1024, h = 128, blur = 48;
            juce::Image img(juce::Image::ARGB, w, h, true);
            Noise n(0xb105f00du);
            std::vector<float> row((size_t) w), smooth((size_t) w);
            for (int y = 0; y < h; ++y)
            {
                for (auto& v : row)
                    v = n.next();
                float acc = 0.0f;
                for (int k = -blur; k <= blur; ++k)
                    acc += row[(size_t) ((k + w) % w)];
                for (int x = 0; x < w; ++x)
                {
                    smooth[(size_t) x] = acc / (float) (2 * blur + 1);
                    acc += row[(size_t) ((x + blur + 1) % w)] - row[(size_t) ((x - blur + w) % w)];
                }
                const float rowOffset = n.next() * 0.3f;
                for (int x = 0; x < w; ++x)
                {
                    const float v = rowOffset + smooth[(size_t) x] * 3.5f + n.next() * 0.3f;
                    img.setPixelAt(x, y, signedPixel(v, 0.1f));
                }
            }
            return img;
        }();
        return tile;
    }

    /** Fine, isotropic speckle for plastic. */
    const juce::Image& speckleTile()
    {
        static const juce::Image tile = [] {
            constexpr int s = 256;
            juce::Image img(juce::Image::ARGB, s, s, true);
            Noise n(0x5ca1ab1eu);
            std::vector<float> v((size_t) (s * s));
            for (auto& x : v)
                x = n.next();
            for (int y = 0; y < s; ++y)
                for (int x = 0; x < s; ++x)
                {
                    // 2x2 average softens the speckle into a satin grain.
                    const float a = v[(size_t) (y * s + x)] + v[(size_t) (y * s + (x + 1) % s)]
                                  + v[(size_t) (((y + 1) % s) * s + x)] + v[(size_t) (((y + 1) % s) * s + (x + 1) % s)];
                    img.setPixelAt(x, y, signedPixel(a * 0.25f, 0.22f));
                }
            return img;
        }();
        return tile;
    }

    juce::Path rounded(juce::Rectangle<float> r, float radius)
    {
        juce::Path p;
        p.addRoundedRectangle(r, radius);
        return p;
    }
}

void brushedMetal(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour base, float grain)
{
    const auto p = rounded(r, radius);
    g.saveState();
    g.reduceClipRegion(p);
    juce::ColourGradient body(base.brighter(0.16f), r.getX(), r.getY(), base.darker(0.28f), r.getX(), r.getBottom(), false);
    g.setGradientFill(body);
    g.fillRect(r);
    g.setTiledImageFill(brushedTile(), 0, 0, juce::jlimit(0.0f, 1.0f, 0.6f * grain));
    g.fillRect(r);
    // Broad anisotropic sheen of brushed metal under a top-left light.
    juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.07f), r.getX() + r.getWidth() * 0.28f, r.getY(),
                               juce::Colours::white.withAlpha(0.0f), r.getX() + r.getWidth() * 0.28f, r.getY() + r.getHeight() * 0.85f, true);
    g.setGradientFill(sheen);
    g.fillRect(r);
    g.restoreState();
}

void texturedPlastic(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour base, float grain)
{
    const auto p = rounded(r, radius);
    g.saveState();
    g.reduceClipRegion(p);
    juce::ColourGradient body(base.brighter(0.1f), r.getX(), r.getY(), base.darker(0.2f), r.getX(), r.getBottom(), false);
    g.setGradientFill(body);
    g.fillRect(r);
    g.setTiledImageFill(speckleTile(), 0, 0, juce::jlimit(0.0f, 1.0f, 0.6f * grain));
    g.fillRect(r);
    g.restoreState();
}

void raisedEdge(juce::Graphics& g, juce::Rectangle<float> r, float radius, float strength)
{
    g.setColour(juce::Colours::black.withAlpha(0.75f * strength));
    g.drawRoundedRectangle(r.expanded(0.5f), radius + 0.5f, 1.0f);
    juce::ColourGradient edge(juce::Colours::white.withAlpha(0.26f * strength), 0.0f, r.getY(),
                              juce::Colours::black.withAlpha(0.55f * strength), 0.0f, r.getBottom(), false);
    edge.addColour(0.12, juce::Colours::white.withAlpha(0.06f * strength));
    edge.addColour(0.8, juce::Colours::transparentBlack);
    g.setGradientFill(edge);
    g.drawRoundedRectangle(r.reduced(0.75f), radius, 1.5f);
}

void recess(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour fill, float depth)
{
    const auto p = rounded(r, radius);
    g.setColour(fill);
    g.fillPath(p);

    // Inner shadow: the panel surface around the hole casts onto its floor.
    g.saveState();
    g.reduceClipRegion(p);
    juce::Path ring;
    ring.addRectangle(r.expanded(24.0f));
    ring.addRoundedRectangle(r, radius);
    ring.setUsingNonZeroWinding(false);
    juce::DropShadow(juce::Colours::black.withAlpha(juce::jlimit(0.0f, 1.0f, 0.85f * depth)), juce::roundToInt(3.0f + 4.0f * depth),
                     { 0, juce::roundToInt(1.0f + 1.5f * depth) }).drawForPath(g, ring);
    g.restoreState();

    // Lit lower lip and a dark upper wall edge.
    juce::ColourGradient lip(juce::Colours::black.withAlpha(0.7f), 0.0f, r.getY(), juce::Colours::white.withAlpha(0.13f), 0.0f, r.getBottom(), false);
    lip.addColour(0.5, juce::Colours::transparentBlack);
    g.setGradientFill(lip);
    g.drawRoundedRectangle(r.expanded(0.5f), radius + 0.5f, 1.0f);
}

void castShadow(juce::Graphics& g, const juce::Path& p, float blurRadius, juce::Point<int> offset, float alpha)
{
    juce::DropShadow(juce::Colours::black.withAlpha(juce::jlimit(0.0f, 1.0f, alpha)), juce::jmax(1, juce::roundToInt(blurRadius)), offset)
        .drawForPath(g, p);
}

void castShadow(juce::Graphics& g, juce::Rectangle<float> r, float radius, float blurRadius, juce::Point<int> offset, float alpha)
{
    castShadow(g, rounded(r, radius), blurRadius, offset, alpha);
}

void screw(juce::Graphics& g, juce::Point<float> c, float radius, float angle)
{
    const auto disc = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(c);
    // Countersink shadow ring.
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.fillEllipse(disc.expanded(1.2f));
    juce::ColourGradient head(juce::Colour(0xffb9bcc2), disc.getX(), disc.getY(), juce::Colour(0xff3d3f44), disc.getRight(), disc.getBottom(), false);
    head.addColour(0.45, juce::Colour(0xff7d8087));
    g.setGradientFill(head);
    g.fillEllipse(disc);
    // Phillips cross: dark cut with a lit lower edge.
    for (int k = 0; k < 2; ++k)
    {
        const float a = angle + (float) k * juce::MathConstants<float>::halfPi;
        const juce::Point<float> d(std::cos(a) * radius * 0.62f, std::sin(a) * radius * 0.62f);
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawLine(juce::Line<float>(c - d, c + d).withShortenedStart(0.0f).withShortenedEnd(0.0f), 1.0f);
        g.setColour(juce::Colour(0xff15161a));
        g.drawLine(juce::Line<float>(c - d + juce::Point<float>(0.0f, -0.6f), c + d + juce::Point<float>(0.0f, -0.6f)),
                   juce::jmax(1.0f, radius * 0.28f));
    }
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawEllipse(disc.reduced(0.6f), 0.6f);
}

juce::Path textPath(const juce::String& text, juce::Rectangle<float> r, const juce::Font& font, juce::Justification just,
                    int maxLines, float minScale)
{
    juce::GlyphArrangement ga;
    ga.addFittedText(font, text, r.getX(), r.getY(), r.getWidth(), r.getHeight(), just, maxLines, minScale);
    juce::Path p;
    ga.createPath(p);
    return p;
}

void engravedText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& font,
                  juce::Justification just, juce::Colour light, float glow, int maxLines)
{
    if (text.isEmpty())
        return;
    const auto p = textPath(text, r, font, just, maxLines);
    if (glow > 0.0f)
        juce::DropShadow(light.withAlpha(juce::jlimit(0.0f, 1.0f, 0.38f * glow)), 5, {}).drawForPath(g, p);
    // The cut walls: shadowed above, a lit lip below.
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillPath(p, juce::AffineTransform::translation(-0.3f, -0.9f));
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.fillPath(p, juce::AffineTransform::translation(0.2f, 0.9f));
    // Light through the diffuser: brighter in the middle of each line.
    const auto b = p.getBounds();
    juce::ColourGradient lit(light.darker(0.15f), 0.0f, b.getY(), light.darker(0.25f), 0.0f, b.getBottom(), false);
    lit.addColour(0.45, light.brighter(0.15f));
    g.setGradientFill(lit);
    g.fillPath(p);
}

void etchedText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& font,
                juce::Justification just, float brightness)
{
    if (text.isEmpty())
        return;
    const auto p = textPath(text, r, font, just);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.fillPath(p, juce::AffineTransform::translation(0.0f, -0.8f));
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.fillPath(p, juce::AffineTransform::translation(0.0f, 0.8f));
    g.setColour(Colours::text.withMultipliedBrightness(brightness));
    g.fillPath(p);
}

void engravedLine(juce::Graphics& g, juce::Line<float> l, float thickness)
{
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawLine(l, thickness);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawLine(l.getStartX(), l.getStartY() + thickness, l.getEndX(), l.getEndY() + thickness, thickness);
}

void glassWindow(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour glass)
{
    recess(g, r, radius, glass, 1.0f);
    // Depth: the glass is darker towards its edges.
    juce::ColourGradient vignette(juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                  juce::Colours::black.withAlpha(0.35f), r.getX(), r.getY(), true);
    g.saveState();
    g.reduceClipRegion(rounded(r, radius));
    g.setGradientFill(vignette);
    g.fillRect(r);
    g.restoreState();
}

void glassSheen(juce::Graphics& g, juce::Rectangle<float> r, float radius, float strength)
{
    g.saveState();
    g.reduceClipRegion(rounded(r, radius));
    juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.11f * strength), r.getX(), r.getY(),
                               juce::Colours::white.withAlpha(0.0f), r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.9f, false);
    g.setGradientFill(sheen);
    juce::Path band;
    band.startNewSubPath(r.getX(), r.getY());
    band.lineTo(r.getX() + r.getWidth() * 0.62f, r.getY());
    band.lineTo(r.getX() + r.getWidth() * 0.34f, r.getBottom());
    band.lineTo(r.getX(), r.getBottom());
    band.closeSubPath();
    g.fillPath(band);
    g.setColour(juce::Colours::white.withAlpha(0.08f * strength));
    g.drawHorizontalLine(juce::roundToInt(r.getY() + 1.5f), r.getX() + radius, r.getRight() - radius);
    g.restoreState();
}

void glowText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& font, juce::Colour colour,
              juce::Justification just, float glow, int maxLines)
{
    g.setFont(font);
    const auto ri = r.toNearestInt();
    if (glow > 0.0f)
    {
        g.setColour(colour.withAlpha(juce::jlimit(0.0f, 1.0f, 0.16f * glow)));
        for (auto d : { juce::Point<int>(-1, 0), juce::Point<int>(1, 0), juce::Point<int>(0, -1), juce::Point<int>(0, 1) })
            g.drawFittedText(text, ri + d, just, maxLines, 0.7f);
    }
    g.setColour(colour);
    g.drawFittedText(text, ri, just, maxLines, 0.7f);
}

void LegendCache::draw(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
                       juce::Justification just, juce::Colour light, float glow, int maxLines)
{
    const float scale = juce::jlimit(0.5f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto newKey = text + "|" + area.toString() + "|" + juce::String(font.getHeight()) + "|" + juce::String(scale)
                      + "|" + light.toString() + "|" + juce::String(glow) + "|" + juce::String(just.getFlags());
    if (newKey != key || image.isNull())
    {
        key = newKey;
        imageArea = area.expanded(7.0f);
        image = juce::Image(juce::Image::ARGB, juce::jmax(1, (int) std::ceil(imageArea.getWidth() * scale)),
                            juce::jmax(1, (int) std::ceil(imageArea.getHeight() * scale)), true);
        juce::Graphics ig(image);
        ig.addTransform(juce::AffineTransform::translation(-imageArea.getX(), -imageArea.getY()).scaled(scale));
        engravedText(ig, text, area, font, just, light, glow, maxLines);
    }
    g.drawImage(image, imageArea);
}

} // namespace zsynth::ui::material
