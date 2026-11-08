#include "Skins.h"

#include "Materials.h"

namespace zsynth::ui
{

namespace
{
    using C = juce::Colour;
    const C kHouseLcdBack = Colours::lcdBack, kHouseLcdText = Colours::lcdText;
    const C kRedLedBack { 0xff170504 }, kRedLed { 0xffff4030 };

    //                 nameplate                        plate          finish                    ink            accent
    //                 knob                     knob colour    fader cap      insert          lcd back        lcd text
    //                 meter face     meter ink      bezel          lamp   label
    const Skin kComp[(int) CompModel::Count] = {
        { "LEVELING AMPLIFIER", C(0xffc3c6c6), PlateFinish::Paint, C(0xff1b1c1d), C(0xffc0392b),
          KnobStyle::BlackRound, C(0xff131313), C(0xff1a1a1a), C(0xffe8e4dc), kHouseLcdBack, kHouseLcdText,
          C(0xfff2e6c4), C(0xff1d1a14), C(0xff141414), true, "VU" },
        { "STEREO BUS COMPRESSOR", C(0xff3a3d41), PlateFinish::Paint, C(0xffecebe6), C(0xff4f9bff),
          KnobStyle::Ribbed, C(0xff9aa0a6), C(0xff2a2c30), C(0xffecebe6), kHouseLcdBack, kHouseLcdText,
          C(0xfff4f2ec), C(0xff121212), C(0xff1c1d20), false, "COMP" },
        { "LIMITING AMPLIFIER", C(0xff141416), PlateFinish::Anodised, C(0xffe2ded4), C(0xff4aa3ff),
          KnobStyle::Chrome, C(0xff101010), C(0xff1a1a1a), C(0xffe2ded4), kHouseLcdBack, kHouseLcdText,
          C(0xfff0e0b4), C(0xff1d1a14), C(0xff0b0b0b), true, "VU" },
        { "VARIABLE-MU LIMITER", C(0xff4e5351), PlateFinish::Hammertone, C(0xfff1ede1), C(0xffe0a040),
          KnobStyle::BlackSkirt, C(0xff121110), C(0xff1a1a1a), C(0xfff1ede1), kHouseLcdBack, kHouseLcdText,
          C(0xffeadcb6), C(0xff2a2118), C(0xff2a2926), true, "DB" },
    };

    Skin reverbStyle(const char* plateName, C plate, PlateFinish finish, C ink, C accent, KnobStyle knob, C knobColour,
                     C faderCap, C insert, C lcdBack, C lcdText)
    {
        return { plateName, plate, finish, ink, accent, knob, knobColour, faderCap, insert, lcdBack, lcdText,
                 C(0xfff2e6c4), C(0xff1d1a14), C(0xff141414), true, "" };
    }
}

juce::Colour Skin::capFor(const juce::String& paramId) const
{
    if (knob != KnobStyle::Ribbed)
        return knobColour;
    // Neve: frequency switches grey, gains blue, the high-pass red.
    if (paramId.startsWith("nev."))
        return paramId == "nev.hpf" ? C(0xffb9312a) : (paramId.endsWith("g") ? C(0xff3a74c4) : C(0xff9aa0a6));
    // API: frequency rings grey, gains API blue.
    if (paramId.startsWith("a5.") || paramId.startsWith("b5."))
        return paramId.endsWith("g") ? C(0xff2f6fd1) : C(0xff8a8f96);
    if (paramId == "eq.drive" || paramId == "peq.out")
        return C(0xffd8d8d4);
    // Bus-compressor colour coding.
    if (paramId == "comp.thresh")                                return C(0xffd8d8d4);
    if (paramId == "comp.makeup")                                return C(0xffc8322d);
    if (paramId == "comp.mix")                                   return C(0xffe2b623);
    if (paramId == "comp.attack" || paramId == "comp.release")   return C(0xff2f6fd1);
    if (paramId == "comp.knee" || paramId == "comp.emph")        return C(0xff3f9a52);
    return knobColour;
}

const Skin& compSkin(CompModel model)
{
    return kComp[juce::jlimit(0, (int) CompModel::Count - 1, (int) model)];
}

const Skin* reverbSkin(ReverbType type)
{
    static const Skin plate = reverbStyle("PLATE REVERBERATION UNIT", C(0xff8e9a91), PlateFinish::Hammertone, C(0xff15181a),
                                          C(0xffe8e4dc), KnobStyle::ChickenHead, C(0xff111111), C(0xff1a1a1a), C(0xffe8e8e8),
                                          kHouseLcdBack, kHouseLcdText);
    static const Skin chamber = reverbStyle("ECHO CHAMBER", C(0xff4b4338), PlateFinish::Paint, C(0xffeee3c8), C(0xffe8c26a),
                                            KnobStyle::BlackSkirt, C(0xff141210), C(0xff2a2620), C(0xffeee3c8), kHouseLcdBack, kHouseLcdText);
    static const Skin spring = reverbStyle("SPRING REVERB", C(0xff1c1c1d), PlateFinish::Paint, C(0xffe6e1d3), C(0xffd6453a),
                                           KnobStyle::ChickenHead, C(0xff141414), C(0xffb9bcc0), C(0xffc0392b), kHouseLcdBack, kHouseLcdText);
    static const Skin lex224 = reverbStyle("DIGITAL REVERBERATOR", C(0xff2f2c2a), PlateFinish::Brushed, C(0xffe7e2d6), C(0xffff4a3a),
                                           KnobStyle::BlackSkirt, C(0xff141414), C(0xff9b9d9f), C(0xff1a1a1a), kRedLedBack, kRedLed);
    static const Skin lex480 = reverbStyle("DIGITAL EFFECTS SYSTEM", C(0xff19191b), PlateFinish::Anodised, C(0xffd9d6cf), C(0xffff4a3a),
                                           KnobStyle::BlackSkirt, C(0xff141414), C(0xff8d9094), C(0xff161616), kRedLedBack, kRedLed);
    static const Skin bricasti = reverbStyle("STEREO REVERB PROCESSOR", C(0xffc8cbce), PlateFinish::Brushed, C(0xff23262a),
                                             C(0xff5aa8ff), KnobStyle::Silver, C(0xffd5d8db), C(0xffd0d3d6), C(0xff23262a),
                                             C(0xff04070b), C(0xffc4e4ff));
    static const Skin ams = reverbStyle("DIGITAL REVERBERATION", C(0xff3d5c7e), PlateFinish::Paint, C(0xfff2f2ee), C(0xffff4a3a),
                                        KnobStyle::BlackSkirt, C(0xff151515), C(0xff1c1c1e), C(0xfff2f2ee), kRedLedBack, kRedLed);
    static const Skin rev7 = reverbStyle("DIGITAL REVERBERATOR", C(0xffb0b2af), PlateFinish::Paint, C(0xff1d1e1f), C(0xff6fd04a),
                                         KnobStyle::Silver, C(0xffd0d2d4), C(0xff2a2b2d), C(0xff9be36a), C(0xff0d1709), C(0xffa6ec74));
    static const Skin sp2016 = reverbStyle("EFFECTS PROCESSOR", C(0xff161a2a), PlateFinish::Anodised, C(0xffdfe4ee), C(0xffffa040),
                                           KnobStyle::BlackSkirt, C(0xff141414), C(0xff2d3346), C(0xffffa040), C(0xff170904), C(0xffff7a33));
    static const Skin qrs = reverbStyle("ROOM SIMULATOR", C(0xffcbc2a9), PlateFinish::Paint, C(0xff2a2620), C(0xff3fae5a),
                                        KnobStyle::BlackSkirt, C(0xff1a1a1a), C(0xff3a352d), C(0xffe0d6bc), C(0xff06120a), C(0xff8ff07a));
    static const Skin pcm70 = reverbStyle("DIGITAL EFFECTS PROCESSOR", C(0xff161617), PlateFinish::Anodised, C(0xffe8e6e0),
                                          C(0xffff4a3a), KnobStyle::BlackSkirt, C(0xff121212), C(0xff2a2a2c), C(0xffff3b2a),
                                          kRedLedBack, kRedLed);
    switch (type)
    {
        case ReverbType::Plate:          return &plate;
        case ReverbType::Chamber:        return &chamber;
        case ReverbType::Spring:         return &spring;
        case ReverbType::Lex224:         return &lex224;
        case ReverbType::Lex480:         return &lex480;
        case ReverbType::BricastiM7:     return &bricasti;
        case ReverbType::AmsNonlin:
        case ReverbType::AmsReverse:     return &ams;
        case ReverbType::YamahaRev7:     return &rev7;
        case ReverbType::EventideSP2016: return &sp2016;
        case ReverbType::QuantecQRS:     return &qrs;
        case ReverbType::LexPCM70:       return &pcm70;
        default:                         return nullptr; // Hall, Room, Shimmer: the ZYNTH house look
    }
}

const Skin& sequencerSkin()
{
    static const Skin s = reverbStyle("ANALOG SEQUENCER  -  16 STEPS x 2", C(0xff1b1b1d), PlateFinish::Anodised, C(0xffe9e6df), kRedLed,
                                      KnobStyle::BlackSkirt, C(0xff141414), C(0xff2a2a2c), C(0xffe9e6df), kRedLedBack, kRedLed);
    return s;
}

const Skin& sequencerRowSkin()
{
    static const Skin s = reverbStyle("", C(0xff1b1b1d), PlateFinish::Anodised, C(0xffe9e6df), kRedLed, KnobStyle::BlackSkirt,
                                      C(0xff141414), C(0xff2a2a2c), C(0xffe9e6df), kRedLedBack, kRedLed);
    return s;
}

const Skin* eqSkin(EqModel model)
{
    auto skin = [](const char* plateName, C plate, PlateFinish finish, C ink, KnobStyle knob, C knobColour, C faderCap, C insert) {
        return Skin { plateName, plate, finish, ink, C(0xffe8c26a), knob, knobColour, faderCap, insert, kHouseLcdBack, kHouseLcdText,
                      C(0xfff2e6c4), C(0xff1d1a14), C(0xff141414), true, "" };
    };
    static const Skin pultec = skin("PROGRAM EQUALIZER", C(0xff5b6e7c), PlateFinish::Hammertone, C(0xfff2f0ea),
                                    KnobStyle::ChickenHead, C(0xff121212), C(0xff1a1a1a), C(0xfff2f0ea));
    static const Skin neve = skin("CLASS A EQUALISER", C(0xff667788), PlateFinish::Paint, C(0xfff4f2ec),
                                  KnobStyle::Ribbed, C(0xff9aa0a6), C(0xff2a2c30), C(0xfff4f2ec));
    static const Skin api = skin("DISCRETE EQUALIZER", C(0xff1c1e21), PlateFinish::Anodised, C(0xffeeeeea),
                                 KnobStyle::Ribbed, C(0xff8a8f96), C(0xff1c1c1e), C(0xffeeeeea));
    static const Skin api560 = skin("GRAPHIC EQUALIZER", C(0xff16171a), PlateFinish::Anodised, C(0xffeeeeea),
                                    KnobStyle::Ribbed, C(0xff8a8f96), C(0xff18181a), C(0xffeeeeea));
    switch (model)
    {
        case EqModel::Pultec:   return &pultec;
        case EqModel::Neve1073: return &neve;
        case EqModel::Api550A:
        case EqModel::Api550B:  return &api;
        case EqModel::Api560:   return &api560;
        default:                return nullptr;
    }
}

const Skin& tapeSkin(TapeModel model)
{
    auto skin = [](const char* plateName, C plate, PlateFinish finish, C ink, C accent, KnobStyle knob, C knobColour, C lcdBack, C lcdText) {
        return Skin { plateName, plate, finish, ink, accent, knob, knobColour, C(0xff1a1a1a), ink, lcdBack, lcdText,
                      C(0xfff2e6c4), C(0xff1d1a14), C(0xff141414), true, "" };
    };
    static const Skin skins[(int) TapeModel::Count] = {
        skin("MULTICHANNEL RECORDER", C(0xffb8b7af), PlateFinish::Paint, C(0xff1e1f20), C(0xff3fae5a), KnobStyle::Silver, C(0xffd0d2d4),
             C(0xff06120a), C(0xff8ff07a)),
        skin("MASTERING RECORDER", C(0xff2a2b2e), PlateFinish::Brushed, C(0xffe6e3da), C(0xffe8c26a), KnobStyle::BlackSkirt, C(0xff141414),
             kHouseLcdBack, kHouseLcdText),
        skin("FOUR-TRACK RECORDER", C(0xff8a8f86), PlateFinish::Hammertone, C(0xff15181a), C(0xffe8c26a), KnobStyle::BlackRound, C(0xff131313),
             kHouseLcdBack, kHouseLcdText),
        skin("MULTITRACK RECORDER", C(0xff1a1a1c), PlateFinish::Anodised, C(0xffdcdcd6), C(0xffff4a3a), KnobStyle::BlackSkirt, C(0xff141414),
             kRedLedBack, kRedLed),
        skin("CASSETTE MULTITRACKER", C(0xffc4c6c8), PlateFinish::Brushed, C(0xff222426), C(0xffff7a33), KnobStyle::Silver, C(0xffd5d8db),
             C(0xff170904), C(0xffff7a33)),
    };
    return skins[juce::jlimit(0, (int) TapeModel::Count - 1, (int) model)];
}

void paintFaceplate(juce::Graphics& g, juce::Rectangle<float> r, const Skin& s, const juce::String& title, bool inset)
{
    namespace m = material;
    const float radius = inset ? 3.0f : 6.0f;
    auto plate = r;
    if (inset)
    {
        // The bay, then a plate screwed into it standing slightly proud.
        m::recess(g, r, 4.0f, juce::Colour(0xff050506), 1.0f);
        plate = r.reduced(2.5f);
        m::castShadow(g, plate, radius, 4.0f, { 1, 2 }, 0.8f);
    }
    else
    {
        m::castShadow(g, r, 6.0f, 12.0f, { 2, 7 }, 0.85f);
    }

    switch (s.finish)
    {
        case PlateFinish::Brushed:  m::brushedMetal(g, plate, radius, s.plate, 1.0f); break;
        case PlateFinish::Anodised: m::brushedMetal(g, plate, radius, s.plate, 0.45f); break;
        case PlateFinish::Paint:    m::texturedPlastic(g, plate, radius, s.plate, 0.5f); break;
        case PlateFinish::Hammertone:
        {
            m::texturedPlastic(g, plate, radius, s.plate, 0.4f);
            g.saveState();
            juce::Path clip;
            clip.addRoundedRectangle(plate, radius);
            g.reduceClipRegion(clip);
            // Hammer-paint dimples: overlapping soft cells, lighter rims on the lit side.
            juce::Random rng(0x4a4d4d52);
            const int cells = (int) (plate.getWidth() * plate.getHeight() / 22.0f);
            for (int i = 0; i < cells; ++i)
            {
                const float x = plate.getX() + rng.nextFloat() * plate.getWidth();
                const float y = plate.getY() + rng.nextFloat() * plate.getHeight();
                const float d = 1.6f + rng.nextFloat() * 2.6f;
                g.setColour(juce::Colours::black.withAlpha(0.025f + 0.03f * rng.nextFloat()));
                g.fillEllipse(x, y, d, d);
                g.setColour(juce::Colours::white.withAlpha(0.02f + 0.025f * rng.nextFloat()));
                g.fillEllipse(x - 0.5f, y - 0.5f, d * 0.5f, d * 0.5f);
            }
            g.restoreState();
            break;
        }
    }
    // Soft sheen from the panel light, then the bevel.
    juce::ColourGradient sheen(juce::Colours::white.withAlpha(s.lightPlate() ? 0.10f : 0.05f), plate.getX(), plate.getY(),
                               juce::Colours::transparentWhite, plate.getX(), plate.getY() + plate.getHeight() * 0.5f, false);
    g.setGradientFill(sheen);
    g.fillRoundedRectangle(plate, radius);
    m::raisedEdge(g, plate, radius, inset ? 0.8f : 1.0f);

    const float screwR = inset ? 2.8f : 4.0f;
    const float sx = inset ? 7.0f : 10.0f, sy = inset ? 7.0f : 10.0f;
    for (auto p : { juce::Point<float>(plate.getX() + sx, plate.getY() + sy), juce::Point<float>(plate.getRight() - sx, plate.getY() + sy),
                    juce::Point<float>(plate.getX() + sx, plate.getBottom() - sy),
                    juce::Point<float>(plate.getRight() - sx, plate.getBottom() - sy) })
        m::screw(g, p, screwR, 0.3f + p.x * 0.013f + p.y * 0.007f);

    // Title and the model's nameplate, printed in the plate's ink.
    const float glow = s.lightPlate() ? 0.0f : 0.5f;
    if (inset)
    {
        const auto titleArea = juce::Rectangle<float>(plate.getX() + 14.0f, plate.getY() + 1.0f, plate.getWidth() - 28.0f, 14.0f);
        const auto font = titleFont(10.5f);
        m::engravedText(g, title, titleArea, font, juce::Justification::centredLeft, s.ink, glow);
        const float titleW = juce::GlyphArrangement::getStringWidth(font, title) + 14.0f;
        // Leave the right-hand end of the title row to the ON / PRE FX switches.
        const auto nameArea = juce::Rectangle<float>(titleArea.getX() + titleW, titleArea.getY() + 1.0f,
                                                     juce::jmax(0.0f, plate.getWidth() - titleW - (plate.getWidth() > 300.0f ? 130.0f : 60.0f)), 13.0f);
        if (plate.getWidth() >= 180.0f) // narrow bays carry the title only
            m::engravedText(g, s.nameplate, nameArea, titleFont(8.0f), juce::Justification::centredLeft, s.ink.withMultipliedAlpha(0.75f), 0.0f);
    }
    else
    {
        const auto titleArea = juce::Rectangle<float>(plate.getX() + 23.0f, plate.getY() + 3.0f, plate.getWidth() * 0.5f, 23.0f);
        m::engravedText(g, title, titleArea, titleFont(13.0f), juce::Justification::centredLeft, s.ink, glow);
        // Right-aligned, clear of the window's buttons (close, FLAT).
        const auto nameArea = juce::Rectangle<float>(plate.getCentreX() - 60.0f, plate.getY() + 4.0f, plate.getWidth() * 0.5f - 70.0f, 21.0f);
        m::engravedText(g, s.nameplate, nameArea, titleFont(10.0f), juce::Justification::centred, s.ink.withMultipliedAlpha(0.8f), 0.0f);
        g.setColour(s.ink.withAlpha(0.35f));
        g.drawHorizontalLine((int) (plate.getY() + 27.0f), plate.getX() + 8.0f, plate.getRight() - 8.0f);
    }
}

} // namespace zsynth::ui
