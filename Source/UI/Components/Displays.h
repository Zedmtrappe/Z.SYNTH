#pragma once

// Read-only visual feedback elements of the panel. Each display reads
// parameter values / engine status atomics in refresh() (called by the panel's
// timer) and only repaints when what it shows has changed.

#include "../../Plugin/ZSynthProcessor.h"
#include "Theme.h"

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
    EnvelopeDisplay(ZSynthProcessor& p, int envIndex) : CachedDisplay(p), env(envIndex) {}
    void paint(juce::Graphics&) override;
protected:
    uint64_t stateHash() const override;
private:
    int env;
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

class OutputMeter : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    float level[2] {}, hold[2] {};
    int holdTimer[2] {};
};

class OutputInfo : public Display
{
public:
    using Display::Display;
    void refresh() override;
    void paint(juce::Graphics&) override;
private:
    juce::String lines[7];
    float cpu = 0.0f;
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
private:
    int step = -1, length = 16;
    bool running = false;
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
