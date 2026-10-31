#pragma once

// Read-only visual feedback elements of the panel. Each display reads
// parameter values / engine status atomics in refresh() (called by the panel's
// timer) and only repaints when what it shows has changed.

#include "../../Plugin/ZSynthProcessor.h"
#include "Theme.h"
#include "../../DSP/Effects/ParametricEq.h"

namespace zsynth::ui
{

class Display : public juce::Component
{
public:
    explicit Display(ZSynthProcessor& p) : processor(p) { setInterceptsMouseClicks(false, false); }
    virtual void refresh() {}

protected:
    float v(int index) const { return processor.getValue(index); }
    int iv(int index) const { return (int) std::lround(processor.getValue(index)); }
    ZSynthProcessor& processor;
};

/** Repaints only when the hash of what is shown changes. */
class CachedDisplay : public Display
{
public:
    using Display::Display;
    void refresh() override
    {
        const uint64_t h = stateHash();
        if (h != lastHash)
        {
            lastHash = h;
            repaint();
        }
    }

protected:
    virtual uint64_t stateHash() const = 0;
    static uint64_t mix(uint64_t h, float x) { uint32_t b; std::memcpy(&b, &x, 4); return (h ^ b) * 1099511628211ull; }

private:
    uint64_t lastHash = 0;
};

class EnvelopeDisplay : public CachedDisplay
{
public:
    /** firstParam: the envelope's Attack parameter; D, S, R must follow it. */
    EnvelopeDisplay(ZSynthProcessor& p, int firstParam) : CachedDisplay(p), base(firstParam) {}
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
private:
    int base;
};

class WavetableDisplay : public CachedDisplay
{
public:
    using CachedDisplay::CachedDisplay;
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
};

class FmAlgorithmDisplay : public CachedDisplay
{
public:
    using CachedDisplay::CachedDisplay;
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
};

/** Approximate magnitude response of the selected filter (analog prototypes). */
class FilterCurveDisplay : public CachedDisplay
{
public:
    using CachedDisplay::CachedDisplay;
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
};

class FilterModelBadge : public CachedDisplay
{
public:
    using CachedDisplay::CachedDisplay;
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
};

class LfoShapeDisplay : public CachedDisplay
{
public:
    LfoShapeDisplay(ZSynthProcessor& p, int lfoIndex) : CachedDisplay(p), lfo(lfoIndex) {}
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
private:
    int lfo;
};

class StereoDisplay : public CachedDisplay
{
public:
    using CachedDisplay::CachedDisplay;
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
};

/** Response curve of the reverb's parametric EQ. The large (interactive)
    version lets you drag each band's node: left/right = frequency,
    up/down = gain, mouse wheel = Q of a bell, double-click = flat band. */
class ReverbEqCurve : public CachedDisplay, public juce::SettableTooltipClient
{
public:
    ReverbEqCurve(ZSynthProcessor& p, bool isInteractive);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
protected:
    uint64_t stateHash() const override;
private:
    struct Band { int freq, gain, q; };
    static constexpr int kBands = 6;
    static const Band bands[kBands];
    dsp::EqSettings settings() const;
    juce::Point<float> nodeFor(int band) const;
    juce::Rectangle<float> plotArea() const;
    float xForFreq(float f) const;
    float freqForX(float x) const;
    float yForDb(float db) const;
    float dbForY(float y) const;
    int bandAt(juce::Point<float>) const;
    void setParam(int index, float value);
    void gesture(int index, bool begin);
    bool interactive;
    int dragging = -1, hoverBand = -1;
};

/** The vintage graphic EQ: its response with a bar per band. The panel
    version is clickable and opens the full EQ window. */
class GraphicEqDisplay : public CachedDisplay, public juce::SettableTooltipClient
{
public:
    GraphicEqDisplay(ZSynthProcessor& p, bool isClickable);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
protected:
    uint64_t stateHash() const override;
private:
    bool clickable;
};

/** Analogue VU meter showing the output compressor's gain reduction. */
class CompMeter : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    float needle = 0.0f, shown = -1.0f;
    bool lit = false;
};

/** Output level meter; its tooltip carries CPU load, voices and peak. */
class OutputMeter : public Display, public juce::SettableTooltipClient
{
public:
    explicit OutputMeter(ZSynthProcessor& p) : Display(p) { setInterceptsMouseClicks(true, false); }
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    float level[2] {}, hold[2] {};
    int holdTimer[2] {};
    int tooltipCounter = 0;
};

/** LED ladder showing SMOOTH's gain reduction. */
class SmoothMeter : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    float shown = 0.0f;
    int litLeds = -1;
};

/** The valve stage: glowing valve(s) whose heaters follow drive and signal,
    with the valve type and operating point. */
class ValveDisplay : public CachedDisplay
{
public:
    using CachedDisplay::CachedDisplay;
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
private:
    /** Static layers rendered once per size/scale/type; only the glow is drawn live. */
    void renderLayers(float scale, int type);
    struct Geometry
    {
        juce::Rectangle<float> window, tube, plates[2], slots[2], getter;
        float floorY = 0.0f;
    };
    Geometry geometry() const;
    juce::Image back, internals, front;
    float layerScale = 0.0f;
    int layerType = -1;
};

class VoiceLeds : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    uint32_t mask = 0;
    int limit = 16;
};

class SeqLeds : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
    int getStep() const { return step; }
    /** Horizontal distance between step LEDs and the width of each LED. */
    void setGeometry(float pitchPx, float ledWidthPx) { pitch = pitchPx; ledWidth = ledWidthPx; }
private:
    int step = -1, length = 16;
    bool running = false;
    float pitch = 39.0f, ledWidth = 22.0f;
};

class ArpDisplay : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    double tempo = 120.0;
    bool host = false;
    int route = 0;
};

class ActivityLed : public Display
{
public:
    ActivityLed(ZSynthProcessor& p, bool isMidi) : Display(p), midi(isMidi) {}
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    bool midi;
    uint32_t lastCount = 0;
    float glow = 0.0f;
};

class VoiceCountDisplay : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    int voices = 0, limit = 16;
};

class PresetDisplay : public Display, public juce::SettableTooltipClient, private juce::ChangeListener
{
public:
    explicit PresetDisplay(ZSynthProcessor& p);
    ~PresetDisplay() override;
    void paint(juce::Graphics&) override;
    std::function<void()> onClick;
    void mouseUp(const juce::MouseEvent&) override { if (onClick) onClick(); }
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }
};

class Logo : public juce::Component
{
public:
    Logo() { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics&) override;
};

} // namespace zsynth::ui
