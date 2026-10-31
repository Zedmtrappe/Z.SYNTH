#pragma once

// Musical typing: play ZYNTH from the computer keyboard (the layout Logic Pro
// uses), shown on a floating window with a drawing of a Mac keyboard.
//
//   A S D F G H J K L ; '   white notes, C upwards
//   W E   T Y U   O P       black notes
//   Z / X                   octave down / up        C / V   velocity down / up
//   Tab                     sustain pedal (held)    1 / 2   pitch bend down / up (held)
//   3 4 5 6 7 8             modulation wheel off, 20 % ... 100 %
//
// Keys are read as held/released states (not typed characters), so notes
// sound for exactly as long as a key is down. Notes go through the
// processor's MidiKeyboardState, the same path as the on-screen keyboard.

#include "../../Plugin/ZSynthProcessor.h"
#include "Materials.h"

#include <map>
#include <set>

namespace zsynth::ui
{

class MusicalTyping
{
public:
    enum class Kind { Note, OctaveDown, OctaveUp, VelocityDown, VelocityUp, Sustain, BendDown, BendUp, Mod, None };

    struct Key
    {
        int keyCode;            // JUCE key code (lower-case character or special key)
        juce::String cap;       // legend printed on the key
        float width;            // in key units
        Kind kind = Kind::None;
        int value = 0;          // semitone offset (Note) or mod step (Mod)
    };

    explicit MusicalTyping(ZSynthProcessor& p);
    ~MusicalTyping();

    /** Rows of the drawn keyboard, top to bottom. */
    static const std::vector<std::vector<Key>>& layout();

    /** Polls the computer keyboard; returns true if a mapped key changed. */
    bool keyStateChanged();
    /** True for keys this feature uses (so they are not passed on). */
    bool isMapped(const juce::KeyPress&) const;

    /** Mouse / programmatic press and release of a key code. */
    void press(int keyCode);
    void release(int keyCode);
    bool isDown(int keyCode) const { return down.count(keyCode) > 0; }

    /** Releases everything (window closed, focus lost). */
    void allOff();

    int getBaseNote() const { return baseNote; }
    int getVelocity() const { return velocity; }
    bool isSustaining() const { return sustain; }
    int getModStep() const { return modStep; }
    juce::String noteName(int semitoneOffset) const;

    std::function<void()> onChange;

private:
    const Key* find(int keyCode) const;
    void act(const Key& k, bool pressed);

    ZSynthProcessor& processor;
    std::map<int, int> down;     // key code -> note playing (or -1 for non-note keys)
    std::set<int> mouseDown;     // keys held by the mouse rather than the keyboard
    int baseNote = 60;           // note of the A key
    int velocity = 100;
    int modStep = 0;
    bool sustain = false;
    int bend = 0;
};

/** The drawing of the keyboard, with live key states; keys can be clicked. */
class MusicalTypingComponent : public juce::Component
{
public:
    explicit MusicalTypingComponent(MusicalTyping& t);
    ~MusicalTypingComponent() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void focusLost(FocusChangeType) override;

    static constexpr int kWidth = 900, kHeight = 360;

private:
    int keyAt(juce::Point<float>) const;
    std::vector<std::pair<juce::Rectangle<float>, const MusicalTyping::Key*>> keyRects() const;

    MusicalTyping& typing;
    int mouseKey = -1;
};

/** Floating window holding the drawing. Hiding it releases all notes. */
class MusicalTypingWindow : public juce::DocumentWindow
{
public:
    explicit MusicalTypingWindow(MusicalTyping& t);
    void closeButtonPressed() override;
    void setVisible(bool) override;

private:
    MusicalTyping& typing;
};

} // namespace zsynth::ui
