#include "PanelLayout.h"

namespace zsynth::ui
{

juce::Point<int> controlSize(ControlKind kind)
{
    switch (kind)
    {
        case ControlKind::LargeKnob:  return { 72, 90 };
        case ControlKind::Knob:       return { 52, 64 };
        case ControlKind::SmallKnob:  return { 30, 30 };
        case ControlKind::Selector:   return { 90, 40 };
        case ControlKind::Switch:     return { 48, 40 };
        case ControlKind::VSlider:    return { 28, 150 };
        case ControlKind::StepSlider: return { 30, 84 };
        case ControlKind::StepToggle: return { 30, 14 };
        case ControlKind::HSlider:    return { 104, 24 };
        case ControlKind::Segmented:  return { 200, 20 };
        default:                      return { 50, 50 };
    }
}

const PanelLayout& PanelLayout::get()
{
    static const PanelLayout layout;
    return layout;
}

juce::Rectangle<int> PanelLayout::section(const juce::String& id) const
{
    for (const auto& s : sectionList)
        if (s.id == id)
            return s.bounds;
    return {};
}

juce::Rectangle<int> PanelLayout::utility(const juce::String& id) const
{
    for (const auto& u : utilityList)
        if (u.id == id)
            return u.bounds;
    return {};
}

PanelLayout::PanelLayout()
{
    juce::String currentSection;

    auto sectionAt = [&](const juce::String& id, const juce::String& title, int x, int y, int w, int h, bool sub = false) {
        sectionList.push_back({ id, title, { x, y, w, h }, sub });
        currentSection = id;
    };
    auto inSection = [&](const juce::String& id) { currentSection = id; };
    auto add = [&](const char* id, ControlKind kind, int x, int y, const juce::String& label, int w = -1, int h = -1) {
        const auto size = controlSize(kind);
        controlList.push_back({ id, kind, { x, y, w > 0 ? w : size.x, h > 0 ? h : size.y }, label, currentSection });
    };
    auto utilityAt = [&](const juce::String& id, int x, int y, int w, int h) { utilityList.push_back({ id, { x, y, w, h } }); };
    auto text = [&](const juce::String& t, int x, int y, int w, int h, juce::Justification j = juce::Justification::centred) {
        labelList.push_back({ t, { x, y, w, h }, j });
    };

    using K = ControlKind;

    // ================================================================ HEADER
    sectionAt("header", "", 0, 0, kWidth, 72);
    utilityAt("logo", 18, 10, 250, 52);
    utilityAt("presetPrev", 296, 20, 30, 32);
    utilityAt("presetDisplay", 330, 10, 340, 52);
    utilityAt("presetNext", 674, 20, 30, 32);
    utilityAt("presetBrowse", 712, 20, 74, 32);
    utilityAt("presetSave", 790, 20, 58, 32);
    utilityAt("presetSaveAs", 852, 20, 74, 32);
    utilityAt("presetInit", 930, 20, 50, 32);

    add("master.tune",      K::SmallKnob, 1004, 10, "TUNE");
    add("master.transpose", K::SmallKnob, 1060, 10, "TRANSP");
    add("master.octave",    K::SmallKnob, 1116, 10, "OCTAVE");
    add("master.tempo",     K::SmallKnob, 1172, 10, "TEMPO");
    for (auto [t, x] : { std::pair<const char*, int> { "TUNE", 1004 }, { "TRANSP", 1060 }, { "OCTAVE", 1116 }, { "TEMPO", 1172 } })
        text(t, x - 12, 44, 54, 14);
    add("master.oversampling", K::Selector, 1222, 16, "QUALITY", 92);
    add("midi.channel",        K::Selector, 1322, 16, "MIDI CH", 70);
    utilityAt("midiLed", 1402, 16, 56, 40);
    utilityAt("audioLed", 1460, 16, 56, 40);
    utilityAt("voiceCount", 1522, 14, 104, 44);
    add("master.volume", K::Knob, 1640, 4, "MASTER");
    utilityAt("panic", 1710, 20, 88, 34);
    utilityAt("version", 1808, 20, 100, 34);

    // =========================================================== OSCILLATORS
    sectionAt("osc", "OSCILLATORS", 10, 80, 760, 328);
    add("osc.engine", K::Segmented, 560, 83, "ENGINE", 200, 20);
    struct OscIds { const char *on, *wave, *oct, *semi, *fine, *pw, *level, *sync, *fm, *ring; };
    const OscIds oscs[3] = {
        { "osc1.on", "osc1.wave", "osc1.octave", "osc1.semi", "osc1.fine", "osc1.pw", "osc1.level", nullptr, "osc1.fm", nullptr },
        { "osc2.on", "osc2.wave", "osc2.octave", "osc2.semi", "osc2.fine", "osc2.pw", "osc2.level", "osc2.sync", "osc2.fm", "osc2.xmod" },
        { "osc3.on", "osc3.wave", "osc3.octave", "osc3.semi", "osc3.fine", "osc3.pw", "osc3.level", "osc3.sync", "osc3.fm", "osc3.xmod" },
    };
    for (int i = 0; i < 3; ++i)
    {
        const int y = 112 + i * 74;
        text("OSC " + juce::String(i + 1), 16, y + 2, 48, 16, juce::Justification::centredLeft);
        add(oscs[i].on,    K::Switch,   16, y + 20, "ON");
        add(oscs[i].wave,  K::Selector, 70, y + 16, "WAVE", 100);
        add(oscs[i].oct,   K::Selector, 176, y + 16, "RANGE", 58);
        add(oscs[i].semi,  K::Knob,     242, y + 4, "SEMI");
        add(oscs[i].fine,  K::Knob,     298, y + 4, "FINE");
        add(oscs[i].pw,    K::Knob,     354, y + 4, "PW");
        add(oscs[i].level, K::Knob,     410, y + 4, "LEVEL");
        if (oscs[i].sync != nullptr)
            add(oscs[i].sync, K::Switch, 468, y + 20, "SYNC");
        add(oscs[i].fm,    K::Knob,     522, y + 4, i == 0 ? "FM 3>1" : "FM 1>" + juce::String(i + 1));
        if (oscs[i].ring != nullptr)
            add(oscs[i].ring, K::Knob,  578, y + 4, "RING");
    }
    // Sub / noise / character row
    add("osc.sub.level",   K::Knob,     24, 338, "SUB");
    add("osc.sub.wave",    K::Selector, 82, 350, "SUB WAVE", 96);
    add("osc.noise.level", K::Knob,     196, 338, "NOISE");
    add("osc.noise.type",  K::Selector, 254, 350, "COLOUR", 76);
    add("osc.drift",       K::Knob,     350, 338, "DRIFT");
    add("mix.drive",       K::Knob,     408, 338, "DRIVE");
    utilityAt("oscDivider", 16, 334, 620, 2);

    sectionAt("wt", "WAVETABLE", 640, 112, 124, 290, true);
    add("wt.table",  K::Selector, 647, 140, "TABLE", 110);
    utilityAt("wtDisplay", 647, 188, 110, 64);
    add("wt.pos",    K::Knob,     647, 262, "POSITION");
    add("wt.spread", K::Knob,     705, 262, "SPREAD");
    inSection("osc");

    // ================================================================ FILTER
    sectionAt("filter", "FILTER", 778, 80, 336, 328);
    add("flt.model",  K::Selector, 788, 112, "MODEL", 96);
    add("flt.type",   K::Selector, 890, 112, "TYPE", 72);
    add("flt.slope",  K::Selector, 968, 112, "SLOPE", 76);
    add("flt.envinv", K::Switch,   1056, 112, "INVERT");
    add("flt.cutoff", K::LargeKnob, 792, 160, "CUTOFF");
    add("flt.res",    K::LargeKnob, 880, 160, "RESONANCE");
    add("flt.env",    K::Knob,      980, 166, "ENV AMT");
    add("flt.hpf",    K::Knob,      1046, 166, "HPF");
    add("flt.drive",  K::Knob,      792, 258, "DRIVE");
    add("flt.key",    K::Knob,      852, 258, "KEY");
    add("flt.vel",    K::Knob,      912, 258, "VELO");
    add("flt.lfo",    K::Knob,      972, 258, "LFO 1");
    utilityAt("filterDisplay", 1034, 258, 72, 62);
    utilityAt("filterCurve", 792, 330, 314, 70);

    // ============================================================= ENVELOPES
    sectionAt("env", "ENVELOPES", 1122, 80, 400, 328);
    const char* envNames[3] = { "1  AMP", "2  FILTER", "3  MOD" };
    const char* envIds[3][4] = { { "env1.a", "env1.d", "env1.s", "env1.r" },
                                 { "env2.a", "env2.d", "env2.s", "env2.r" },
                                 { "env3.a", "env3.d", "env3.s", "env3.r" } };
    const char* adsr[4] = { "A", "D", "S", "R" };
    for (int e = 0; e < 3; ++e)
    {
        const int x = 1130 + e * 132;
        text(envNames[e], x, 110, 124, 14, juce::Justification::centredLeft);
        utilityAt("env" + juce::String(e + 1) + "Display", x, 126, 124, 50);
        for (int s = 0; s < 4; ++s)
            add(envIds[e][s], K::VSlider, x + 2 + s * 31, 182, adsr[s]);
    }
    add("env1.vel",  K::Knob,     1130 + 36, 338, "VELO");
    add("env2.vel",  K::Knob,     1262 + 36, 338, "VELO");
    add("env3.amt",  K::Knob,     1394,      338, "AMOUNT");
    add("env3.dest", K::Selector, 1394 + 56, 350, "DEST", 68);

    // ==================================================================== FM
    sectionAt("fm", "FM OPERATORS", 1530, 80, 380, 328);
    add("fm.algo",     K::Selector, 1538, 116, "ALGORITHM", 110);
    add("fm.depth",    K::Knob,     1658, 108, "DEPTH");
    add("fm.feedback", K::Knob,     1716, 108, "FDBK");
    add("fm.level",    K::Knob,     1774, 108, "LEVEL");
    add("fm.vel",      K::Knob,     1832, 108, "VELO");
    utilityAt("fmAlgoDisplay", 1538, 178, 96, 222);
    const char* opCols[7] = { "RATIO", "FINE", "LEVEL", "A", "D", "S", "R" };
    const char* opSuffix[7] = { "ratio", "fine", "level", "a", "d", "s", "r" };
    for (int c = 0; c < 7; ++c)
        text(opCols[c], 1668 + c * 34, 178, 38, 14);
    static juce::StringArray opIds; // keeps the c_str() pointers alive
    if (opIds.isEmpty())
        for (int op = 1; op <= 6; ++op)
            for (int c = 0; c < 7; ++c)
                opIds.add("fm.op" + juce::String(op) + "." + opSuffix[c]);
    for (int op = 0; op < 6; ++op)
    {
        const int y = 194 + op * 34;
        text("OP" + juce::String(op + 1), 1640, y + 8, 30, 14, juce::Justification::centredLeft);
        for (int c = 0; c < 7; ++c)
            add(opIds[op * 7 + c].toRawUTF8(), K::SmallKnob, 1672 + c * 34, y, opCols[c]);
    }

    // ================================================================== LFOs
    sectionAt("lfo", "LFOs", 10, 416, 440, 290);
    for (int l = 0; l < 2; ++l)
    {
        const int y = 444 + l * 130;
        const juce::String n(l + 1);
        sectionAt("lfo" + n, "", 16, y, 428, 124, true);
        text("LFO " + n, 22, y + 6, 40, 16, juce::Justification::centredLeft);
        static juce::StringArray lfoIds;
        if (lfoIds.size() < 20)
            for (int k = 1; k <= 2; ++k)
                for (auto s : { "wave", "mode", "dest", "sync", "uni", "rate", "div", "phase", "delay", "amt" })
                    lfoIds.add("lfo" + juce::String(k) + "." + s);
        auto id = [&](int k) { return lfoIds[l * 10 + k].toRawUTF8(); };
        add(id(0), K::Selector, 64, y + 6, "WAVE", 96);
        add(id(1), K::Selector, 166, y + 6, "TRIGGER", 72);
        add(id(2), K::Selector, 244, y + 6, "DEST", 88);
        add(id(3), K::Switch,   338, y + 6, "SYNC");
        add(id(4), K::Switch,   390, y + 6, "UNIPOLAR");
        add(id(5), K::Knob,     64,  y + 54, "RATE");
        add(id(6), K::Knob,     134, y + 54, "DIVISION");
        add(id(7), K::Knob,     204, y + 54, "PHASE");
        add(id(8), K::Knob,     274, y + 54, "DELAY");
        add(id(9), K::Knob,     344, y + 54, "AMOUNT");
        utilityAt("lfo" + n + "Display", 22, y + 30, 36, 80);
    }
    inSection("lfo");

    // ================================================================= VOICE
    sectionAt("voice", "VOICE & AMP", 458, 416, 372, 290);
    add("voice.mode",      K::Selector, 468, 448, "MODE", 80);
    add("voice.priority",  K::Selector, 554, 448, "PRIORITY", 70);
    add("voice.alloc",     K::Selector, 630, 448, "ALLOCATE", 80);
    add("voice.velcurve",  K::Selector, 716, 448, "VEL CURVE", 76);
    add("voice.poly",      K::Knob,     468, 500, "VOICES");
    add("voice.glide",     K::Knob,     526, 500, "GLIDE");
    add("voice.glidemode", K::Selector, 584, 512, "GLIDE MODE", 76);
    add("voice.bend",      K::Knob,     668, 500, "BEND");
    add("amp.level",       K::Knob,     726, 500, "AMP");
    add("voice.unison",    K::Switch,   468, 590, "UNISON");
    add("voice.univoices", K::Knob,     522, 578, "STACK");
    add("voice.detune",    K::Knob,     580, 578, "DETUNE");
    add("voice.spread",    K::Knob,     638, 578, "SPREAD");
    add("voice.vintage",   K::Knob,     696, 578, "VINTAGE");
    add("amp.pan",         K::Knob,     754, 578, "PAN");
    utilityAt("voiceLeds", 468, 652, 352, 46);

    // ============================================================ MOD MATRIX
    sectionAt("mod", "MODULATION MATRIX", 838, 416, 440, 290);
    text("SOURCE", 868, 442, 140, 14, juce::Justification::centredLeft);
    text("DESTINATION", 1014, 442, 150, 14, juce::Justification::centredLeft);
    text("AMOUNT", 1170, 442, 104, 14);
    static juce::StringArray modIds;
    if (modIds.isEmpty())
        for (int m = 1; m <= 8; ++m)
            for (auto s : { "src", "dst", "amt" })
                modIds.add("mod" + juce::String(m) + "." + s);
    for (int m = 0; m < 8; ++m)
    {
        const int y = 460 + m * 30;
        text(juce::String(m + 1), 846, y + 4, 18, 16);
        add(modIds[m * 3].toRawUTF8(),     K::Selector, 868, y, "", 140, 24);
        add(modIds[m * 3 + 1].toRawUTF8(), K::Selector, 1014, y, "", 150, 24);
        add(modIds[m * 3 + 2].toRawUTF8(), K::HSlider,  1170, y, "");
    }

    // =========================================================== ARPEGGIATOR
    sectionAt("arp", "ARPEGGIATOR", 1286, 416, 236, 290);
    add("arp.on",    K::Switch,   1296, 448, "ARP ON");
    add("arp.hold",  K::Switch,   1350, 448, "HOLD");
    add("arp.mode",  K::Selector, 1404, 448, "MODE", 108);
    add("arp.div",   K::Knob,     1294, 500, "RATE");
    add("arp.oct",   K::Knob,     1350, 500, "OCTAVES");
    add("arp.gate",  K::Knob,     1406, 500, "GATE");
    add("arp.swing", K::Knob,     1462, 500, "SWING");
    utilityAt("arpDisplay", 1296, 580, 216, 116);

    // ================================================================ OUTPUT
    sectionAt("output", "OUTPUT", 1530, 416, 380, 290);
    add("out.macro",   K::LargeKnob, 1544, 448, "MACRO");
    add("out.limiter", K::Switch,    1556, 556, "LIMITER");
    utilityAt("outputInfo", 1630, 448, 196, 248);
    utilityAt("meter", 1836, 448, 64, 248);

    // ============================================================= SEQUENCER
    sectionAt("seq", "STEP SEQUENCER", 10, 714, 820, 198);
    add("seq.mode", K::Selector, 18, 746, "MODE", 78);
    add("seq.div",  K::Knob,     30, 794, "RATE");
    add("seq.len",  K::Knob,     104, 744, "LENGTH");
    add("seq.gate", K::Knob,     104, 818, "GATE");
    text("GATE", 160, 842, 36, 14, juce::Justification::centredLeft);
    text("ACC", 160, 858, 36, 14, juce::Justification::centredLeft);
    text("SLIDE", 160, 874, 36, 14, juce::Justification::centredLeft);
    utilityAt("seqLeds", 196, 740, 624, 12);
    static juce::StringArray seqIds;
    if (seqIds.isEmpty())
        for (int s = 1; s <= 16; ++s)
            for (auto k : { "val", "gate", "acc", "slide" })
                seqIds.add("seq.s" + juce::String(s) + "." + k);
    for (int s = 0; s < 16; ++s)
    {
        const int x = 196 + s * 39;
        add(seqIds[s * 4].toRawUTF8(),     K::StepSlider, x, 754, juce::String(s + 1));
        add(seqIds[s * 4 + 1].toRawUTF8(), K::StepToggle, x, 842, "");
        add(seqIds[s * 4 + 2].toRawUTF8(), K::StepToggle, x, 858, "");
        add(seqIds[s * 4 + 3].toRawUTF8(), K::StepToggle, x, 874, "");
    }

    // =============================================================== EFFECTS
    sectionAt("fx", "EFFECTS", 838, 714, 1072, 198);
    struct Module { const char* id; const char* title; };
    const Module modules[8] = { { "fxDrive", "DRIVE" }, { "fxChorus", "CHORUS" }, { "fxEns", "ENSEMBLE" }, { "fxPhaser", "PHASER" },
                                { "fxFlanger", "FLANGER" }, { "fxDelay", "DELAY" }, { "fxReverb", "REVERB" }, { "fxStereo", "STEREO" } };
    juce::Rectangle<int> mod[8];
    for (int i = 0; i < 8; ++i)
    {
        mod[i] = { 846 + (i % 4) * 266, 742 + (i / 4) * 84, 264, 80 };
        sectionAt(modules[i].id, modules[i].title, mod[i].getX(), mod[i].getY(), mod[i].getWidth(), mod[i].getHeight(), true);
    }
    auto knobAt = [&](int m, int k) { return juce::Point<int>(mod[m].getX() + 4 + k * 52, mod[m].getY() + 16); };
    auto onAt = [&](int m, int slot) { return juce::Point<int>(mod[m].getRight() - 40 - slot * 44, mod[m].getY() + 1); };

    inSection("fxDrive");
    add("fx.drive.on",   K::Switch, onAt(0, 0).x, onAt(0, 0).y, "ON", 38, 14);
    add("fx.drive.type", K::Selector, knobAt(0, 0).x, knobAt(0, 0).y + 12, "TYPE", 98);
    add("fx.drive.amt",  K::Knob, knobAt(0, 2).x, knobAt(0, 2).y, "DRIVE");
    add("fx.drive.tone", K::Knob, knobAt(0, 3).x, knobAt(0, 3).y, "TONE");
    add("fx.drive.mix",  K::Knob, knobAt(0, 4).x, knobAt(0, 4).y, "MIX");

    inSection("fxChorus");
    add("fx.cho.on",    K::Switch, onAt(1, 0).x, onAt(1, 0).y, "ON", 38, 14);
    add("fx.cho.rate",  K::Knob, knobAt(1, 0).x, knobAt(1, 0).y, "RATE");
    add("fx.cho.depth", K::Knob, knobAt(1, 1).x, knobAt(1, 1).y, "DEPTH");
    add("fx.cho.mix",   K::Knob, knobAt(1, 2).x, knobAt(1, 2).y, "MIX");

    inSection("fxEns");
    add("fx.ens.on",    K::Switch, onAt(2, 0).x, onAt(2, 0).y, "ON", 38, 14);
    add("fx.ens.rate",  K::Knob, knobAt(2, 0).x, knobAt(2, 0).y, "RATE");
    add("fx.ens.depth", K::Knob, knobAt(2, 1).x, knobAt(2, 1).y, "DEPTH");
    add("fx.ens.mix",   K::Knob, knobAt(2, 2).x, knobAt(2, 2).y, "MIX");

    inSection("fxPhaser");
    add("fx.pha.on",    K::Switch, onAt(3, 0).x, onAt(3, 0).y, "ON", 38, 14);
    add("fx.pha.rate",  K::Knob, knobAt(3, 0).x, knobAt(3, 0).y, "RATE");
    add("fx.pha.depth", K::Knob, knobAt(3, 1).x, knobAt(3, 1).y, "DEPTH");
    add("fx.pha.fb",    K::Knob, knobAt(3, 2).x, knobAt(3, 2).y, "FDBK");
    add("fx.pha.mix",   K::Knob, knobAt(3, 3).x, knobAt(3, 3).y, "MIX");

    inSection("fxFlanger");
    add("fx.fla.on",    K::Switch, onAt(4, 0).x, onAt(4, 0).y, "ON", 38, 14);
    add("fx.fla.rate",  K::Knob, knobAt(4, 0).x, knobAt(4, 0).y, "RATE");
    add("fx.fla.depth", K::Knob, knobAt(4, 1).x, knobAt(4, 1).y, "DEPTH");
    add("fx.fla.fb",    K::Knob, knobAt(4, 2).x, knobAt(4, 2).y, "FDBK");
    add("fx.fla.mix",   K::Knob, knobAt(4, 3).x, knobAt(4, 3).y, "MIX");

    inSection("fxDelay");
    add("fx.dly.on",   K::Switch, onAt(5, 0).x, onAt(5, 0).y, "ON", 38, 14);
    add("fx.dly.pp",   K::Switch, onAt(5, 1).x, onAt(5, 1).y, "P.PONG", 42, 14);
    add("fx.dly.sync", K::Switch, onAt(5, 2).x, onAt(5, 2).y, "SYNC", 38, 14);
    add("fx.dly.time", K::Knob, knobAt(5, 0).x, knobAt(5, 0).y, "TIME");
    add("fx.dly.div",  K::Knob, knobAt(5, 1).x, knobAt(5, 1).y, "DIVISION");
    add("fx.dly.fb",   K::Knob, knobAt(5, 2).x, knobAt(5, 2).y, "FDBK");
    add("fx.dly.tone", K::Knob, knobAt(5, 3).x, knobAt(5, 3).y, "TONE");
    add("fx.dly.mix",  K::Knob, knobAt(5, 4).x, knobAt(5, 4).y, "MIX");

    inSection("fxReverb");
    add("fx.rev.on",   K::Switch, onAt(6, 0).x, onAt(6, 0).y, "ON", 38, 14);
    add("fx.rev.size", K::Knob, knobAt(6, 0).x, knobAt(6, 0).y, "SIZE");
    add("fx.rev.damp", K::Knob, knobAt(6, 1).x, knobAt(6, 1).y, "DAMP");
    add("fx.rev.pre",  K::Knob, knobAt(6, 2).x, knobAt(6, 2).y, "PRE-DLY");
    add("fx.rev.mix",  K::Knob, knobAt(6, 3).x, knobAt(6, 3).y, "MIX");

    inSection("fxStereo");
    add("out.width",     K::Knob, knobAt(7, 0).x, knobAt(7, 0).y, "WIDTH");
    add("out.character", K::Knob, knobAt(7, 1).x, knobAt(7, 1).y, "CHAR");
    utilityAt("stereoScope", knobAt(7, 2).x + 4, mod[7].getY() + 18, 150, 58);

    // ============================================================== KEYBOARD
    sectionAt("wheels", "", 10, 920, 150, 270);
    utilityAt("pitchWheel", 26, 940, 48, 196);
    utilityAt("modWheel", 88, 940, 48, 196);
    utilityAt("kbdOctDown", 22, 1150, 56, 28);
    utilityAt("kbdOctUp", 84, 1150, 56, 28);
    sectionAt("keys", "", 168, 920, 1742, 270);
    utilityAt("keyboard", 172, 924, 1734, 262);
}

} // namespace zsynth::ui
