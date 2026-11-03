#pragma once

// SKIP EDITION: a joke overlay that makes ZYNTH look as if it was pulled out
// of a skip after six months outside. It is drawn on top of the whole panel
// and never takes mouse clicks, so every control still works underneath.
// Everything is drawn procedurally (deterministic random), once, into a cache.

#include <juce_gui_basics/juce_gui_basics.h>

namespace zsynth::ui
{

class SkipGrime : public juce::Component
{
public:
    SkipGrime() { setInterceptsMouseClicks(false, false); }

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
    struct V3 { float x, y, z; };

    static float rnd(juce::Random& r, float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); }

    void render(float scale)
    {
        cacheScale = scale;
        cacheSize = getLocalBounds();
        cache = juce::Image(juce::Image::ARGB, juce::jmax(1, juce::roundToInt((float) getWidth() * scale)),
                            juce::jmax(1, juce::roundToInt((float) getHeight() * scale)), true);
        juce::Graphics g(cache);
        g.addTransform(juce::AffineTransform::scale(scale));
        juce::Random r(19790613);
        const auto b = getLocalBounds().toFloat();

        weathering(g, b, r);
        tideMarks(g, r);
        mud(g, b, r);
        rust(g, r);
        scratches(g, b, r);
        dents(g);
        cracks(g);
        brokenCorner(g);
        shatteredLcd(g, { 638.0f, 46.0f });
        coffeeRing(g, { 1452.0f, 905.0f }, 58.0f);
        moss(g, b, r);
        spiderWeb(g, { (float) getWidth(), 0.0f }, 190.0f);
        ductTape(g, { 994.0f, 250.0f }, 300.0f, 1.42f, "DO NOT TOUCH!!");
        ductTape(g, { 640.0f, 650.0f }, 180.0f, -0.35f, {});
        for (auto leaf : std::initializer_list<V3> { V3{1712, 214, 0.6f}, V3{214, 560, -0.9f}, V3{905, 845, 2.2f},
                           V3{520, 1120, 1.1f}, V3{790, 300, -2.4f}, V3{1648, 742, 0.2f},
                           V3{1310, 520, 2.9f} })
            deadLeaf(g, { leaf.x, leaf.y }, leaf.z, r);
        twig(g, { 1180, 400 }, { 1350, 470 });
        twig(g, { 60, 900 }, { 180, 1010 });
        birdPoo(g, { 1252, 162 }, 1.0f, r);
        birdPoo(g, { 428, 868 }, 0.8f, r);
        birdPoo(g, { 1784, 1010 }, 1.2f, r);
        chewingGum(g, { 1712, 52 });
        bottleCap(g, { 1405, 628 });
        cigaretteButt(g, { 962, 1150 }, 0.35f);
        bananaSkin(g, { 1080, 1060 });
        snail(g, { 1868, 560 });
        priceSticker(g, { 1300, 62 });
        dymoLabel(g, { 1430, 1120 }, "ZYNTH  -  SKIP EDITION", -0.03f);
        dust(g, b, r);
    }

    // ------------------------------------------------------------- surfaces
    void weathering(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        // Sun-bleached and grubby: a sepia wash, darker towards the bottom (where the rain sat).
        juce::ColourGradient wash(C(0x26b8a27a), b.getX(), b.getY(), C(0x4a3b2a14), b.getX(), b.getBottom(), false);
        g.setGradientFill(wash);
        g.fillRect(b);
        for (int i = 0; i < 70; ++i)
        {
            const P c(rnd(r, 0, b.getWidth()), rnd(r, 0, b.getHeight()));
            const float rad = rnd(r, 40, 260);
            juce::ColourGradient blot(C(0xff4a3a22).withAlpha(rnd(r, 0.05f, 0.16f)), c.x, c.y, C(0x004a3a22), c.x + rad, c.y, true);
            g.setGradientFill(blot);
            g.fillEllipse(c.x - rad, c.y - rad * 0.7f, rad * 2, rad * 1.4f);
        }
    }

    void tideMarks(juce::Graphics& g, juce::Random& r)
    {
        // Rings left by puddles that dried on the case.
        for (int i = 0; i < 9; ++i)
        {
            const P c(rnd(r, 100, 1820), rnd(r, 150, 1150));
            const float rad = rnd(r, 60, 220);
            juce::Path p;
            for (int k = 0; k <= 48; ++k)
            {
                const float a = (float) k / 48.0f * juce::MathConstants<float>::twoPi;
                const float rr = rad * (1.0f + 0.12f * std::sin(a * 3.0f + (float) i) + 0.05f * std::sin(a * 7.0f));
                const P pt(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr * 0.75f);
                if (k == 0) p.startNewSubPath(pt); else p.lineTo(pt);
            }
            p.closeSubPath();
            g.setColour(C(0xff5a4a30).withAlpha(0.10f));
            g.fillPath(p);
            g.setColour(C(0xff3a2c18).withAlpha(0.35f));
            g.strokePath(p, juce::PathStrokeType(rnd(r, 1.5f, 3.5f)));
        }
    }

    void splat(juce::Graphics& g, P c, float size, C colour, juce::Random& r, int lobes = 9)
    {
        juce::Path p;
        for (int k = 0; k <= lobes * 4; ++k)
        {
            const float a = (float) k / (float) (lobes * 4) * juce::MathConstants<float>::twoPi;
            const float rr = size * (0.65f + 0.35f * std::abs(std::sin(a * (float) lobes * 0.5f + size)) + rnd(r, -0.08f, 0.08f));
            const P pt(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr * 0.8f);
            if (k == 0) p.startNewSubPath(pt); else p.lineTo(pt);
        }
        p.closeSubPath();
        g.setColour(colour);
        g.fillPath(p);
    }

    void mud(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        // Mud thrown up from the bottom of the skip, drips running down.
        for (int i = 0; i < 26; ++i)
        {
            const P c(rnd(r, 0, b.getWidth()), rnd(r, b.getHeight() * 0.55f, b.getHeight()));
            splat(g, c, rnd(r, 12, 70), C(0xff3d2b17).withAlpha(rnd(r, 0.35f, 0.7f)), r);
            for (int d = 0; d < 6; ++d)
                splat(g, c + P(rnd(r, -110, 110), rnd(r, -80, 80)), rnd(r, 2, 9), C(0xff3d2b17).withAlpha(0.6f), r, 5);
        }
        for (int i = 0; i < 14; ++i)
        {
            const float x = rnd(r, 0, b.getWidth()), y0 = rnd(r, 80, 900), len = rnd(r, 60, 260);
            juce::ColourGradient drip(C(0xff4a3520).withAlpha(0.55f), x, y0, C(0x004a3520), x, y0 + len, false);
            g.setGradientFill(drip);
            g.fillRoundedRectangle(x, y0, rnd(r, 3, 8), len, 3.0f);
        }
    }

    void rust(juce::Graphics& g, juce::Random& r)
    {
        // Rust blooms (around the screws and along the bottom) and streaks running down.
        const P spots[] = { { 14, 100 }, { 1906, 100 }, { 14, 960 }, { 1906, 960 }, { 995, 420 }, { 1334, 104 }, { 168, 1190 },
                            { 1910, 1190 }, { 500, 760 }, { 1530, 760 }, { 700, 98 } };
        for (const auto& s : spots)
        {
            juce::ColourGradient bloom(C(0xff6a3014).withAlpha(0.45f), s.x, s.y, C(0x006a3014), s.x + 60.0f, s.y, true);
            g.setGradientFill(bloom);
            g.fillEllipse(s.x - 60, s.y - 48, 120, 96);
            for (int k = 0; k < 420; ++k)
            {
                const float a = rnd(r, 0, juce::MathConstants<float>::twoPi), d = std::pow(r.nextFloat(), 1.3f) * 64.0f;
                const P pt(s.x + std::cos(a) * d, s.y + std::sin(a) * d * 0.8f);
                const C col = r.nextBool() ? C(0xff8a3c16) : (r.nextBool() ? C(0xff5a2810) : C(0xffa85a24));
                g.setColour(col.withAlpha(rnd(r, 0.25f, 0.8f)));
                const float sz = rnd(r, 0.8f, 3.2f);
                g.fillEllipse(pt.x - sz, pt.y - sz, sz * 2, sz * 1.6f);
            }
            const float len = rnd(r, 80, 220);
            juce::ColourGradient streak(C(0xb08a3a14), s.x, s.y, C(0x008a3a14), s.x, s.y + len, false);
            g.setGradientFill(streak);
            g.fillRect(s.x - 6.0f, s.y, rnd(r, 6, 14), len);
        }
    }

    void scratches(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        for (int i = 0; i < 160; ++i)
        {
            const P a(rnd(r, 0, b.getWidth()), rnd(r, 0, b.getHeight()));
            const float ang = rnd(r, -0.6f, 0.6f) + (r.nextBool() ? 0.0f : 1.4f), len = rnd(r, 20, 260);
            const P e(a.x + std::cos(ang) * len, a.y + std::sin(ang) * len);
            g.setColour(juce::Colours::white.withAlpha(rnd(r, 0.08f, 0.28f)));
            g.drawLine({ a, e }, rnd(r, 0.6f, 1.6f));
        }
        // A few deep gouges: dark core, bright lip.
        for (int i = 0; i < 7; ++i)
        {
            const P a(rnd(r, 100, 1800), rnd(r, 120, 1150));
            juce::Path p;
            p.startNewSubPath(a);
            P pt = a;
            const float ang = rnd(r, -0.5f, 0.5f);
            for (int k = 0; k < 6; ++k)
            {
                pt += P(std::cos(ang + rnd(r, -0.2f, 0.2f)) * 45.0f, std::sin(ang + rnd(r, -0.2f, 0.2f)) * 45.0f);
                p.lineTo(pt);
            }
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.strokePath(p, juce::PathStrokeType(3.0f));
            g.setColour(C(0xffd8d0c0).withAlpha(0.45f));
            g.strokePath(p, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(0.0f, 2.2f));
        }
    }

    void dents(juce::Graphics& g)
    {
        // Something heavy landed on it: shaded hollows.
        for (const auto& d : std::initializer_list<V3> { V3{1500, 260, 70}, V3{300, 720, 90}, V3{1100, 890, 60} })
        {
            juce::ColourGradient dark(juce::Colours::black.withAlpha(0.55f), d.x - d.z * 0.4f, d.y - d.z * 0.4f,
                                      juce::Colours::transparentBlack, d.x + d.z * 0.3f, d.y + d.z * 0.3f, false);
            g.setGradientFill(dark);
            g.fillEllipse(d.x - d.z, d.y - d.z * 0.7f, d.z * 2, d.z * 1.4f);
            juce::ColourGradient lit(juce::Colours::transparentWhite, d.x, d.y, juce::Colours::white.withAlpha(0.22f), d.x + d.z * 0.8f,
                                     d.y + d.z * 0.6f, true);
            g.setGradientFill(lit);
            g.fillEllipse(d.x - d.z, d.y - d.z * 0.7f, d.z * 2, d.z * 1.4f);
        }
    }

    // --------------------------------------------------------------- damage
    void crackPath(juce::Graphics& g, P from, float angle, float length, int depth, juce::Random& r)
    {
        juce::Path p;
        p.startNewSubPath(from);
        P pt = from;
        const int steps = juce::jmax(3, (int) (length / 22.0f));
        for (int k = 0; k < steps; ++k)
        {
            angle += rnd(r, -0.45f, 0.45f);
            pt += P(std::cos(angle) * length / (float) steps, std::sin(angle) * length / (float) steps);
            p.lineTo(pt);
            if (depth > 0 && r.nextFloat() < 0.18f)
                crackPath(g, pt, angle + (r.nextBool() ? 0.9f : -0.9f), length * 0.4f, depth - 1, r);
        }
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.strokePath(p, juce::PathStrokeType(depth > 1 ? 2.6f : 1.6f));
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.strokePath(p, juce::PathStrokeType(0.8f), juce::AffineTransform::translation(1.2f, 1.4f));
    }

    void cracks(juce::Graphics& g)
    {
        juce::Random r(4242);
        crackPath(g, { 250, 110 }, 0.55f, 620, 3, r);
        crackPath(g, { 1906, 640 }, 3.4f, 380, 2, r);
        crackPath(g, { 1200, 1250 }, -1.9f, 300, 2, r);
        crackPath(g, { 30, 600 }, -0.2f, 240, 2, r);
    }

    void brokenCorner(juce::Graphics& g)
    {
        // The top-left corner snapped off, showing the circuit board inside.
        juce::Path hole;
        const P pts[] = { { 0, 0 }, { 268, 0 }, { 238, 26 }, { 252, 48 }, { 204, 70 }, { 214, 96 }, { 160, 104 }, { 128, 138 },
                          { 96, 126 }, { 62, 168 }, { 34, 150 }, { 0, 190 } };
        hole.startNewSubPath(pts[0]);
        for (const auto& p : pts)
            hole.lineTo(p);
        hole.closeSubPath();

        g.saveState();
        g.reduceClipRegion(hole);
        g.setColour(C(0xff0c1a10));
        g.fillPath(hole);
        // PCB, a little lower inside the case.
        g.setColour(C(0xff1e5a32));
        g.fillRect(0.0f, 0.0f, 270.0f, 200.0f);
        juce::Random r(77);
        g.setColour(C(0xffb08a3c).withAlpha(0.85f)); // copper traces
        for (int i = 0; i < 22; ++i)
        {
            const float y = rnd(r, 4, 190), x0 = rnd(r, 0, 120);
            juce::Path t;
            t.startNewSubPath(x0, y);
            t.lineTo(x0 + rnd(r, 20, 80), y);
            t.lineTo(x0 + rnd(r, 90, 160), y + rnd(r, -30, 30));
            g.strokePath(t, juce::PathStrokeType(1.6f));
            g.fillEllipse(x0 - 2.5f, y - 2.5f, 5.0f, 5.0f);
        }
        g.setColour(C(0xff141414)); // a chip, a capacitor, a resistor
        g.fillRoundedRectangle(60, 40, 70, 34, 2);
        g.setColour(C(0xff9a9a9a));
        for (int k = 0; k < 7; ++k)
        {
            g.fillRect(64.0f + (float) k * 9.5f, 34.0f, 3.0f, 6.0f);
            g.fillRect(64.0f + (float) k * 9.5f, 74.0f, 3.0f, 6.0f);
        }
        g.setColour(C(0xff2a3f9a));
        g.fillEllipse(150, 30, 34, 34);
        g.setColour(C(0xffd0d0d0).withAlpha(0.5f));
        g.drawEllipse(156, 36, 22, 22, 1.2f);
        g.setColour(C(0xffc8a06a));
        g.fillRoundedRectangle(28, 110, 38, 12, 5);
        g.setColour(C(0xff6a2a1a));
        g.fillRect(36.0f, 110.0f, 4.0f, 12.0f);
        g.fillRect(48.0f, 110.0f, 4.0f, 12.0f);
        // Shadow under the broken edge.
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.strokePath(hole, juce::PathStrokeType(14.0f));
        g.restoreState();
        // The broken edge of the case.
        g.setColour(C(0xff2a2622));
        g.strokePath(hole, juce::PathStrokeType(4.0f));
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.strokePath(hole, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(1.5f, 1.5f));
        // A dangling wire.
        juce::Path wire;
        wire.startNewSubPath(120, 120);
        wire.cubicTo(150, 190, 90, 210, 130, 260);
        g.setColour(C(0xffc0281e));
        g.strokePath(wire, juce::PathStrokeType(3.0f));
        g.setColour(C(0xffd8b060));
        g.fillEllipse(126, 256, 8, 8);
    }

    void shatteredLcd(juce::Graphics& g, P c)
    {
        juce::Random r(9);
        for (int k = 0; k < 13; ++k)
        {
            const float a = (float) k / 13.0f * juce::MathConstants<float>::twoPi + rnd(r, -0.2f, 0.2f);
            juce::Path p;
            p.startNewSubPath(c);
            P pt = c;
            for (int s = 0; s < 5; ++s)
            {
                pt += P(std::cos(a + rnd(r, -0.25f, 0.25f)) * rnd(r, 14, 34), std::sin(a + rnd(r, -0.25f, 0.25f)) * rnd(r, 8, 22));
                p.lineTo(pt);
            }
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.strokePath(p, juce::PathStrokeType(1.0f));
        }
        for (float rad : { 12.0f, 26.0f, 44.0f })
        {
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawEllipse(c.x - rad, c.y - rad * 0.55f, rad * 2, rad * 1.1f, 0.8f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillEllipse(c.x - 4, c.y - 3, 8, 6);
    }

    void coffeeRing(juce::Graphics& g, P c, float rad)
    {
        g.setColour(C(0xff5a3416).withAlpha(0.18f));
        g.fillEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour(C(0xff4a2810).withAlpha(0.6f));
        g.drawEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2, 4.0f);
        g.drawEllipse(c.x - rad + 14, c.y - rad + 9, rad * 2, rad * 2, 2.0f);
    }

    void moss(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        // Moss along the bottom edge and up the sides.
        for (int i = 0; i < 1600; ++i)
        {
            float x, y;
            const int side = r.nextInt(5);
            if (side < 3) { x = rnd(r, 0, b.getWidth()); y = b.getBottom() - std::pow(r.nextFloat(), 2.5f) * 60.0f; }
            else if (side == 3) { x = std::pow(r.nextFloat(), 2.5f) * 40.0f; y = rnd(r, 700, b.getBottom()); }
            else { x = b.getRight() - std::pow(r.nextFloat(), 2.5f) * 40.0f; y = rnd(r, 800, b.getBottom()); }
            const C col = r.nextBool() ? C(0xff4f6b22) : (r.nextBool() ? C(0xff6f8a2a) : C(0xff2f4a18));
            g.setColour(col.withAlpha(rnd(r, 0.5f, 0.95f)));
            const float s = rnd(r, 1.5f, 5.0f);
            g.fillEllipse(x - s, y - s, s * 2, s * 2);
        }
    }

    void spiderWeb(juce::Graphics& g, P corner, float size)
    {
        g.setColour(juce::Colours::white.withAlpha(0.32f));
        const int spokes = 7;
        for (int k = 0; k < spokes; ++k)
        {
            const float a = juce::MathConstants<float>::pi * (0.5f + 0.5f * (float) k / (float) (spokes - 1));
            g.drawLine({ corner, corner + P(std::cos(a) * size, std::sin(a) * size) }, 0.8f);
        }
        for (float d = 24; d < size; d += 20)
        {
            juce::Path p;
            for (int k = 0; k < spokes; ++k)
            {
                const float a = juce::MathConstants<float>::pi * (0.5f + 0.5f * (float) k / (float) (spokes - 1));
                const P pt = corner + P(std::cos(a) * d, std::sin(a) * d);
                if (k == 0) p.startNewSubPath(pt); else p.quadraticTo(corner + P(std::cos(a - 0.12f) * d * 0.9f, std::sin(a - 0.12f) * d * 0.9f), pt);
            }
            g.strokePath(p, juce::PathStrokeType(0.7f));
        }
        // ... and its owner.
        const P s = corner + P(-88, 70);
        g.setColour(juce::Colours::black);
        g.fillEllipse(s.x - 7, s.y - 6, 14, 12);
        g.fillEllipse(s.x - 4, s.y + 4, 8, 8);
        for (int k = 0; k < 4; ++k)
            for (int side : { -1, 1 })
            {
                const float a = (float) k * 0.35f - 0.5f;
                juce::Path leg;
                leg.startNewSubPath(s);
                leg.lineTo(s + P((float) side * 12.0f, -6.0f + a * 14.0f));
                leg.lineTo(s + P((float) side * 18.0f, 4.0f + a * 18.0f));
                g.strokePath(leg, juce::PathStrokeType(1.4f));
            }
    }

    void ductTape(juce::Graphics& g, P c, float length, float angle, const juce::String& text)
    {
        // A "repair": silver gaffer tape, wrinkled, frayed ends, scrawled on.
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
        const juce::Rectangle<float> t(c.x - length * 0.5f, c.y - 26.0f, length, 52.0f);
        juce::Path tape;
        tape.startNewSubPath(t.getX() + 4, t.getY());
        tape.lineTo(t.getRight() - 2, t.getY());
        for (int k = 0; k <= 8; ++k)
            tape.lineTo(t.getRight() + ((k % 2) ? 5.0f : -3.0f), t.getY() + t.getHeight() * (float) k / 8.0f);
        tape.lineTo(t.getX() + 2, t.getBottom());
        for (int k = 8; k >= 0; --k)
            tape.lineTo(t.getX() + ((k % 2) ? -5.0f : 3.0f), t.getY() + t.getHeight() * (float) k / 8.0f);
        tape.closeSubPath();
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillPath(tape, juce::AffineTransform::translation(3, 4));
        juce::ColourGradient grey(C(0xffb8b8b4), t.getX(), t.getY(), C(0xff8a8a86), t.getX(), t.getBottom(), false);
        g.setGradientFill(grey);
        g.fillPath(tape);
        juce::Random r(31);
        for (int k = 0; k < 9; ++k)
        {
            const float x = rnd(r, t.getX() + 10, t.getRight() - 10);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawLine(x, t.getY() + 2, x + rnd(r, -14, 14), t.getBottom() - 2, 1.0f);
            g.setColour(juce::Colours::black.withAlpha(0.15f));
            g.drawLine(x + 2, t.getY() + 2, x + 2 + rnd(r, -14, 14), t.getBottom() - 2, 1.0f);
        }
        if (text.isNotEmpty())
        {
            g.setColour(C(0xff1a1a1a).withAlpha(0.85f));
            g.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)).withHorizontalScale(0.85f));
            g.drawText(text, t.reduced(10, 4), juce::Justification::centred, false);
        }
        g.restoreState();
    }

    // --------------------------------------------------------------- debris
    void deadLeaf(juce::Graphics& g, P c, float angle, juce::Random& r)
    {
        const float len = rnd(r, 46, 80), wid = len * rnd(r, 0.36f, 0.5f);
        juce::Path leaf;
        leaf.startNewSubPath(0, 0);
        leaf.cubicTo(len * 0.25f, -wid, len * 0.75f, -wid * 0.8f, len, 0);
        leaf.cubicTo(len * 0.7f, wid * 0.9f, len * 0.3f, wid, 0, 0);
        // Ragged, nibbled edge.
        for (int k = 0; k < 3; ++k)
        {
            const float x = rnd(r, len * 0.2f, len * 0.8f);
            leaf.addEllipse(x - 5, (r.nextBool() ? -wid * 0.75f : wid * 0.6f) - 5, 10, 10);
        }
        leaf.setUsingNonZeroWinding(false);
        const auto tf = juce::AffineTransform::rotation(angle).translated(c.x, c.y);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillPath(leaf, tf.translated(3, 4));
        const C base = r.nextBool() ? C(0xff8a4a1c) : (r.nextBool() ? C(0xffa8661e) : C(0xff6a3a18));
        juce::ColourGradient grad(base.brighter(0.3f), c.x, c.y, base.darker(0.5f), c.x + len, c.y, false);
        g.setGradientFill(grad);
        g.fillPath(leaf, tf);
        g.setColour(base.darker(0.9f));
        juce::Path veins;
        veins.startNewSubPath(-10, 0);
        veins.lineTo(len, 0);
        for (int k = 1; k < 5; ++k)
        {
            const float x = len * (float) k / 5.0f;
            veins.startNewSubPath(x, 0);
            veins.lineTo(x + len * 0.12f, -wid * 0.55f);
            veins.startNewSubPath(x, 0);
            veins.lineTo(x + len * 0.12f, wid * 0.5f);
        }
        g.strokePath(veins, juce::PathStrokeType(1.1f), tf);
    }

    void twig(juce::Graphics& g, P a, P b)
    {
        juce::Path p;
        p.startNewSubPath(a);
        p.quadraticTo((a + b) * 0.5f + P(10, -18), b);
        const P mid = a + (b - a) * 0.55f;
        p.startNewSubPath(mid);
        p.lineTo(mid + P(26, -30));
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.strokePath(p, juce::PathStrokeType(6.0f), juce::AffineTransform::translation(3, 4));
        g.setColour(C(0xff4a3422));
        g.strokePath(p, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(C(0xff7a5a3a));
        g.strokePath(p, juce::PathStrokeType(1.5f), juce::AffineTransform::translation(-1, -1));
    }

    void birdPoo(juce::Graphics& g, P c, float size, juce::Random& r)
    {
        splat(g, c, 26.0f * size, C(0xfff2f0e6).withAlpha(0.92f), r, 7);
        for (int d = 0; d < 5; ++d)
            splat(g, c + P(rnd(r, -40, 40) * size, rnd(r, -30, 30) * size), rnd(r, 3, 7) * size, C(0xfff2f0e6).withAlpha(0.9f), r, 5);
        splat(g, c + P(3, 2), 10.0f * size, C(0xff8a8270).withAlpha(0.8f), r, 5);
        g.setColour(C(0xff2a2418).withAlpha(0.8f));
        g.fillEllipse(c.x - 3 * size, c.y - 2 * size, 6 * size, 4 * size);
        // A drip running down.
        g.setColour(C(0xfff2f0e6).withAlpha(0.85f));
        g.fillRoundedRectangle(c.x + 6 * size, c.y, 6 * size, 34 * size, 3 * size);
        g.fillEllipse(c.x + 4.5f * size, c.y + 30 * size, 9 * size, 9 * size);
    }

    void chewingGum(juce::Graphics& g, P c)
    {
        juce::Random r(5);
        splat(g, c, 15, C(0xffe87aa8), r, 6);
        g.setColour(C(0xffffb0d0).withAlpha(0.7f));
        g.fillEllipse(c.x - 7, c.y - 8, 7, 5);
        g.setColour(C(0xffb04a78));
        g.drawLine(c.x + 4, c.y + 6, c.x + 18, c.y + 30, 2.0f); // a stringy bit
    }

    void bottleCap(juce::Graphics& g, P c)
    {
        juce::Path cap;
        for (int k = 0; k <= 42; ++k)
        {
            const float a = (float) k / 42.0f * juce::MathConstants<float>::twoPi;
            const float rr = (k % 2) ? 20.0f : 17.5f;
            const P pt(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr * 0.8f);
            if (k == 0) cap.startNewSubPath(pt); else cap.lineTo(pt);
        }
        cap.closeSubPath();
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillPath(cap, juce::AffineTransform::translation(3, 3));
        g.setColour(C(0xffc02020));
        g.fillPath(cap);
        g.setColour(C(0xffe0d8c8));
        g.drawEllipse(c.x - 12, c.y - 9.5f, 24, 19, 2.0f);
        g.setColour(C(0xff8a5a2a).withAlpha(0.7f)); // rust on the crimp
        g.fillEllipse(c.x + 8, c.y + 2, 9, 6);
    }

    void cigaretteButt(juce::Graphics& g, P c, float angle)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(c.x - 30 + 3, c.y - 6 + 4, 60, 12, 4);
        g.setColour(C(0xffd8a050)); // filter
        g.fillRoundedRectangle(c.x - 30, c.y - 6, 26, 12, 4);
        g.setColour(C(0xffefe8dc)); // paper, squashed
        g.fillRect(c.x - 6.0f, c.y - 5.5f, 30.0f, 11.0f);
        g.setColour(C(0xff2a2a2a)); // burnt end
        g.fillEllipse(c.x + 20, c.y - 6, 10, 12);
        g.setColour(C(0xff9a9488).withAlpha(0.6f)); // ash
        g.fillEllipse(c.x + 28, c.y - 4, 14, 9);
        g.restoreState();
    }

    void bananaSkin(juce::Graphics& g, P c)
    {
        // An old banana skin lying across the keys: four flaps around the stalk end.
        struct Flap { float angle, length, width, curl; };
        const Flap flaps[] = { { 0.05f, 230, 40, 0.30f }, { 0.55f, 210, 38, -0.25f }, { 1.05f, 190, 36, 0.35f }, { -0.45f, 175, 34, -0.35f } };
        auto flapPath = [](const Flap& f) {
            juce::Path p;
            p.startNewSubPath(0, -f.width * 0.35f);
            p.cubicTo(f.length * 0.3f, -f.width * (1.0f + f.curl), f.length * 0.75f, -f.width * 0.6f, f.length, f.curl * 30.0f);
            p.cubicTo(f.length * 0.78f, f.width * 0.4f, f.length * 0.3f, f.width * (0.9f - f.curl), 0, f.width * 0.35f);
            p.closeSubPath();
            return p;
        };
        juce::Random r(2024);
        for (const auto& f : flaps)
        {
            const auto tf = juce::AffineTransform::rotation(f.angle).translated(c.x, c.y);
            const auto p = flapPath(f);
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillPath(p, tf.translated(5, 7));
            // Outside: yellow going brown; the inner side shows pale where it curls.
            juce::ColourGradient grad(C(0xffd8b832), c.x, c.y, C(0xff4a3010), c.x + std::cos(f.angle) * f.length, c.y + std::sin(f.angle) * f.length, false);
            grad.addColour(0.45, C(0xffc09a26));
            grad.addColour(0.8, C(0xff7a5418));
            g.setGradientFill(grad);
            g.fillPath(p, tf);
            juce::Path inner;
            inner.addEllipse(f.length * 0.35f, -f.width * 0.18f, f.length * 0.5f, f.width * 0.36f);
            g.setColour(C(0xfff0e6c0).withAlpha(0.55f));
            g.fillPath(inner, tf);
            // Ridges and brown speckles.
            g.setColour(C(0xff8a6a1a).withAlpha(0.7f));
            juce::Path ridge;
            ridge.startNewSubPath(6, 0);
            ridge.quadraticTo(f.length * 0.5f, -f.width * 0.25f * (1.0f + f.curl), f.length * 0.95f, f.curl * 26.0f);
            g.strokePath(ridge, juce::PathStrokeType(1.4f), tf);
            for (int k = 0; k < 16; ++k)
            {
                const P sp = P(rnd(r, 10, f.length * 0.95f), rnd(r, -f.width * 0.45f, f.width * 0.45f)).transformedBy(tf);
                g.setColour(C(0xff3a2408).withAlpha(rnd(r, 0.5f, 0.9f)));
                const float s = rnd(r, 2, 7);
                g.fillEllipse(sp.x - s, sp.y - s * 0.6f, s * 2, s * 1.2f);
            }
            g.setColour(C(0xff4a3010));
            g.strokePath(p, juce::PathStrokeType(1.3f), tf);
        }
        // The stalk.
        juce::Path stalk;
        stalk.addRoundedRectangle(-46, -8, 50, 16, 5);
        const auto tf = juce::AffineTransform::rotation(0.3f).translated(c.x, c.y);
        g.setColour(C(0xff3a2a14));
        g.fillPath(stalk, tf);
        const P tip = P(-46, 0).transformedBy(tf);
        g.setColour(C(0xff1e140a));
        g.fillEllipse(tip.x - 8, tip.y - 7, 16, 14);
        // A fruit fly.
        const P fly(c.x + 140, c.y - 100);
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillEllipse(fly.x - 9, fly.y - 9, 9, 6);
        g.fillEllipse(fly.x, fly.y - 9, 9, 6);
        g.setColour(C(0xff2a1a0a));
        g.fillEllipse(fly.x - 3.5f, fly.y - 3, 7, 7);
    }

    void snail(juce::Graphics& g, P c)
    {
        // Climbing up the side, leaving a silvery trail.
        juce::Path trail;
        trail.startNewSubPath(c.x + 4, c.y + 26);
        trail.cubicTo(c.x + 14, c.y + 120, c.x - 10, c.y + 200, c.x + 6, c.y + 320);
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.strokePath(trail, juce::PathStrokeType(9.0f));
        g.setColour(C(0xffa89a78));
        g.fillRoundedRectangle(c.x - 9, c.y - 30, 18, 66, 9);
        g.drawLine(c.x - 4, c.y - 30, c.x - 9, c.y - 46, 2.0f);
        g.drawLine(c.x + 4, c.y - 30, c.x + 8, c.y - 46, 2.0f);
        g.setColour(C(0xff2a2010));
        g.fillEllipse(c.x - 12, c.y - 50, 6, 6);
        g.fillEllipse(c.x + 6, c.y - 50, 6, 6);
        juce::ColourGradient shell(C(0xffc08a4a), c.x - 26, c.y - 10, C(0xff5a3a1a), c.x + 6, c.y + 30, false);
        g.setGradientFill(shell);
        g.fillEllipse(c.x - 30, c.y - 12, 40, 40);
        juce::Path spiral;
        for (int k = 0; k <= 60; ++k)
        {
            const float a = (float) k / 60.0f * 4.0f * juce::MathConstants<float>::pi;
            const float rr = 2.0f + (float) k / 60.0f * 17.0f;
            const P pt(c.x - 10 + std::cos(a) * rr, c.y + 8 + std::sin(a) * rr);
            if (k == 0) spiral.startNewSubPath(pt); else spiral.lineTo(pt);
        }
        g.setColour(C(0xff3a2410));
        g.strokePath(spiral, juce::PathStrokeType(1.8f));
    }

    void priceSticker(juce::Graphics& g, P c)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(0.25f, c.x, c.y));
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillEllipse(c.x - 38 + 2, c.y - 38 + 3, 76, 76);
        g.setColour(C(0xfff07818));
        g.fillEllipse(c.x - 38, c.y - 38, 76, 76);
        g.setColour(C(0xff1a1a1a));
        g.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
        g.drawText(juce::String::fromUTF8("\xc2\xa3") + "2", juce::Rectangle<float>(c.x - 38, c.y - 30, 76, 34), juce::Justification::centred, false);
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText("SOLD AS SEEN", juce::Rectangle<float>(c.x - 38, c.y + 4, 76, 14), juce::Justification::centred, false);
        g.drawText("NO REFUNDS", juce::Rectangle<float>(c.x - 38, c.y + 17, 76, 12), juce::Justification::centred, false);
        // Half peeled off.
        juce::Path curl;
        curl.startNewSubPath(c.x + 18, c.y - 32);
        curl.quadraticTo(c.x + 44, c.y - 30, c.x + 34, c.y - 4);
        curl.quadraticTo(c.x + 30, c.y - 20, c.x + 18, c.y - 32);
        g.setColour(C(0xfff4e8d8));
        g.fillPath(curl);
        g.restoreState();
    }

    void dymoLabel(juce::Graphics& g, P c, const juce::String& text, float angle)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
        const auto font = juce::Font(juce::FontOptions(26.0f, juce::Font::bold));
        const float w = juce::GlyphArrangement::getStringWidth(font, text) + 40.0f;
        const juce::Rectangle<float> t(c.x, c.y - 19, w, 38);
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillRoundedRectangle(t.translated(2, 3), 3);
        g.setColour(C(0xff10100e));
        g.fillRoundedRectangle(t, 3);
        g.setColour(C(0xffe8e4da));
        g.setFont(font);
        g.drawText(text, t, juce::Justification::centred, false);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawText(text, t.translated(0, 1.5f), juce::Justification::centred, false);
        g.restoreState();
    }

    void dust(juce::Graphics& g, juce::Rectangle<float> b, juce::Random& r)
    {
        for (int i = 0; i < 6000; ++i)
        {
            const P pt(rnd(r, 0, b.getWidth()), rnd(r, 0, b.getHeight()));
            g.setColour((r.nextBool() ? C(0xffd8ccb0) : C(0xff2a2014)).withAlpha(rnd(r, 0.15f, 0.55f)));
            const float s = rnd(r, 0.5f, 1.8f);
            g.fillEllipse(pt.x, pt.y, s, s);
        }
    }
};

} // namespace zsynth::ui
