#pragma once

// FABULOUS EDITION: a joke overlay that dresses ZYNTH up for the night out:
// a glossy sheen, glitter and glints, rhinestones, a pink faux-fur trim, a
// feather boa, gold-capped tassels, a disco ball, a satin bow, hearts, a
// lipstick kiss and a bedazzled edition badge. It sits on top of the whole
// panel and never takes mouse clicks, so every control still works.
// Everything is drawn procedurally (deterministic random), once, into a cache.

#include <juce_gui_basics/juce_gui_basics.h>

namespace zsynth::ui
{

class GlamOverlay : public juce::Component
{
public:
    GlamOverlay() { setInterceptsMouseClicks(false, false); }

    void paint(juce::Graphics& g) override
    {
        const float scale = juce::jlimit(1.0f, 2.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        if (cache.isNull() || std::abs(scale - cacheScale) > 0.01f || cacheSize != getLocalBounds())
            render(scale);
        g.drawImage(cache, getLocalBounds().toFloat());
    }

private:
    juce::Image cache;
    float cacheScale = 0.0f;
    juce::Rectangle<int> cacheSize;

    using C = juce::Colour;
    using P = juce::Point<float>;
    static constexpr float kTau = juce::MathConstants<float>::twoPi;

    static float rnd(juce::Random& r, float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); }

    // The pinks of the fur and feathers, from blush to fuchsia.
    static C furPink(juce::Random& r)
    {
        static const C shades[] = { C(0xffffd6ec), C(0xffffb3dc), C(0xffff8ccb), C(0xffff5fb4), C(0xfff23a9c), C(0xffffe8f4) };
        return shades[r.nextInt(6)];
    }

    void render(float scale)
    {
        cacheScale = scale;
        cacheSize = getLocalBounds();
        cache = juce::Image(juce::Image::ARGB, juce::jmax(1, juce::roundToInt((float) getWidth() * scale)),
                            juce::jmax(1, juce::roundToInt((float) getHeight() * scale)), true);
        juce::Graphics g(cache);
        g.addTransform(juce::AffineTransform::scale(scale));
        juce::Random r(0x5eb11e5);
        const auto b = getLocalBounds().toFloat();

        discoSpots(g, b, r);
        glitter(g, b, r);
        // Rhinestones along the header's light bar and round the display.
        for (float x = 24.0f; x < b.getWidth() - 10.0f; x += 26.0f)
            rhinestone(g, { x, 95.5f }, 4.2f, r);
        stoneFrame(g, { 432.0f, 7.0f, 412.0f, 82.0f }, r);
        hearts(g, r);
        kiss(g, { 905.0f, 92.0f }, -0.25f);
        sticker(g, { 1452.0f, 452.0f }, 0.18f, "SLAY", C(0xffff3fa4));
        sticker(g, { 1172.0f, 935.0f }, -0.2f, "ICON", C(0xffb05cff));
        furTrim(g, b, r);
        boa(g, r);
        for (auto x : { 10.0f, 996.0f, 1910.0f })
            tassel(g, { x, x < 20.0f || x > 1900.0f ? 12.0f : 97.0f }, x < 20.0f || x > 1900.0f ? 120.0f : 72.0f, r);
        discoBall(g, { 1334.0f, 0.0f }, { 1334.0f, 128.0f }, 27.0f, r);
        bow(g, { 366.0f, 79.0f }, 0.55f);
        badge(g, { 76.0f, 69.0f, 262.0f, 21.0f }, r);
    }

    // ------------------------------------------------------------- shine
    void sheen(juce::Graphics& g, juce::Rectangle<float> b)
    {
        // Diagonal gloss bands, as if the whole case were lacquered.
        for (auto band : { P(-200.0f, 120.0f), P(380.0f, 60.0f), P(1040.0f, 180.0f), P(1500.0f, 70.0f) })
        {
            juce::Path p;
            const float x = band.x, w = band.y;
            p.startNewSubPath(x, b.getBottom());
            p.lineTo(x + 700.0f, 0.0f);
            p.lineTo(x + 700.0f + w, 0.0f);
            p.lineTo(x + w, b.getBottom());
            p.closeSubPath();
            juce::ColourGradient grad(C(0x00ffffff), x, 0.0f, C(0x00ffffff), x + w, 0.0f, false);
            grad.addColour(0.5, C(0x1affffff));
            grad.point1 = { x + 350.0f, b.getCentreY() };
            grad.point2 = { x + 350.0f + w, b.getCentreY() };
            g.setGradientFill(grad);
            g.fillPath(p);
        }
        // A rosy bloom from the top, darker plum at the bottom.
        juce::ColourGradient wash(C(0x22ff9ad6), 0.0f, 0.0f, C(0x18600040), 0.0f, b.getBottom(), false);
        g.setGradientFill(wash);
        g.fillRect(b);
    }

    void discoSpots(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        // Soft spots of light thrown by the disco ball.
        for (int i = 0; i < 46; ++i)
        {
            const P c(rnd(r, 0, b.getWidth()), rnd(r, 100, b.getHeight()));
            const float rad = rnd(r, 6, 18);
            const C col = r.nextBool() ? C(0xffffffff) : C(0xffffc2ea);
            juce::ColourGradient spot(col.withAlpha(rnd(r, 0.12f, 0.26f)), c.x, c.y, col.withAlpha(0.0f), c.x + rad, c.y, true);
            g.setGradientFill(spot);
            g.fillEllipse(c.x - rad, c.y - rad * 0.8f, rad * 2.0f, rad * 1.6f);
        }
    }

    void glint(juce::Graphics& g, P c, float size, C col)
    {
        // Four-point star with a soft halo.
        juce::ColourGradient halo(col.withAlpha(0.45f), c.x, c.y, col.withAlpha(0.0f), c.x + size * 1.1f, c.y, true);
        g.setGradientFill(halo);
        g.fillEllipse(c.x - size * 1.1f, c.y - size * 1.1f, size * 2.2f, size * 2.2f);
        juce::Path star;
        const float t = size * 0.13f;
        star.startNewSubPath(c.x, c.y - size);
        star.quadraticTo(c.x + t, c.y - t, c.x + size, c.y);
        star.quadraticTo(c.x + t, c.y + t, c.x, c.y + size);
        star.quadraticTo(c.x - t, c.y + t, c.x - size, c.y);
        star.quadraticTo(c.x - t, c.y - t, c.x, c.y - size);
        star.closeSubPath();
        g.setColour(col);
        g.fillPath(star);
        g.setColour(C(0xffffffff));
        g.fillEllipse(c.x - t, c.y - t, t * 2.0f, t * 2.0f);
    }

    void glitter(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        static const C sparkle[] = { C(0xffffffff), C(0xffffd1ee), C(0xffff7cc8), C(0xffffe08a), C(0xffd8b4ff), C(0xffa8f0ff) };
        // Fine glitter everywhere...
        for (int i = 0; i < 1600; ++i)
        {
            const P c(rnd(r, 0, b.getWidth()), rnd(r, 0, b.getHeight()));
            const float s = rnd(r, 0.7f, 2.1f);
            g.setColour(sparkle[r.nextInt(6)].withAlpha(rnd(r, 0.25f, 0.85f)));
            if (r.nextInt(3) == 0)
            {
                juce::Path hex;
                hex.addPolygon(c, 6, s, rnd(r, 0, kTau));
                g.fillPath(hex);
            }
            else
            {
                g.fillRect(c.x, c.y, s, s);
            }
        }
        // ...and the odd flash where a flake catches the light.
        for (int i = 0; i < 60; ++i)
            glint(g, { rnd(r, 0, b.getWidth()), rnd(r, 0, b.getHeight()) }, rnd(r, 3.5f, 9.0f), sparkle[r.nextInt(6)]);
    }

    // ------------------------------------------------------------- bling
    void rhinestone(juce::Graphics& g, P c, float rad, juce::Random& r)
    {
        static const C stones[] = { C(0xffffffff), C(0xffff8fd0), C(0xffe0b0ff), C(0xffffc6e8), C(0xffb0eaff) };
        const C stone = stones[r.nextInt(5)];
        // Silver claw setting.
        g.setColour(C(0x66000000));
        g.fillEllipse(c.x - rad - 0.6f, c.y - rad + 0.8f, (rad + 0.6f) * 2.0f, (rad + 0.6f) * 2.0f);
        juce::ColourGradient rim(C(0xfff4f4f8), c.x - rad, c.y - rad, C(0xff8a8a96), c.x + rad, c.y + rad, false);
        g.setGradientFill(rim);
        g.fillEllipse(c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
        // The faceted stone: dark-to-bright gradient, facet lines, a hot highlight.
        const float s = rad * 0.8f;
        juce::ColourGradient body(stone.darker(0.55f), c.x + s, c.y + s, stone.brighter(0.4f), c.x - s, c.y - s, false);
        body.addColour(0.55, stone);
        g.setGradientFill(body);
        g.fillEllipse(c.x - s, c.y - s, s * 2.0f, s * 2.0f);
        juce::Path facets;
        facets.addPolygon(c, 8, s * 0.5f, 0.2f);
        for (int k = 0; k < 8; ++k)
        {
            const float a = 0.2f + (float) k / 8.0f * kTau;
            facets.startNewSubPath(c.x + std::sin(a) * s * 0.5f, c.y - std::cos(a) * s * 0.5f);
            facets.lineTo(c.x + std::sin(a) * s, c.y - std::cos(a) * s);
        }
        g.setColour(C(0x66ffffff));
        g.strokePath(facets, juce::PathStrokeType(0.5f));
        g.setColour(C(0xeeffffff));
        g.fillEllipse(c.x - s * 0.55f, c.y - s * 0.6f, s * 0.55f, s * 0.45f);
        if (r.nextInt(4) == 0)
            glint(g, { c.x - s * 0.3f, c.y - s * 0.35f }, rad * 1.6f, C(0xffffffff));
    }

    void stoneFrame(juce::Graphics& g, juce::Rectangle<float> f, juce::Random& r)
    {
        for (float x = f.getX(); x <= f.getRight() + 0.1f; x += 13.0f)
        {
            rhinestone(g, { x, f.getY() }, 3.6f, r);
            rhinestone(g, { x, f.getBottom() }, 3.6f, r);
        }
        for (float y = f.getY() + 13.0f; y < f.getBottom() - 6.0f; y += 13.0f)
        {
            rhinestone(g, { f.getX(), y }, 3.6f, r);
            rhinestone(g, { f.getRight(), y }, 3.6f, r);
        }
    }

    // ------------------------------------------------------------- fluff
    void furTuft(juce::Graphics& g, P base, float angle, float length, juce::Random& r, int strands, float spread, float thickness = 1.0f)
    {
        for (int k = 0; k < strands; ++k)
        {
            const float a = angle + rnd(r, -spread, spread);
            const float len = length * rnd(r, 0.55f, 1.15f);
            const float curl = rnd(r, -1.1f, 1.1f);
            const P tip(base.x + std::cos(a) * len, base.y + std::sin(a) * len);
            const P mid(base.x + std::cos(a + curl) * len * 0.55f, base.y + std::sin(a + curl) * len * 0.55f);
            juce::Path p;
            p.startNewSubPath(base);
            p.quadraticTo(mid, tip);
            g.setColour(furPink(r).withAlpha(rnd(r, 0.55f, 0.95f)));
            g.strokePath(p, juce::PathStrokeType(rnd(r, 1.0f, 2.4f) * thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void furTrim(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        // A fluffy pink trim round the whole case: a dense base, then wisps.
        const float w = b.getWidth(), h = b.getHeight();
        auto edge = [&](P from, P to, float inward, float depth) {
            const float len = from.getDistanceFrom(to);
            const P in(std::cos(inward), std::sin(inward));
            // Soft plush underlayer, then curly strands on top.
            juce::Path base;
            base.startNewSubPath(from);
            base.lineTo(to);
            g.setColour(C(0xccff8ccb));
            g.strokePath(base, juce::PathStrokeType(depth * 0.9f));
            for (float d = 0.0f; d < len; d += 1.1f)
            {
                const P at = from + (to - from) * (d / len) + in * rnd(r, -2.0f, depth * 0.35f);
                furTuft(g, at, inward + rnd(r, -0.5f, 0.5f), rnd(r, 0.5f, 1.0f) * depth, r, 3, 1.4f, 1.1f);
            }
        };
        const float down = kTau * 0.25f, up = -down, right = 0.0f, left = kTau * 0.5f;
        edge({ 0, 0 }, { w, 0 }, down, 8.0f);
        edge({ 0, h }, { w, h }, up, 15.0f);
        edge({ 0, 0 }, { 0, h }, right, 15.0f);
        edge({ w, 0 }, { w, h }, left, 15.0f);
    }

    void boa(juce::Graphics& g, juce::Random& r)
    {
        // A feather boa slung across the panel above the EFFECTS row, in three swags.
        juce::Path spine;
        spine.startNewSubPath(-20.0f, 752.0f);
        spine.cubicTo(250.0f, 800.0f, 420.0f, 800.0f, 640.0f, 758.0f);
        spine.cubicTo(860.0f, 806.0f, 1080.0f, 808.0f, 1290.0f, 756.0f);
        spine.cubicTo(1500.0f, 804.0f, 1720.0f, 798.0f, 1940.0f, 750.0f);
        const float len = spine.getLength();
        // Shadow under it first.
        g.setColour(C(0x44300018));
        g.strokePath(spine, juce::PathStrokeType(34.0f), juce::AffineTransform::translation(3.0f, 9.0f));
        g.setColour(C(0xffe0499e));
        g.strokePath(spine, juce::PathStrokeType(20.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (int layer = 0; layer < 2; ++layer)
            for (float d = 0.0f; d < len; d += 1.2f)
            {
                const P at = spine.getPointAlongPath(d) + P(rnd(r, -4.0f, 4.0f), rnd(r, -4.0f, 4.0f));
                furTuft(g, at, rnd(r, 0.0f, kTau), rnd(r, 8.0f, layer == 0 ? 22.0f : 15.0f), r, 4, 1.2f, 1.3f);
            }
        // A few long plumes sticking out.
        for (int i = 0; i < 90; ++i)
        {
            const P at = spine.getPointAlongPath(rnd(r, 0.0f, len));
            furTuft(g, at, rnd(r, 0.0f, kTau), rnd(r, 24.0f, 32.0f), r, 8, 0.25f, 0.8f);
        }
    }

    void tassel(juce::Graphics& g, P hook, float length, juce::Random& r)
    {
        const C gold(0xffe9c15a), goldDark(0xff8a6418);
        // Cord.
        const P capTop(hook.x, hook.y + length * 0.28f);
        g.setColour(C(0xffff4faa));
        g.drawLine({ hook, capTop }, 2.2f);
        g.setColour(C(0x88ffffff));
        g.drawLine({ hook.translated(-0.6f, 0), capTop.translated(-0.6f, 0) }, 0.7f);
        // Skirt of strands.
        const float skirtTop = capTop.y + 13.0f, bottom = hook.y + length;
        for (int k = 0; k < 70; ++k)
        {
            const float u = rnd(r, -1.0f, 1.0f);
            juce::Path p;
            p.startNewSubPath(hook.x + u * 4.5f, skirtTop);
            p.quadraticTo(hook.x + u * 8.0f, (skirtTop + bottom) * 0.5f, hook.x + u * 11.0f + rnd(r, -1.0f, 1.0f), bottom - rnd(r, 0.0f, 6.0f));
            g.setColour((r.nextInt(5) == 0 ? C(0xffffd0ea) : C(0xffff4faa).withBrightness(rnd(r, 0.75f, 1.0f))).withAlpha(0.95f));
            g.strokePath(p, juce::PathStrokeType(rnd(r, 0.8f, 1.5f)));
        }
        // Gold bead cap with a collar.
        const auto cap = juce::Rectangle<float>(hook.x - 7.0f, capTop.y, 14.0f, 16.0f);
        juce::ColourGradient metal(gold.brighter(0.5f), cap.getX(), cap.getY(), goldDark, cap.getRight(), cap.getBottom(), false);
        metal.addColour(0.4, gold);
        g.setGradientFill(metal);
        g.fillEllipse(cap);
        g.fillRoundedRectangle(hook.x - 6.0f, skirtTop - 3.0f, 12.0f, 4.0f, 1.5f);
        g.setColour(C(0xccffffff));
        g.fillEllipse(cap.getX() + 3.0f, cap.getY() + 2.5f, 4.0f, 3.5f);
        rhinestone(g, { hook.x, cap.getCentreY() + 1.0f }, 2.6f, r);
    }

    void discoBall(juce::Graphics& g, P hook, P centre, float rad, juce::Random& r)
    {
        // Chain.
        for (float y = hook.y; y < centre.y - rad; y += 6.0f)
        {
            g.setColour(C(0xffd8d8e0));
            g.drawEllipse(hook.x - 1.6f, y, 3.2f, 5.0f, 1.0f);
        }
        // Rays of light.
        for (int k = 0; k < 9; ++k)
        {
            const float a = rnd(r, 0.0f, kTau);
            const P end(centre.x + std::cos(a) * rad * 4.5f, centre.y + std::sin(a) * rad * 4.5f);
            juce::ColourGradient ray(C(0x55ffffff), centre.x, centre.y, C(0x00ffffff), end.x, end.y, false);
            g.setGradientFill(ray);
            juce::Path p;
            const P n(-std::sin(a) * 7.0f, std::cos(a) * 7.0f);
            p.startNewSubPath(centre);
            p.lineTo(end + n);
            p.lineTo(end - n);
            p.closeSubPath();
            g.fillPath(p);
        }
        juce::DropShadow(C(0x99300018), 14, { 4, 8 }).drawForPath(g, [&] { juce::Path s; s.addEllipse(centre.x - rad, centre.y - rad, rad * 2, rad * 2); return s; }());
        // Mirror tiles on a sphere: latitude rows of tiles, lit from the top left.
        g.saveState();
        juce::Path clip;
        clip.addEllipse(centre.x - rad, centre.y - rad, rad * 2.0f, rad * 2.0f);
        g.reduceClipRegion(clip);
        g.setColour(C(0xff3a2a38));
        g.fillPath(clip);
        constexpr int rows = 12;
        for (int i = 0; i < rows; ++i)
        {
            const float lat0 = -kTau * 0.25f + (float) i / rows * kTau * 0.5f;
            const float lat1 = lat0 + kTau * 0.5f / rows;
            const int cols = juce::jmax(4, (int) (24.0f * std::cos((lat0 + lat1) * 0.5f)));
            for (int j = 0; j < cols; ++j)
            {
                const float lon0 = -kTau * 0.25f + (float) j / cols * kTau * 0.5f;
                const float lon1 = lon0 + kTau * 0.5f / cols;
                auto proj = [&](float lat, float lon) { return P(centre.x + rad * std::cos(lat) * std::sin(lon), centre.y + rad * std::sin(lat)); };
                juce::Path tile;
                tile.startNewSubPath(proj(lat0, lon0));
                tile.lineTo(proj(lat0, lon1));
                tile.lineTo(proj(lat1, lon1));
                tile.lineTo(proj(lat1, lon0));
                tile.closeSubPath();
                const float lit = juce::jlimit(0.0f, 1.0f, 0.55f - 0.45f * std::sin((lat0 + lat1) * 0.5f + 0.4f) - 0.3f * std::sin((lon0 + lon1) * 0.5f));
                C col = C::fromHSV(0.9f, rnd(r, 0.0f, 0.35f), 0.35f + 0.6f * lit * rnd(r, 0.6f, 1.0f), 1.0f);
                if (r.nextInt(9) == 0)
                    col = C(0xffffffff);
                g.setColour(col);
                g.fillPath(tile);
                g.setColour(C(0x88201018));
                g.strokePath(tile, juce::PathStrokeType(0.6f));
            }
        }
        juce::ColourGradient gloss(C(0x99ffffff), centre.x - rad * 0.45f, centre.y - rad * 0.5f, C(0x00ffffff), centre.x, centre.y, true);
        g.setGradientFill(gloss);
        g.fillEllipse(centre.x - rad, centre.y - rad, rad * 2.0f, rad * 2.0f);
        g.restoreState();
        glint(g, { centre.x - rad * 0.42f, centre.y - rad * 0.45f }, 11.0f, C(0xffffffff));
        glint(g, { centre.x + rad * 0.5f, centre.y + rad * 0.2f }, 7.0f, C(0xffffd1ee));
    }

    // ------------------------------------------------------------- trimmings
    static juce::Path heartPath(P c, float s)
    {
        juce::Path p;
        p.startNewSubPath(c.x, c.y + s * 0.95f);
        p.cubicTo(c.x - s * 1.25f, c.y + s * 0.1f, c.x - s * 0.95f, c.y - s * 0.95f, c.x, c.y - s * 0.35f);
        p.cubicTo(c.x + s * 0.95f, c.y - s * 0.95f, c.x + s * 1.25f, c.y + s * 0.1f, c.x, c.y + s * 0.95f);
        p.closeSubPath();
        return p;
    }

    void heart(juce::Graphics& g, P c, float s, float angle, C col)
    {
        auto p = heartPath(c, s);
        p.applyTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
        juce::DropShadow(C(0x66300018), 5, { 1, 3 }).drawForPath(g, p);
        juce::ColourGradient grad(col.brighter(0.5f), c.x - s, c.y - s, col.darker(0.4f), c.x + s, c.y + s, false);
        g.setGradientFill(grad);
        g.fillPath(p);
        g.setColour(C(0xbbffffff));
        g.fillEllipse(c.x - s * 0.62f, c.y - s * 0.55f, s * 0.42f, s * 0.3f);
    }

    void hearts(juce::Graphics& g, juce::Random& r)
    {
        const P spots[] = { { 372, 52 }, { 1662, 74 }, { 1558, 428 }, { 742, 434 }, { 1905, 742 }, { 520, 760 },
                            { 1250, 952 }, { 82, 952 }, { 1015, 438 }, { 1900, 960 } };
        for (auto c : spots)
            heart(g, c, rnd(r, 7.0f, 12.0f), rnd(r, -0.5f, 0.5f), r.nextBool() ? C(0xffff3a9a) : C(0xffff7cc4));
    }

    void kiss(juce::Graphics& g, P c, float angle)
    {
        // A lipstick kiss: an upper lip with a Cupid's bow, a fuller lower lip, creases.
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
        const C lip(0xddd4145e);
        juce::Path upper;
        upper.startNewSubPath(c.x - 26, c.y);
        upper.cubicTo(c.x - 18, c.y - 12, c.x - 8, c.y - 13, c.x, c.y - 6);
        upper.cubicTo(c.x + 8, c.y - 13, c.x + 18, c.y - 12, c.x + 26, c.y);
        upper.cubicTo(c.x + 12, c.y - 1, c.x - 12, c.y - 1, c.x - 26, c.y);
        juce::Path lower;
        lower.startNewSubPath(c.x - 24, c.y + 2);
        lower.cubicTo(c.x - 12, c.y + 3, c.x + 12, c.y + 3, c.x + 24, c.y + 2);
        lower.cubicTo(c.x + 16, c.y + 17, c.x - 16, c.y + 17, c.x - 24, c.y + 2);
        g.setColour(lip);
        g.fillPath(upper);
        g.fillPath(lower);
        g.setColour(C(0x55ffffff));
        for (int k = -4; k <= 4; ++k)
        {
            g.drawLine((float) (c.x + k * 5), c.y - 6 + std::abs(k) * 0.8f, (float) (c.x + k * 5.6f), c.y - 1.5f, 0.6f);
            g.drawLine((float) (c.x + k * 4.6f), c.y + 4, (float) (c.x + k * 4), c.y + 12 - std::abs(k) * 0.9f, 0.6f);
        }
        g.restoreState();
    }

    void bow(juce::Graphics& g, P c, float s)
    {
        // A satin bow: two loops, two tails and a knot.
        const C satin(0xffff3fa4);
        auto fillSatin = [&](const juce::Path& p, juce::Rectangle<float> box) {
            juce::DropShadow(C(0x77300018), 6, { 2, 4 }).drawForPath(g, p);
            juce::ColourGradient grad(satin.brighter(0.6f), box.getX(), box.getY(), satin.darker(0.5f), box.getRight(), box.getBottom(), false);
            grad.addColour(0.45, satin);
            g.setGradientFill(grad);
            g.fillPath(p);
        };
        juce::Path tails;
        tails.startNewSubPath(c.x - 3 * s, c.y + 3 * s);
        tails.lineTo(c.x - 16 * s, c.y + 34 * s);
        tails.lineTo(c.x - 9 * s, c.y + 31 * s);
        tails.lineTo(c.x - 7 * s, c.y + 38 * s);
        tails.lineTo(c.x + 2 * s, c.y + 4 * s);
        tails.startNewSubPath(c.x + 3 * s, c.y + 3 * s);
        tails.lineTo(c.x + 18 * s, c.y + 32 * s);
        tails.lineTo(c.x + 11 * s, c.y + 30 * s);
        tails.lineTo(c.x + 10 * s, c.y + 37 * s);
        tails.lineTo(c.x - 2 * s, c.y + 4 * s);
        fillSatin(tails, { c.x - 16 * s, c.y, 34 * s, 38 * s });
        for (float side : { -1.0f, 1.0f })
        {
            juce::Path loop;
            loop.startNewSubPath(c);
            loop.cubicTo(c.x + side * 14 * s, c.y - 26 * s, c.x + side * 34 * s, c.y - 14 * s, c.x + side * 30 * s, c.y + 2 * s);
            loop.cubicTo(c.x + side * 28 * s, c.y + 16 * s, c.x + side * 12 * s, c.y + 12 * s, c.x, c.y);
            fillSatin(loop, { c.x - 30 * s, c.y - 22 * s, 60 * s, 34 * s });
            g.setColour(C(0x66ffffff));
            juce::Path fold;
            fold.startNewSubPath(c.x + side * 6 * s, c.y - 4 * s);
            fold.quadraticTo(c.x + side * 18 * s, c.y - 14 * s, c.x + side * 26 * s, c.y - 6 * s);
            g.strokePath(fold, juce::PathStrokeType(1.4f));
        }
        juce::Path knot;
        knot.addRoundedRectangle(c.x - 6 * s, c.y - 7 * s, 12 * s, 14 * s, 4 * s);
        fillSatin(knot, { c.x - 6 * s, c.y - 7 * s, 12 * s, 14 * s });
    }

    void sticker(juce::Graphics& g, P c, float angle, const juce::String& word, C col)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
        const auto r = juce::Rectangle<float>(c.x - 36, c.y - 14, 72, 28);
        juce::DropShadow(C(0x88300018), 6, { 2, 3 }).drawForRectangle(g, r.toNearestInt());
        g.setColour(C(0xffffffff));
        g.fillRoundedRectangle(r, 14.0f);
        juce::ColourGradient grad(col.brighter(0.4f), r.getX(), r.getY(), col.darker(0.3f), r.getRight(), r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r.reduced(2.5f), 11.5f);
        g.setColour(C(0xffffffff));
        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold | juce::Font::italic)).withExtraKerningFactor(0.12f));
        g.drawText(word, r, juce::Justification::centred);
        g.restoreState();
        glint(g, { r.getRight() - 6, r.getY() + 3 }, 6.0f, C(0xffffffff));
    }

    void badge(juce::Graphics& g, juce::Rectangle<float> r, juce::Random& rng)
    {
        // Bedazzled edition badge over the panel's subtitle.
        juce::DropShadow(C(0x99300018), 6, { 1, 3 }).drawForRectangle(g, r.toNearestInt());
        juce::ColourGradient grad(C(0xffffe39a), r.getX(), r.getY(), C(0xffb98a24), r.getX(), r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, 11.0f);
        g.setColour(C(0xffff2f98));
        g.fillRoundedRectangle(r.reduced(2.0f), 9.0f);
        juce::Font script(juce::FontOptions("Snell Roundhand", 19.0f, juce::Font::bold | juce::Font::italic));
        juce::String text = juce::String::fromUTF8("\xe2\x99\xa5 Fabulous Edition \xe2\x99\xa5");
        g.setColour(C(0x88600030));
        g.setFont(script);
        g.drawText(text, r.translated(1.0f, 1.0f), juce::Justification::centred);
        g.setColour(C(0xffffffff));
        g.drawText(text, r, juce::Justification::centred);
        for (float x = r.getX() + 12.0f; x < r.getRight() - 6.0f; x += 32.0f)
        {
            rhinestone(g, { x, r.getY() + 0.5f }, 2.4f, rng);
            rhinestone(g, { x + 16.0f, r.getBottom() - 0.5f }, 2.4f, rng);
        }
    }
};

} // namespace zsynth::ui
