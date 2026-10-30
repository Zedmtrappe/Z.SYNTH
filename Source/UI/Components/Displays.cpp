#include "Displays.h"

#include "../../DSP/FM/FmAlgorithms.h"
#include "../../DSP/Wavetable/WavetableBank.h"

#include <complex>

namespace zsynth::ui
{

namespace
{
    void fillPanelInset(juce::Graphics& g, juce::Rectangle<float> r)
    {
        drawLcd(g, r);
    }

    void drawLcdText(juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size, juce::Colour c = Colours::lcdText,
                     juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour(c);
        g.setFont(lcdFont(size));
        g.drawFittedText(t, r, j, 1, 0.7f);
    }
}

// =============================================================== envelope ====
uint64_t EnvelopeDisplay::stateHash() const
{
    const int b = base;
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < 4; ++i)
        h = mix(h, v(b + i));
    return h;
}

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    r = r.reduced(4.0f, 5.0f);

    const int b = base;
    const float a = v(b), d = v(b + 1), s = v(b + 2), rel = v(b + 3);
    auto width = [](float t) { return std::log2(1.0f + t * 40.0f) + 0.15f; };
    const float wa = width(a), wd = width(d), wr = width(rel), ws = 1.6f;
    const float scale = r.getWidth() / (wa + wd + ws + wr);

    juce::Path p;
    const float x0 = r.getX(), y0 = r.getBottom(), h = r.getHeight();
    p.startNewSubPath(x0, y0);
    constexpr int steps = 24;
    constexpr float T = 1.3f;
    for (int i = 1; i <= steps; ++i)
    {
        const float u = (float) i / steps;
        const float y = T * (1.0f - std::pow(1.0f - 1.0f / T, u));
        p.lineTo(x0 + u * wa * scale, y0 - y * h);
    }
    const float xd = x0 + wa * scale;
    for (int i = 1; i <= steps; ++i)
    {
        const float u = (float) i / steps;
        const float y = s + (1.0f - s) * std::exp(-6.9f * u);
        p.lineTo(xd + u * wd * scale, y0 - y * h);
    }
    const float xs = xd + wd * scale;
    p.lineTo(xs + ws * scale, y0 - s * h);
    const float xr = xs + ws * scale;
    for (int i = 1; i <= steps; ++i)
    {
        const float u = (float) i / steps;
        p.lineTo(xr + u * wr * scale, y0 - s * std::exp(-6.9f * u) * h);
    }

    juce::Path fill(p);
    fill.lineTo(r.getRight(), y0);
    fill.closeSubPath();
    g.setColour(Colours::lcdText.withAlpha(0.14f));
    g.fillPath(fill);
    g.setColour(Colours::lcdText);
    g.strokePath(p, juce::PathStrokeType(1.6f));
}

// ============================================================== wavetable ====
uint64_t WavetableDisplay::stateHash() const
{
    uint64_t h = 1469598103934665603ull;
    h = mix(h, v(P::WtTable));
    h = mix(h, v(P::WtPos));
    return h;
}

void WavetableDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    const auto& table = dsp::WavetableBank::get().table(iv(P::WtTable));
    const float pos = v(P::WtPos);
    const auto area = r.reduced(6.0f, 8.0f);

    auto drawFrame = [&](float framePos, float dx, float dy, juce::Colour c, float thickness) {
        const float fp = juce::jlimit(0.0f, 1.0f, framePos) * (float) (dsp::Wavetable::kFrames - 1);
        const int f0 = juce::jmin((int) fp, dsp::Wavetable::kFrames - 2);
        const float m = fp - (float) f0;
        const float* a = table.frame(f0, 3);
        const float* b = table.frame(f0 + 1, 3);
        juce::Path p;
        constexpr int points = 64;
        for (int i = 0; i <= points; ++i)
        {
            const int idx = juce::jmin(dsp::Wavetable::kSize - 1, i * dsp::Wavetable::kSize / points);
            const float y = a[idx] + (b[idx] - a[idx]) * m;
            const float x = area.getX() + dx + (float) i / points * (area.getWidth() - 12.0f);
            const float yy = area.getCentreY() - dy - y * (area.getHeight() * 0.38f);
            if (i == 0) p.startNewSubPath(x, yy); else p.lineTo(x, yy);
        }
        g.setColour(c);
        g.strokePath(p, juce::PathStrokeType(thickness));
    };

    for (int k = 5; k >= 1; --k)
        drawFrame((float) k / 5.0f, (float) k * 2.0f, (float) k * 1.6f, Colours::lcdDim.withAlpha(0.6f), 1.0f);
    drawFrame(pos, 0.0f, 0.0f, Colours::lcdText, 1.8f);
}

// =================================================================== FM ====
uint64_t FmAlgorithmDisplay::stateHash() const
{
    uint64_t h = 1469598103934665603ull;
    h = mix(h, v(P::FmAlgo));
    h = mix(h, v(P::FmFeedback));
    for (int op = 0; op < 6; ++op)
        h = mix(h, v(P::Op1Level + op * kOpParamStride));
    return h;
}

void FmAlgorithmDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    const auto& alg = dsp::fmAlgorithm(iv(P::FmAlgo));

    // Level 0 = carriers; a modulator sits one level above the highest op it drives.
    int level[6] {};
    for (int op = 0; op < 6; ++op)
        level[op] = -1;
    for (int pass = 0; pass < 8; ++pass)
        for (int op = 0; op < 6; ++op)
        {
            int l = ((alg.carriers >> op) & 1u) ? 0 : -1;
            for (int t = 0; t < 6; ++t)
                if (((alg.modulators[t] >> op) & 1u) && level[t] >= 0)
                    l = juce::jmax(l, level[t] + 1);
            level[op] = l;
        }

    int maxLevel = 0;
    for (int op = 0; op < 6; ++op)
        maxLevel = juce::jmax(maxLevel, level[op]);

    juce::Point<float> centre[6];
    const auto area = r.reduced(8.0f, 10.0f).withTrimmedBottom(12.0f);
    const float rowH = area.getHeight() / (float) (maxLevel + 1);
    for (int l = 0; l <= maxLevel; ++l)
    {
        int count = 0;
        for (int op = 0; op < 6; ++op)
            count += level[op] == l ? 1 : 0;
        int k = 0;
        for (int op = 0; op < 6; ++op)
            if (level[op] == l)
            {
                const float x = area.getX() + area.getWidth() * ((float) k + 0.5f) / (float) count;
                const float y = area.getBottom() - rowH * ((float) l + 0.5f);
                centre[op] = { x, y };
                ++k;
            }
    }

    const float box = juce::jmin(22.0f, rowH * 0.62f);
    g.setColour(Colours::lcdDim.brighter(0.5f));
    for (int t = 0; t < 6; ++t)
        for (int m = 0; m < 6; ++m)
            if ((alg.modulators[t] >> m) & 1u)
                g.drawLine(centre[m].x, centre[m].y + box * 0.5f, centre[t].x, centre[t].y - box * 0.5f, 1.4f);

    const float busY = r.getBottom() - 8.0f;
    g.setColour(Colours::lcdText.withAlpha(0.7f));
    float minX = 1e9f, maxX = -1e9f;
    for (int op = 0; op < 6; ++op)
        if ((alg.carriers >> op) & 1u)
        {
            g.drawLine(centre[op].x, centre[op].y + box * 0.5f, centre[op].x, busY, 1.4f);
            minX = juce::jmin(minX, centre[op].x);
            maxX = juce::jmax(maxX, centre[op].x);
        }
    g.drawLine(minX, busY, maxX, busY, 1.6f);

    if (v(P::FmFeedback) > 0.0f)
    {
        const auto c = centre[5];
        g.setColour(Colours::lcdText.withAlpha(0.4f + 0.6f * v(P::FmFeedback)));
        g.drawRoundedRectangle(c.x + box * 0.2f, c.y - box * 0.8f, box * 0.8f, box * 1.0f, 3.0f, 1.2f);
    }

    for (int op = 0; op < 6; ++op)
    {
        const auto b = juce::Rectangle<float>(box, box).withCentre(centre[op]);
        const bool carrier = (alg.carriers >> op) & 1u;
        const float lvl = v(P::Op1Level + op * kOpParamStride);
        g.setColour(carrier ? Colours::lcdText.withAlpha(0.25f + 0.6f * lvl) : Colours::lcdDim.withAlpha(0.4f + 0.6f * lvl));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(Colours::lcdText);
        g.drawRoundedRectangle(b, 3.0f, 1.0f);
        g.setColour(carrier ? Colours::lcdBack : Colours::lcdText);
        g.setFont(lcdFont(12.0f));
        g.drawText(juce::String(op + 1), b.toNearestInt(), juce::Justification::centred, false);
    }
}

// ============================================================ filter curve ====
uint64_t FilterCurveDisplay::stateHash() const
{
    uint64_t h = 1469598103934665603ull;
    for (int p : { P::FltModel, P::FltType, P::FltSlope, P::FltCutoff, P::FltRes, P::FltHpf })
        h = mix(h, v(p));
    return h;
}

void FilterCurveDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    const auto area = r.reduced(4.0f, 4.0f);

    const auto model = (FilterModel) iv(P::FltModel);
    const auto type = (FilterType) iv(P::FltType);
    const bool four = iv(P::FltSlope) == 1 && model != FilterModel::K35;
    const float fc = v(P::FltCutoff), res = v(P::FltRes), hpf = v(P::FltHpf);
    using C = std::complex<float>;

    auto response = [&](float f) -> float {
        const C s(0.0f, f / fc);
        C y;
        if (model == FilterModel::Ladder || model == FilterModel::Ota)
        {
            const float k = res * (model == FilterModel::Ladder ? 4.1f : 4.25f);
            const C H = 1.0f / (1.0f + s);
            const C H2 = H * H, H4 = H2 * H2;
            const C x = 1.0f / (1.0f + k * H4);
            const C y1 = x * H, y2 = x * H2, y3 = x * H2 * H, y4 = x * H4;
            switch (type)
            {
                case FilterType::LowPass:  y = four ? y4 : y2; break;
                case FilterType::BandPass: y = four ? 4.0f * (y2 - 2.0f * y3 + y4) : 2.0f * (y1 - y2); break;
                case FilterType::HighPass: y = four ? x - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4 : x - 2.0f * y1 + y2; break;
                default:                   y = four ? x - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + 2.0f * y4 : x - 2.0f * y1 + 2.0f * y2; break;
            }
            y *= model == FilterModel::Ladder ? (1.0f + 0.35f * k) : (1.0f + 0.5f * k);
        }
        else
        {
            const float damping = model == FilterModel::Sem ? 2.0f - 1.96f * res : juce::jmax(0.05f, 2.0f - (0.01f + 2.09f * res));
            auto svf = [&](float k) {
                const C D = s * s + k * s + 1.0f;
                switch (type)
                {
                    case FilterType::LowPass:  return 1.0f / D;
                    case FilterType::BandPass: return k * s / D;
                    case FilterType::HighPass: return model == FilterModel::K35 ? s * (s + k) / D : s * s / D; // K35 HP: resonant 6 dB/oct
                    default:                   return (s * s + 1.0f) / D;
                }
            };
            y = svf(four ? 0.7654f * (1.0f - 0.97f * res) : damping);
            if (four)
                y *= svf(1.8478f);
        }
        if (hpf > 12.0f)
        {
            const C sh(0.0f, f / hpf);
            y *= sh * sh / (sh * sh + 1.41421f * sh + 1.0f);
        }
        return std::abs(y);
    };

    auto xFor = [&](float f) { return area.getX() + area.getWidth() * std::log(f / 20.0f) / std::log(1000.0f); };
    g.setColour(Colours::lcdDim.withAlpha(0.5f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine((int) xFor(f), area.getY(), area.getBottom());
    const float zeroDbY = area.getY() + area.getHeight() * (24.0f / 72.0f);
    g.drawHorizontalLine((int) zeroDbY, area.getX(), area.getRight());

    juce::Path p;
    constexpr int points = 140;
    for (int i = 0; i <= points; ++i)
    {
        const float f = 20.0f * std::pow(1000.0f, (float) i / points);
        const float db = juce::jlimit(-48.0f, 24.0f, 20.0f * std::log10(response(f) + 1.0e-6f));
        const float x = area.getX() + area.getWidth() * (float) i / points;
        const float y = area.getY() + area.getHeight() * (24.0f - db) / 72.0f;
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    juce::Path fill(p);
    fill.lineTo(area.getRight(), area.getBottom());
    fill.lineTo(area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setColour(Colours::lcdText.withAlpha(0.12f));
    g.fillPath(fill);
    g.setColour(Colours::lcdText);
    g.strokePath(p, juce::PathStrokeType(1.6f));

    g.setFont(lcdFont(10.0f));
    g.setColour(Colours::lcdDim.brighter(0.6f));
    g.drawText("100", (int) xFor(100.0f) + 2, (int) area.getBottom() - 12, 30, 12, juce::Justification::left);
    g.drawText("1k", (int) xFor(1000.0f) + 2, (int) area.getBottom() - 12, 30, 12, juce::Justification::left);
    g.drawText("10k", (int) xFor(10000.0f) + 2, (int) area.getBottom() - 12, 30, 12, juce::Justification::left);
}

uint64_t FilterModelBadge::stateHash() const
{
    uint64_t h = 1469598103934665603ull;
    h = mix(h, v(P::FltModel));
    h = mix(h, v(P::FltType));
    h = mix(h, v(P::FltSlope));
    return h;
}

void FilterModelBadge::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    static const char* names[] = { "LADDER", "OTA", "SEM", "K35" };
    static const char* topo[] = { "TRANSISTOR", "CASCADE", "STATE VAR", "SALLEN-KEY" };
    const auto model = (FilterModel) iv(P::FltModel);
    const bool four = iv(P::FltSlope) == 1 && model != FilterModel::K35;
    const auto type = (FilterType) iv(P::FltType);
    juce::String slope = model == FilterModel::K35 ? (type == FilterType::HighPass ? "6 dB" : "12 dB") : (four ? "24 dB" : "12 dB");
    auto inner = r.reduced(4.0f, 4.0f).toNearestInt();
    drawLcdText(g, names[(int) model], inner.removeFromTop(18), 14.0f, Colours::lcdText, juce::Justification::centred);
    drawLcdText(g, topo[(int) model], inner.removeFromTop(16), 9.5f, Colours::lcdDim.brighter(0.7f), juce::Justification::centred);
    drawLcdText(g, slope, inner, 11.0f, Colours::lcdText.withAlpha(0.8f), juce::Justification::centred);
}

// ==================================================================== LFO ====
uint64_t LfoShapeDisplay::stateHash() const
{
    const int b = lfo * kLfoParamStride;
    uint64_t h = 1469598103934665603ull;
    h = mix(h, v(P::Lfo1Wave + b));
    h = mix(h, v(P::Lfo1Phase + b));
    h = mix(h, v(P::Lfo1Unipolar + b));
    return h;
}

void LfoShapeDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    const int b = lfo * kLfoParamStride;
    const auto wave = (LfoWave) iv(P::Lfo1Wave + b);
    const float phase = v(P::Lfo1Phase + b) / 360.0f;
    const bool uni = v(P::Lfo1Unipolar + b) >= 0.5f;
    const auto area = r.reduced(4.0f, 8.0f);

    static const float randomSteps[8] = { 0.3f, -0.7f, 0.9f, -0.2f, 0.5f, -0.9f, 0.1f, 0.7f };
    auto value = [&](float p) {
        p = p + phase;
        p -= std::floor(p);
        switch (wave)
        {
            case LfoWave::Sine:      return std::sin(p * juce::MathConstants<float>::twoPi);
            case LfoWave::Triangle:  return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case LfoWave::SawUp:     return 2.0f * p - 1.0f;
            case LfoWave::RampDown:  return 1.0f - 2.0f * p;
            case LfoWave::Square:    return p < 0.5f ? 1.0f : -1.0f;
            case LfoWave::SampleHold: return randomSteps[(int) (p * 8.0f) & 7];
            default:
            {
                const float x = p * 8.0f;
                const int i = (int) x;
                const float f = x - (float) i;
                const float s = f * f * (3.0f - 2.0f * f);
                return randomSteps[i & 7] + (randomSteps[(i + 1) & 7] - randomSteps[i & 7]) * s;
            }
        }
    };

    // Vertical display: time runs downward, value horizontally (fits the tall slot).
    juce::Path p;
    constexpr int points = 80;
    for (int i = 0; i <= points; ++i)
    {
        const float t = (float) i / points;
        float val = value(t);
        if (uni)
            val = val * 0.5f + 0.5f;
        const float x = uni ? area.getX() + val * area.getWidth() : area.getCentreX() + val * area.getWidth() * 0.5f;
        const float y = area.getY() + t * area.getHeight();
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    g.setColour(Colours::lcdDim);
    g.drawVerticalLine((int) (uni ? area.getX() : area.getCentreX()), area.getY(), area.getBottom());
    g.setColour(Colours::lcdText);
    g.strokePath(p, juce::PathStrokeType(1.4f));
}

// ================================================================= stereo ====
uint64_t StereoDisplay::stateHash() const
{
    uint64_t h = 1469598103934665603ull;
    h = mix(h, v(P::Width));
    h = mix(h, v(P::Character));
    h = mix(h, v(P::UnisonSpread));
    return h;
}

void StereoDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    const float width = v(P::Width), character = v(P::Character);
    const auto c = juce::Point<float>(r.getCentreX(), r.getBottom() - 6.0f);
    const float radius = r.getHeight() - 14.0f;

    g.setColour(Colours::lcdDim);
    juce::Path half;
    half.addCentredArc(c.x, c.y, radius, radius, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    g.strokePath(half, juce::PathStrokeType(1.0f));
    g.drawLine(c.x, c.y, c.x, c.y - radius, 1.0f);

    const float spread = juce::jlimit(0.02f, 1.0f, width * 0.5f) * juce::MathConstants<float>::halfPi;
    juce::Path wedge;
    wedge.startNewSubPath(c);
    wedge.addCentredArc(c.x, c.y, radius * 0.92f, radius * 0.92f, 0.0f, -spread, spread, false);
    wedge.closeSubPath();
    g.setColour(Colours::lcdText.withAlpha(0.25f + 0.3f * character));
    g.fillPath(wedge);
    g.setColour(Colours::lcdText);
    g.strokePath(wedge, juce::PathStrokeType(1.2f));
    g.setFont(lcdFont(10.0f));
    g.drawText("L", r.toNearestInt().reduced(6, 4), juce::Justification::bottomLeft, false);
    g.drawText("R", r.toNearestInt().reduced(6, 4), juce::Justification::bottomRight, false);
}

// ================================================================== meter ====
void OutputMeter::refresh()
{
    for (int c = 0; c < 2; ++c)
    {
        const float p = processor.getEngine().getPeak(c);
        level[c] = juce::jmax(p, level[c] * 0.85f);
        if (p >= hold[c]) { hold[c] = p; holdTimer[c] = 45; }
        else if (--holdTimer[c] <= 0) hold[c] *= 0.92f;
    }
    repaint();
}

void OutputMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    r = r.reduced(6.0f, 8.0f);
    auto scaleArea = r.removeFromLeft(22.0f);
    const float barW = (r.getWidth() - 6.0f) / 2.0f;
    auto yFor = [&](float db) { return r.getY() + r.getHeight() * (juce::jlimit(-60.0f, 0.0f, db) / -60.0f); };

    g.setFont(lcdFont(9.0f));
    for (int db : { 0, -6, -12, -24, -36, -48 })
    {
        g.setColour(Colours::lcdDim.brighter(0.5f));
        g.drawText(juce::String(db), scaleArea.withY(yFor((float) db) - 5.0f).withHeight(10.0f).toNearestInt(), juce::Justification::centredRight, false);
        g.setColour(Colours::lcdDim.withAlpha(0.6f));
        g.drawHorizontalLine((int) yFor((float) db), r.getX(), r.getRight());
    }
    for (int c = 0; c < 2; ++c)
    {
        const auto bar = juce::Rectangle<float>(r.getX() + 3.0f + (float) c * (barW + 3.0f), r.getY(), barW - 2.0f, r.getHeight());
        g.setColour(juce::Colours::black);
        g.fillRect(bar);
        const float db = 20.0f * std::log10(level[c] + 1.0e-6f);
        const float top = yFor(db);
        juce::ColourGradient grad(Colours::red, bar.getX(), yFor(0.0f), Colours::green.darker(0.3f), bar.getX(), bar.getBottom(), false);
        grad.addColour(0.12, Colours::accent);
        g.setGradientFill(grad);
        g.fillRect(bar.withTop(top));
        const float hdb = 20.0f * std::log10(hold[c] + 1.0e-6f);
        g.setColour(hold[c] >= 0.98f ? Colours::red : Colours::lcdText);
        g.fillRect(bar.getX(), yFor(hdb) - 1.0f, bar.getWidth(), 2.0f);
    }
}

void OutputInfo::refresh()
{
    const auto& e = processor.getEngine();
    cpu = processor.getCpuLoad();
    const float peak = juce::jmax(e.getPeak(0), e.getPeak(1));
    const float gr = 20.0f * std::log10(juce::jmax(1.0e-4f, e.getLimiterGain()));
    lines[0] = "CPU   " + juce::String(juce::roundToInt(cpu * 100.0f)) + "%";
    lines[1] = "VOICE " + juce::String(e.getActiveVoices()) + "/" + juce::String(processor.getValue(P::Polyphony), 0);
    lines[2] = "PEAK  " + (peak > 1.0e-5f ? juce::String(20.0f * std::log10(peak), 1) : juce::String("-inf"));
    lines[3] = e.getBadSampleEvents() > 0 ? "GUARD " + juce::String((int) e.getBadSampleEvents())
             : "LIMIT " + (processor.getValue(P::Limiter) >= 0.5f ? juce::String(gr, 1) : juce::String("off"));
    warn[0] = cpu > 0.8f;
    warn[3] = e.getBadSampleEvents() > 0;
    repaint();
}

void OutputInfo::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    auto inner = r.reduced(6.0f, 4.0f).toNearestInt();
    const int lineH = inner.getHeight() / 4;
    for (int i = 0; i < 4; ++i)
        drawLcdText(g, lines[i], inner.removeFromTop(lineH), 11.0f, warn[i] ? Colours::red : Colours::lcdText);
}

// ================================================================== valve ====
uint64_t ValveDisplay::stateHash() const
{
    uint64_t h = 1469598103934665603ull;
    for (int i : { (int) P::ValveOn, (int) P::ValveType, (int) P::ValveDrive })
        h = mix(h, v(i));
    const auto& e = processor.getEngine();
    // Heater flicker follows the signal (quantised so idle panels do not repaint).
    h = mix(h, (float) juce::roundToInt(juce::jmax(e.getPeak(0), e.getPeak(1)) * 12.0f));
    return h;
}

void ValveDisplay::paint(juce::Graphics& g)
{
    // A small glass window into the synth: a 9-pin preamp valve on its socket.
    // The heaters glow brighter with DRIVE and flicker slightly with the signal.
    const auto r = getLocalBounds().toFloat();
    const bool on = v(P::ValveOn) >= 0.5f;
    const int type = juce::jlimit(0, 3, iv(P::ValveType));
    const auto& e = processor.getEngine();
    const float signal = juce::jmin(1.0f, juce::jmax(e.getPeak(0), e.getPeak(1)) * 1.5f);
    const float glow = on ? juce::jlimit(0.0f, 1.0f, 0.12f + 0.78f * v(P::ValveDrive) + 0.1f * signal) : 0.0f;
    const auto heat = juce::Colour(0xffff6a12);
    const auto hot = juce::Colour(0xffffc070);

    // ---- bezel: brushed metal frame with four screws
    juce::ColourGradient metal(juce::Colour(0xff6b6e74), r.getX(), r.getY(), juce::Colour(0xff26282c), r.getRight(), r.getBottom(), false);
    g.setGradientFill(metal);
    g.fillRoundedRectangle(r, 10.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
    for (auto p : { r.getTopLeft() + juce::Point<float>(8, 8), r.getTopRight() + juce::Point<float>(-8, 8),
                    r.getBottomLeft() + juce::Point<float>(8, -8), r.getBottomRight() + juce::Point<float>(-8, -8) })
    {
        g.setColour(juce::Colour(0xff1a1b1e));
        g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre(p));
        g.setColour(juce::Colour(0xff8d9096));
        g.drawLine(p.x - 2.0f, p.y + 2.0f, p.x + 2.0f, p.y - 2.0f, 1.0f);
    }

    // ---- the cavity behind the glass
    const auto win = r.reduced(14.0f, 12.0f);
    juce::Path winPath;
    winPath.addRoundedRectangle(win, 8.0f);
    g.saveState();
    g.reduceClipRegion(winPath);
    g.setColour(juce::Colour(0xff0b0b0d));
    g.fillRect(win);
    // Warm light thrown onto the chassis by the heaters.
    juce::ColourGradient spill(heat.withAlpha(0.42f * glow), win.getCentreX(), win.getBottom() - win.getHeight() * 0.38f,
                               heat.withAlpha(0.0f), win.getCentreX() + win.getWidth() * 0.62f, win.getBottom(), true);
    g.setGradientFill(spill);
    g.fillRect(win);

    // Chassis floor and socket.
    const float floorY = win.getBottom() - 16.0f;
    g.setColour(juce::Colour(0xff1d1e21));
    g.fillRect(win.getX(), floorY, win.getWidth(), win.getBottom() - floorY);
    const float vx = win.getCentreX();
    const float vw = 40.0f;
    g.setColour(juce::Colour(0xff2b2a27));
    g.fillRoundedRectangle(vx - vw * 0.62f, floorY - 5.0f, vw * 1.24f, 9.0f, 3.0f);

    // ---- the valve
    const float top = win.getY() + 10.0f;
    const float bottom = floorY - 4.0f;
    juce::Rectangle<float> tube(vx - vw * 0.5f, top, vw, bottom - top);
    // Pins into the socket.
    g.setColour(juce::Colour(0xffa7a9ad));
    for (int k = 0; k < 5; ++k)
        g.fillRect(vx - 12.0f + (float) k * 6.0f, bottom - 1.0f, 1.2f, 5.0f);

    // Internal structure: mica spacers, two triode plate assemblies, heater glow.
    const auto plates = juce::Rectangle<float>(tube.getX() + 6.0f, tube.getY() + tube.getHeight() * 0.36f, tube.getWidth() - 12.0f,
                                               tube.getHeight() * 0.42f);
    // Heater bloom behind and between the plates.
    juce::ColourGradient bloom(hot.withAlpha(0.95f * glow), plates.getCentreX(), plates.getCentreY() + 4.0f,
                               heat.withAlpha(0.0f), plates.getCentreX() + plates.getWidth() * 0.95f, plates.getCentreY(), true);
    g.setGradientFill(bloom);
    g.fillEllipse(plates.expanded(14.0f, 10.0f));
    for (int half = 0; half < 2; ++half)
    {
        const float w = plates.getWidth() * 0.5f - 2.0f;
        const auto pl = juce::Rectangle<float>(plates.getX() + (float) half * (w + 4.0f), plates.getY(), w, plates.getHeight());
        juce::ColourGradient plateGrad(juce::Colour(0xff4d4f54), pl.getX(), 0.0f, juce::Colour(0xff222326), pl.getRight(), 0.0f, false);
        g.setGradientFill(plateGrad);
        g.fillRect(pl);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        for (int k = 1; k < 4; ++k)
            g.drawVerticalLine(juce::roundToInt(pl.getX() + pl.getWidth() * (float) k / 4.0f), pl.getY(), pl.getBottom());
        // Glowing heater visible through the slot in each plate.
        const auto slot = juce::Rectangle<float>(2.4f, pl.getHeight() * 0.62f).withCentre(pl.getCentre());
        g.setColour(juce::Colour(0xff3a1a08).interpolatedWith(hot, glow));
        g.fillRoundedRectangle(slot, 1.2f);
        g.setColour(hot.withAlpha(0.6f * glow));
        g.fillRoundedRectangle(slot.expanded(1.8f * glow, 1.0f), 2.0f);
    }
    g.setColour(juce::Colour(0xffd8d2c4).withAlpha(0.55f));
    g.fillRect(tube.getX() + 3.0f, plates.getY() - 6.0f, tube.getWidth() - 6.0f, 3.0f);
    g.fillRect(tube.getX() + 3.0f, plates.getBottom() + 3.0f, tube.getWidth() - 6.0f, 3.0f);
    g.setColour(juce::Colour(0xff8e9096));
    for (int k = 0; k < 3; ++k)
        g.fillRect(tube.getX() + 8.0f + (float) k * 11.0f, plates.getBottom() + 6.0f, 1.0f, bottom - plates.getBottom() - 8.0f);

    // Getter flash: the silvered dome at the top.
    const auto getter = juce::Rectangle<float>(tube.getX() + 1.5f, tube.getY() + 4.0f, tube.getWidth() - 3.0f, tube.getHeight() * 0.22f);
    juce::ColourGradient silver(juce::Colour(0xffc9ccd2), getter.getX(), getter.getY(), juce::Colour(0xff3c3e44), getter.getRight(), getter.getBottom(), false);
    silver.addColour(0.45, juce::Colour(0xff7a7d84));
    g.setGradientFill(silver);
    g.fillRoundedRectangle(getter, 8.0f);

    // Glass envelope, seal tip and reflections.
    juce::Path glass;
    glass.addRoundedRectangle(tube.getX(), tube.getY(), tube.getWidth(), tube.getHeight(), tube.getWidth() * 0.45f,
                              tube.getWidth() * 0.45f, true, true, false, false);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillPath(glass);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.strokePath(glass, juce::PathStrokeType(1.0f));
    g.fillRoundedRectangle(vx - 3.0f, tube.getY() - 4.0f, 6.0f, 6.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.fillRoundedRectangle(tube.getX() + 5.0f, tube.getY() + 10.0f, 3.0f, tube.getHeight() * 0.7f, 1.5f);
    g.setColour(heat.withAlpha(0.35f * glow));
    g.strokePath(glass, juce::PathStrokeType(1.6f));

    // Type printed on the glass.
    static const char* names[4] = { "12AX7", "12AU7", "EL84", "6L6" };
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText(names[type], tube.withTrimmedTop(tube.getHeight() * 0.25f).withHeight(11.0f).toNearestInt(), juce::Justification::centred, false);

    // Window glass: a faint diagonal reflection across the whole port.
    juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.10f), win.getX(), win.getY(), juce::Colours::white.withAlpha(0.0f),
                               win.getX() + win.getWidth() * 0.55f, win.getY() + win.getHeight() * 0.45f, false);
    g.setGradientFill(sheen);
    g.fillRect(win);
    g.restoreState();

    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(win, 8.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(win.expanded(1.5f), 9.0f, 1.0f);
}

// ============================================================ voice LEDs ====
void VoiceLeds::refresh()
{
    const uint32_t m = processor.getEngine().getVoiceMask();
    const int l = (int) std::lround(processor.getValue(P::Polyphony));
    if (m != mask || l != limit)
    {
        mask = m;
        limit = l;
        repaint();
    }
}

void VoiceLeds::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    auto inner = r.reduced(8.0f, 6.0f);
    g.setFont(lcdFont(9.0f));
    g.setColour(Colours::lcdDim.brighter(0.6f));
    g.drawText("VOICES", inner.removeFromLeft(44.0f).toNearestInt(), juce::Justification::centredLeft, false);
    const float cell = inner.getWidth() / 16.0f;
    for (int i = 0; i < 32; ++i)
    {
        const float x = inner.getX() + (float) (i % 16) * cell + cell * 0.5f;
        const float y = inner.getY() + (i < 16 ? 0.25f : 0.75f) * inner.getHeight();
        const bool on = (mask >> i) & 1u;
        const bool available = i < limit;
        const auto led = juce::Rectangle<float>(7.0f, 7.0f).withCentre({ x, y });
        if (on)
        {
            g.setColour(Colours::lcdText.withAlpha(0.3f));
            g.fillEllipse(led.expanded(3.0f));
        }
        g.setColour(on ? Colours::lcdText : (available ? Colours::lcdDim : Colours::lcdDim.withAlpha(0.25f)));
        g.fillEllipse(led);
    }
}

// =============================================================== seq LEDs ====
void SeqLeds::refresh()
{
    const int s = processor.getEngine().getSeqStep();
    const int len = (int) std::lround(processor.getValue(P::SeqLength));
    const bool run = std::lround(processor.getValue(P::SeqMode)) != 0;
    if (s != step || len != length || run != running)
    {
        step = s;
        length = len;
        running = run;
        repaint();
    }
}

void SeqLeds::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    for (int i = 0; i < 16; ++i)
    {
        const float x = r.getX() + (float) i * pitch;
        const auto led = juce::Rectangle<float>(x + 4.0f, r.getY() + 2.0f, ledWidth - 8.0f, 6.0f);
        const bool inPattern = i < length;
        const bool current = running && i == step;
        g.setColour(current ? Colours::ledOn : (inPattern ? Colours::ledOff.brighter(0.3f) : Colours::ledOff.withAlpha(0.3f)));
        g.fillRoundedRectangle(led, 2.0f);
    }
}

// ================================================================== arp ====
void ArpDisplay::refresh()
{
    const auto& e = processor.getEngine();
    const double t = e.getTempo();
    const bool h = e.isHostTempo();
    const int rt = e.getNoteRoute();
    if (std::abs(t - tempo) > 0.01 || h != host || rt != route)
    {
        tempo = t;
        host = h;
        route = rt;
        repaint();
    }
}

void ArpDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    auto inner = r.reduced(10.0f, 10.0f).toNearestInt();
    const int lh = inner.getHeight() / 4;
    drawLcdText(g, "TEMPO " + juce::String(tempo, 1) + " BPM", inner.removeFromTop(lh), 14.0f);
    drawLcdText(g, host ? "CLOCK  HOST" : "CLOCK  INTERNAL", inner.removeFromTop(lh), 12.0f, Colours::lcdText.withAlpha(0.8f));
    static const char* routes[] = { "NOTES  DIRECT", "NOTES  ARPEGGIATOR", "NOTES  SEQUENCER" };
    drawLcdText(g, routes[juce::jlimit(0, 2, route)], inner.removeFromTop(lh), 12.0f, route == 0 ? Colours::lcdText.withAlpha(0.8f) : Colours::lcdText);
    drawLcdText(g, formatValue(P::ArpMode, v(P::ArpMode)).toUpperCase() + "  " + formatValue(P::ArpDiv, v(P::ArpDiv)), inner, 12.0f,
                Colours::lcdDim.brighter(0.7f));
}

// ================================================================== LEDs ====
void ActivityLed::refresh()
{
    float target = 0.0f;
    if (midi)
    {
        const uint32_t c = processor.getEngine().getMidiActivity();
        if (c != lastCount)
        {
            lastCount = c;
            target = 1.0f;
        }
    }
    else
    {
        const uint32_t c = processor.getAudioCallbackCount();
        const bool running = c != lastCount;
        lastCount = c;
        const float peak = juce::jmax(processor.getEngine().getPeak(0), processor.getEngine().getPeak(1));
        target = running ? (peak > 0.0005f ? 1.0f : 0.25f) : 0.0f;
    }
    const float g = juce::jmax(target, glow * (midi ? 0.75f : 0.6f));
    if (std::abs(g - glow) > 0.01f)
    {
        glow = g;
        repaint();
    }
}

void ActivityLed::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(Colours::text);
    g.setFont(labelFont(10.0f));
    g.drawText(midi ? "MIDI IN" : "AUDIO", r.removeFromTop(14.0f).toNearestInt(), juce::Justification::centred, false);
    const auto led = juce::Rectangle<float>(12.0f, 12.0f).withCentre(r.getCentre());
    const auto col = midi ? Colours::ledOn : Colours::green;
    if (glow > 0.05f)
    {
        g.setColour(col.withAlpha(0.35f * glow));
        g.fillEllipse(led.expanded(5.0f));
    }
    g.setColour(col.interpolatedWith(Colours::ledOff, 1.0f - glow));
    g.fillEllipse(led);
    g.setColour(juce::Colours::black);
    g.drawEllipse(led, 1.0f);
}

void VoiceCountDisplay::refresh()
{
    const int a = processor.getEngine().getActiveVoices();
    const int l = (int) std::lround(processor.getValue(P::Polyphony));
    if (a != voices || l != limit)
    {
        voices = a;
        limit = l;
        repaint();
    }
}

void VoiceCountDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    fillPanelInset(g, r);
    auto inner = r.reduced(8.0f, 4.0f).toNearestInt();
    drawLcdText(g, "POLYPHONY", inner.removeFromTop(16), 10.0f, Colours::lcdDim.brighter(0.7f), juce::Justification::centred);
    drawLcdText(g, juce::String(voices).paddedLeft('0', 2) + " / " + juce::String(limit).paddedLeft('0', 2), inner, 17.0f,
                voices >= limit ? Colours::red : Colours::lcdText, juce::Justification::centred);
}

// ================================================================= preset ====
PresetDisplay::PresetDisplay(ZSynthProcessor& p) : Display(p)
{
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    processor.getPresets().addChangeListener(this);
    setTooltip("Current preset. Click to open the preset browser.");
}

PresetDisplay::~PresetDisplay()
{
    processor.getPresets().removeChangeListener(this);
}

void PresetDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawLcd(g, r);
    auto& pm = processor.getPresets();
    auto inner = r.reduced(12.0f, 5.0f).toNearestInt();
    auto top = inner.removeFromTop(14);
    const int idx = pm.getCurrentIndex();
    drawLcdText(g, pm.getCurrentCategory().toUpperCase(), top, 10.5f, Colours::lcdDim.brighter(0.8f));
    drawLcdText(g, idx >= 0 ? juce::String(idx + 1).paddedLeft('0', 3) + " / " + juce::String((int) pm.getEntries().size()) : juce::String("---"),
                top, 10.5f, Colours::lcdDim.brighter(0.8f), juce::Justification::centredRight);
    drawLcdText(g, pm.getCurrentName(), inner, 22.0f);
}

// =================================================================== logo ====
void Logo::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float h = r.getHeight() - 18.0f;   // mark height; the subtitle sits below
    const float x = r.getX() + 4.0f, y = r.getY() + 2.0f;

    // Stylised Z mark.
    juce::Path z;
    z.startNewSubPath(x, y);
    z.lineTo(x + h * 0.82f, y);
    z.lineTo(x + h * 0.82f, y + h * 0.17f);
    z.lineTo(x + h * 0.27f, y + h * 0.83f);
    z.lineTo(x + h * 0.82f, y + h * 0.83f);
    z.lineTo(x + h * 0.82f, y + h);
    z.lineTo(x, y + h);
    z.lineTo(x, y + h * 0.83f);
    z.lineTo(x + h * 0.55f, y + h * 0.17f);
    z.lineTo(x, y + h * 0.17f);
    z.closeSubPath();

    // Soft glow behind the mark.
    for (int i = 4; i >= 1; --i)
    {
        g.setColour(Colours::accent.withAlpha(0.05f));
        g.strokePath(z, juce::PathStrokeType((float) i * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ColourGradient grad(Colours::accent.brighter(0.35f), x, y, Colours::accent.darker(0.3f), x, y + h, false);
    g.setGradientFill(grad);
    g.fillPath(z);

    // "YNTH" follows the Z mark, set tall and wide, so the mark reads as the
    // first letter of ZYNTH.
    const float textX = x + h * 0.9f;
    auto font = juce::Font(juce::FontOptions(h * 1.12f, juce::Font::bold)).withExtraKerningFactor(0.1f);
    g.setFont(font);
    juce::ColourGradient tg(juce::Colours::white, 0.0f, y, Colours::white.darker(0.25f), 0.0f, y + h, false);
    g.setGradientFill(tg);
    g.drawText("YNTH", juce::Rectangle<float>(textX, y - h * 0.12f, r.getRight() - textX, h * 1.24f).toNearestInt(),
               juce::Justification::centredLeft, false);

    // Subtitle with an accent rule.
    const float subY = y + h + 6.0f;
    g.setColour(Colours::accent.withAlpha(0.8f));
    g.fillRect(x, subY + 4.0f, h * 0.82f, 2.0f);
    g.setColour(Colours::text.withAlpha(0.75f));
    g.setFont(labelFont(10.5f).withExtraKerningFactor(0.42f));
    g.drawText("HYBRID SYNTHESIS INSTRUMENT", juce::Rectangle<float>(x + h * 0.9f, subY - 2.0f, r.getWidth() - h, 12.0f).toNearestInt(),
               juce::Justification::centredLeft, false);
}

} // namespace zsynth::ui
