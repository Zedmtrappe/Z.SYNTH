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
        case ControlKind::Fader:      return { 34, 176 };
        case ControlKind::ShortFader: return { 32, 140 };
        case ControlKind::StepSlider: return { 44, 160 };
        case ControlKind::StepToggle: return { 44, 16 };
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

juce::Rectangle<int> PanelLayout::pageBounds(const juce::String& page) const
{
    if (page == kMainPage)
        return { 0, 0, kWidth, kHeight };
    for (const auto& p : pageList)
        if (p.id == page)
            return p.boundsOnCanvas.withZeroOrigin();
    return {};
}

PanelLayout::PanelLayout()
{
    juce::String currentSection, currentPage = kMainPage;

    // Parameter ids built at runtime need stable storage for the const char* in ControlSpec.
    static juce::StringArray idStore;
    auto stableId = [](const juce::String& s) -> const char* {
        const int i = idStore.indexOf(s);
        if (i >= 0)
            return idStore[i].toRawUTF8();
        idStore.add(s);
        return idStore[idStore.size() - 1].toRawUTF8();
    };

    auto page = [&](const juce::String& id) { currentPage = id; };
    auto sectionAt = [&](const juce::String& id, const juce::String& title, int x, int y, int w, int h, bool sub = false) {
        sectionList.push_back({ id, title, { x, y, w, h }, sub, currentPage });
        currentSection = id;
    };
    auto inSection = [&](const juce::String& id) { currentSection = id; };
    auto add = [&](const juce::String& id, ControlKind kind, int x, int y, const juce::String& label, int w = -1, int h = -1) {
        const auto size = controlSize(kind);
        controlList.push_back({ stableId(id), kind, { x, y, w > 0 ? w : size.x, h > 0 ? h : size.y }, label, currentSection, currentPage });
    };
    auto fader = [&](const juce::String& id, int x, int y, const juce::String& label) { add(id, ControlKind::Fader, x, y, label); };
    auto shortFader = [&](const juce::String& id, int x, int y, const juce::String& label) { add(id, ControlKind::ShortFader, x, y, label); };
    auto compact = [&](const juce::String& id, int x, int y, const juce::String& label, int w = 38) { add(id, ControlKind::Switch, x, y, label, w, 14); };
    auto utilityAt = [&](const juce::String& id, int x, int y, int w, int h) { utilityList.push_back({ id, { x, y, w, h }, currentPage }); };
    auto text = [&](const juce::String& t, int x, int y, int w, int h, juce::Justification j = juce::Justification::centred, bool heading = false) {
        labelList.push_back({ t, { x, y, w, h }, j, heading, currentPage });
    };
    auto heading = [&](const juce::String& t, int x, int y, int w) { text(t, x, y, w, 16, juce::Justification::centredLeft, true); };

    using K = ControlKind;

    // ======================================================================
    //                               MAIN PAGE
    // ======================================================================

    // ------------------------------------------------------------- header
    sectionAt("header", "", 0, 0, kWidth, 96);
    utilityAt("logo", 18, 8, 372, 80);
    utilityAt("presetPrev", 404, 30, 30, 36);
    utilityAt("presetDisplay", 438, 12, 400, 72);
    utilityAt("presetNext", 842, 30, 30, 36);
    utilityAt("presetBrowse", 884, 14, 82, 32);
    utilityAt("presetSave", 972, 14, 72, 32);
    utilityAt("presetSaveAs", 884, 52, 82, 32);
    utilityAt("presetInit", 972, 52, 72, 32);

    const std::pair<const char*, const char*> globals[] = { { "master.tune", "TUNE" }, { "master.transpose", "TRANSP" },
                                                            { "master.octave", "OCTAVE" }, { "master.tempo", "TEMPO" } };
    for (int i = 0; i < 4; ++i)
    {
        const int x = 1068 + i * 52;
        add(globals[i].first, K::SmallKnob, x, 18, globals[i].second);
        text(globals[i].second, x - 12, 52, 54, 14);
    }
    add("master.oversampling", K::Selector, 1280, 26, "QUALITY", 88);
    add("midi.channel",        K::Selector, 1374, 26, "MIDI CH", 70);
    utilityAt("midiLed", 1452, 26, 54, 40);
    utilityAt("audioLed", 1508, 26, 54, 40);
    utilityAt("voiceCount", 1570, 20, 100, 54);
    add("master.volume", K::Knob, 1680, 16, "MASTER");
    utilityAt("panic", 1744, 32, 84, 34);
    utilityAt("version", 1836, 32, 74, 34);

    // -------------------------------------------------------- oscillators
    sectionAt("osc", "OSCILLATORS", 10, 104, 980, 320);
    add("osc.engine", K::Segmented, 770, 107, "ENGINE", 210, 20);
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);
        const int x = 20 + i * 232;
        heading("OSC " + n, x, 134, 60);
        compact("osc" + n + ".on", x + 184, 135, "ON");
        if (i > 0)
            compact("osc" + n + ".sync", x + 140, 135, "SYNC", 40);
        add("osc" + n + ".wave", K::Selector, x, 154, "WAVE", 104);
        add("osc" + n + ".octave", K::Selector, x + 110, 154, "RANGE", 62);
        fader("osc" + n + ".semi",  x,       206, "SEMI");
        fader("osc" + n + ".fine",  x + 36,  206, "FINE");
        fader("osc" + n + ".pw",    x + 72,  206, "PW");
        fader("osc" + n + ".level", x + 108, 206, "LEVEL");
        fader("osc" + n + ".fm",    x + 144, 206, i == 0 ? "FM 3>1" : "FM 1>" + n);
        if (i > 0)
            fader("osc" + n + ".xmod", x + 180, 206, "RING");
    }
    heading("MIXER", 716, 134, 80);
    add("osc.sub.wave",   K::Selector, 716, 154, "SUB WAVE", 72);
    add("osc.noise.type", K::Selector, 794, 154, "NOISE", 64);
    fader("osc.sub.level",   716, 206, "SUB");
    fader("osc.noise.level", 752, 206, "NOISE");
    fader("osc.drift",       788, 206, "DRIFT");
    fader("mix.drive",       824, 206, "DRIVE");

    sectionAt("wt", "WAVETABLE", 870, 132, 114, 286, true);
    add("wt.table", K::Selector, 876, 152, "TABLE", 102);
    utilityAt("wtDisplay", 876, 198, 102, 44);
    shortFader("wt.pos",    884, 250, "POS");
    shortFader("wt.spread", 932, 250, "SPRD");

    // ------------------------------------------------------------- filter
    sectionAt("filter", "FILTER", 998, 104, 332, 320);
    add("flt.model",  K::Selector, 1006, 134, "MODEL", 92);
    add("flt.type",   K::Selector, 1102, 134, "TYPE", 66);
    add("flt.slope",  K::Selector, 1172, 134, "SLOPE", 74);
    add("flt.envinv", K::Switch,   1252, 134, "INVERT");
    utilityAt("filterCurve", 1006, 182, 316, 46);
    const std::pair<const char*, const char*> filterFaders[] = {
        { "flt.cutoff", "CUT" }, { "flt.res", "RESO" }, { "flt.env", "ENV" }, { "flt.key", "KEY" },
        { "flt.vel", "VELO" }, { "flt.lfo", "LFO 1" }, { "flt.drive", "DRIVE" }, { "flt.hpf", "HPF" } };
    for (int k = 0; k < 8; ++k)
        fader(filterFaders[k].first, 1008 + k * 36, 236, filterFaders[k].second);

    // ---------------------------------------------------------- envelopes
    sectionAt("env", "ENVELOPES", 1338, 104, 572, 320);
    const char* envNames[3] = { "1  AMP", "2  FILTER", "3  MOD" };
    const char* adsr[4] = { "A", "D", "S", "R" };
    for (int e = 0; e < 3; ++e)
    {
        const juce::String n(e + 1);
        const int x0 = 1346 + e * 188;
        heading(envNames[e], x0, 134, 120);
        utilityAt("env" + n + "Display", x0, 152, e == 2 ? 104 : 176, 48);
        const char* suffix[4] = { "a", "d", "s", "r" };
        for (int s = 0; s < 4; ++s)
            fader("env" + n + "." + suffix[s], x0 + s * 36, 208, adsr[s]);
        fader(e == 2 ? "env3.amt" : "env" + n + ".vel", x0 + 144, 208, e == 2 ? "AMT" : "VELO");
    }
    add("env3.dest", K::Selector, 1722 + 108, 152, "DEST", 72);

    // --------------------------------------------------------------- LFOs
    sectionAt("lfo", "LFOs", 10, 432, 510, 320);
    utilityAt("openMatrix", 384, 435, 128, 20);
    for (int l = 0; l < 2; ++l)
    {
        const juce::String n(l + 1);
        const int x = 18 + l * 250;
        sectionAt("lfo" + n, "", x, 460, 244, 286, true);
        heading("LFO " + n, x + 6, 464, 50);
        compact("lfo" + n + ".sync", x + 132, 465, "SYNC", 40);
        compact("lfo" + n + ".uni", x + 176, 465, "UNIPOLAR", 62);
        add("lfo" + n + ".wave", K::Selector, x + 6, 486, "WAVE", 90);
        add("lfo" + n + ".mode", K::Selector, x + 100, 486, "TRIGGER", 66);
        add("lfo" + n + ".dest", K::Selector, x + 170, 486, "DEST", 70);
        fader("lfo" + n + ".rate",  x + 6,   540, "RATE");
        fader("lfo" + n + ".div",   x + 42,  540, "DIV");
        fader("lfo" + n + ".phase", x + 78,  540, "PHASE");
        fader("lfo" + n + ".delay", x + 114, 540, "DELAY");
        fader("lfo" + n + ".amt",   x + 150, 540, "AMT");
        utilityAt("lfo" + n + "Display", x + 192, 540, 46, 120);
    }
    inSection("lfo");

    // -------------------------------------------------------------- voice
    sectionAt("voice", "VOICE & AMP", 528, 432, 372, 320);
    add("voice.mode",      K::Selector, 536, 464, "MODE", 80);
    add("voice.priority",  K::Selector, 620, 464, "PRIORITY", 70);
    add("voice.alloc",     K::Selector, 694, 464, "ALLOCATE", 82);
    add("voice.velcurve",  K::Selector, 780, 464, "VEL CURVE", 76);
    add("voice.glidemode", K::Selector, 536, 512, "GLIDE MODE", 80);
    add("voice.unison",    K::Switch,   620, 512, "UNISON");
    utilityAt("voiceLeds", 676, 512, 216, 40);
    const std::pair<const char*, const char*> voiceFaders[] = {
        { "voice.poly", "POLY" }, { "voice.glide", "GLIDE" }, { "voice.bend", "BEND" }, { "amp.level", "AMP" },
        { "amp.pan", "PAN" }, { "voice.univoices", "STACK" }, { "voice.detune", "DTUNE" }, { "voice.spread", "WIDE" },
        { "voice.vintage", "VINT" } };
    for (int k = 0; k < 9; ++k)
        fader(voiceFaders[k].first, 536 + k * 36, 562, voiceFaders[k].second);

    // ----------------------------------------------------------------- FM
    sectionAt("fm", "FM ENGINE", 908, 432, 272, 320);
    utilityAt("openFm", 1060, 435, 116, 20);
    add("fm.algo", K::Selector, 916, 464, "ALGORITHM", 256);
    utilityAt("fmAlgoDisplay", 916, 512, 108, 226);
    add("fm.dx", K::Switch, 1032, 506, "DX MODE", 136, 38);
    fader("fm.depth",    1032, 548, "DEPTH");
    fader("fm.feedback", 1068, 548, "FDBK");
    fader("fm.level",    1104, 548, "LEVEL");
    fader("fm.vel",      1140, 548, "VELO");

    // -------------------------------------------------------- performance
    sectionAt("perf", "ARP & SEQUENCER", 1188, 432, 342, 320);
    utilityAt("openSeq", 1416, 435, 110, 20);
    heading("ARPEGGIATOR", 1196, 460, 150);
    heading("SEQUENCER", 1376, 460, 140);
    add("arp.on",   K::Switch,   1196, 480, "ARP ON");
    add("arp.hold", K::Switch,   1248, 480, "HOLD");
    add("arp.mode", K::Selector, 1196, 524, "MODE", 142);
    add("seq.mode", K::Selector, 1376, 480, "MODE", 146);
    fader("arp.div",   1196, 570, "RATE");
    fader("arp.oct",   1232, 570, "OCT");
    fader("arp.gate",  1268, 570, "GATE");
    fader("arp.swing", 1304, 570, "SWING");
    fader("seq.div",   1376, 570, "RATE");
    fader("seq.len",   1412, 570, "LEN");
    fader("seq.gate",  1448, 570, "GATE");

    // ------------------------------------------------------------- output
    sectionAt("output", "OUTPUT", 1538, 432, 372, 320);
    compact("out.limiter", 1546, 462, "LIMITER", 106);
    utilityAt("outputInfo", 1546, 482, 106, 74);
    fader("out.macro",     1546, 562, "MACRO");
    fader("out.width",     1582, 562, "WIDTH");
    fader("out.character", 1618, 562, "CHAR");
    utilityAt("meter", 1842, 464, 60, 282);

    // The valve stage: a window onto the valve plus DRIVE; the rest is on the "valve" page.
    sectionAt("valve", "VALVE", 1658, 456, 178, 290, true);
    compact("out.valve.on", 1792, 457, "ON");
    utilityAt("valveDisplay", 1664, 480, 166, 168);
    add("out.valve.drive", K::LargeKnob, 1666, 652, "DRIVE");
    utilityAt("openValve", 1746, 684, 80, 26);

    // ------------------------------------------------------------ effects
    sectionAt("fx", "EFFECTS", 10, 760, 1900, 204);
    struct Module { const char* id; const char* title; };
    const Module modules[7] = { { "fxDrive", "DRIVE" }, { "fxChorus", "CHORUS" }, { "fxEns", "ENSEMBLE" }, { "fxPhaser", "PHASER" },
                                { "fxFlanger", "FLANGER" }, { "fxDelay", "DELAY" }, { "fxReverb", "REVERB" } };
    int mx[7];
    for (int i = 0; i < 7; ++i)
    {
        mx[i] = 18 + i * 270;
        sectionAt(modules[i].id, modules[i].title, mx[i], 788, 264, 170, true);
    }
    auto fxFader = [&](int m, int k, const char* id, const char* label) { shortFader(id, mx[m] + 8 + k * 36, 810, label); };
    auto fxOn = [&](int m, const char* id) { compact(id, mx[m] + 222, 789, "ON"); };

    inSection("fxDrive");
    fxOn(0, "fx.drive.on");
    add("fx.drive.type", K::Selector, mx[0] + 8, 812, "TYPE", 100);
    shortFader("fx.drive.amt",  mx[0] + 120, 810, "DRIVE");
    shortFader("fx.drive.tone", mx[0] + 156, 810, "TONE");
    shortFader("fx.drive.mix",  mx[0] + 192, 810, "MIX");

    inSection("fxChorus");
    fxOn(1, "fx.cho.on");
    fxFader(1, 0, "fx.cho.rate", "RATE");
    fxFader(1, 1, "fx.cho.depth", "DEPTH");
    fxFader(1, 2, "fx.cho.mix", "MIX");

    inSection("fxEns");
    fxOn(2, "fx.ens.on");
    fxFader(2, 0, "fx.ens.rate", "RATE");
    fxFader(2, 1, "fx.ens.depth", "DEPTH");
    fxFader(2, 2, "fx.ens.mix", "MIX");

    inSection("fxPhaser");
    fxOn(3, "fx.pha.on");
    fxFader(3, 0, "fx.pha.rate", "RATE");
    fxFader(3, 1, "fx.pha.depth", "DEPTH");
    fxFader(3, 2, "fx.pha.fb", "FDBK");
    fxFader(3, 3, "fx.pha.mix", "MIX");

    inSection("fxFlanger");
    fxOn(4, "fx.fla.on");
    fxFader(4, 0, "fx.fla.rate", "RATE");
    fxFader(4, 1, "fx.fla.depth", "DEPTH");
    fxFader(4, 2, "fx.fla.fb", "FDBK");
    fxFader(4, 3, "fx.fla.mix", "MIX");

    inSection("fxDelay");
    fxOn(5, "fx.dly.on");
    compact("fx.dly.pp", mx[5] + 176, 789, "P.PONG", 42);
    compact("fx.dly.sync", mx[5] + 134, 789, "SYNC", 38);
    fxFader(5, 0, "fx.dly.time", "TIME");
    fxFader(5, 1, "fx.dly.div", "DIV");
    fxFader(5, 2, "fx.dly.fb", "FDBK");
    fxFader(5, 3, "fx.dly.tone", "TONE");
    fxFader(5, 4, "fx.dly.mix", "MIX");
    add("fx.dly.type", K::Selector, mx[5] + 192, 812, "TYPE", 66);
    utilityAt("openDelay", mx[5] + 192, 862, 66, 24);

    inSection("fxReverb");
    fxOn(6, "fx.rev.on");
    fxFader(6, 0, "fx.rev.size", "SIZE");
    fxFader(6, 1, "fx.rev.damp", "DAMP");
    fxFader(6, 2, "fx.rev.pre", "PRE");
    fxFader(6, 3, "fx.rev.mix", "MIX");
    add("fx.rev.type", K::Selector, mx[6] + 158, 812, "TYPE", 100);
    utilityAt("openReverb", mx[6] + 158, 862, 100, 24);

    // ----------------------------------------------------------- keyboard
    sectionAt("wheels", "", 10, 972, 150, 218);
    utilityAt("pitchWheel", 26, 986, 48, 160);
    utilityAt("modWheel", 88, 986, 48, 160);
    utilityAt("kbdOctDown", 22, 1154, 56, 28);
    utilityAt("kbdOctUp", 84, 1154, 56, 28);
    sectionAt("keys", "", 168, 972, 1742, 218);
    utilityAt("keyboard", 172, 976, 1734, 210);

    // ======================================================================
    //                              POPUP PAGES
    // ======================================================================

    // ------------------------------------------------- modulation matrix
    pageList.push_back({ "matrix", "MODULATION MATRIX", { 420, 180, 1080, 520 } });
    page("matrix");
    sectionAt("matrixPage", "MODULATION MATRIX", 0, 0, 1080, 520);
    utilityAt("matrixClose", 1040, 3, 34, 20);
    text("SOURCE", 60, 40, 300, 16, juce::Justification::centredLeft, true);
    text("DESTINATION", 380, 40, 320, 16, juce::Justification::centredLeft, true);
    text("AMOUNT", 720, 40, 330, 16, juce::Justification::centred, true);
    for (int m = 0; m < 8; ++m)
    {
        const juce::String n(m + 1);
        const int y = 66 + m * 54;
        text(n, 16, y + 6, 30, 20, juce::Justification::centred, true);
        add("mod" + n + ".src", K::Selector, 60, y, "", 300, 32);
        add("mod" + n + ".dst", K::Selector, 380, y, "", 320, 32);
        add("mod" + n + ".amt", K::HSlider, 720, y, "", 330, 32);
    }

    // --------------------------------------------------------- FM operators
    pageList.push_back({ "fm", "FM OPERATORS", { 60, 250, 1800, 470 } });
    page("fm");
    sectionAt("fmPage", "FM OPERATORS", 0, 0, 1800, 470);
    utilityAt("fmClose", 1760, 3, 34, 20);
    const char* opCols[7] = { "RATIO", "FINE", "LEVEL", "A", "D", "S", "R" };
    const char* opSuffix[7] = { "ratio", "fine", "level", "a", "d", "s", "r" };
    const char* scaleCols[3] = { "VEL", "K.RATE", "K.LVL" };
    const char* scaleSuffix[3] = { "vel", "krs", "kls" };
    for (int op = 0; op < 6; ++op)
    {
        const juce::String n(op + 1);
        const int bx = 20 + op * 296;
        sectionAt("op" + n, "OPERATOR " + n, bx, 40, 288, 410, true);
        for (int c = 0; c < 7; ++c)
            fader("fm.op" + n + "." + opSuffix[c], bx + 10 + c * 38, 70, opCols[c]);
        utilityAt("op" + n + "EnvDisplay", bx + 10, 264, 160, 170);
        for (int c = 0; c < 3; ++c)
            shortFader("fm.op" + n + "." + scaleSuffix[c], bx + 178 + c * 36, 280, scaleCols[c]);
    }

    // ------------------------------------------------------- valve advanced
    pageList.push_back({ "valve", "VALVE STAGE", { 1080, 300, 560, 330 } });
    page("valve");
    sectionAt("valvePage", "VALVE STAGE  -  ADVANCED", 0, 0, 560, 330);
    utilityAt("valveClose", 520, 3, 34, 20);
    add("out.valve.type", K::Selector, 20, 48, "VALVE TYPE", 220);
    const std::pair<const char*, const char*> valveFaders[4] = { { "out.valve.bias", "BIAS" }, { "out.valve.sag", "SAG" },
        { "out.valve.warmth", "WARMTH" }, { "out.valve.mix", "MIX" } };
    for (int k = 0; k < 4; ++k)
        fader(valveFaders[k].first, 24 + k * 54, 122, valveFaders[k].second);
    heading("VALVE TYPES", 270, 48, 270);
    text("12AX7  High-gain preamp triode: even harmonics, sings when pushed.", 270, 70, 276, 34, juce::Justification::topLeft);
    text("12AU7  Low-gain triode: gentle, clean warmth with lots of headroom.", 270, 108, 276, 34, juce::Justification::topLeft);
    text("EL84   Push-pull pair: odd harmonics, compresses and breaks up early.", 270, 146, 276, 34, juce::Justification::topLeft);
    text("6L6    Big power pair: smooth, solid low end, the most headroom.", 270, 184, 276, 34, juce::Justification::topLeft);
    heading("CONTROLS", 270, 226, 270);
    text("BIAS cold = cleaner/symmetric, hot = richer/grittier.  SAG = supply droop that blooms on loud notes.  WARMTH = output transformer (softer top, fuller bass).",
         270, 246, 276, 70, juce::Justification::topLeft);

    // ------------------------------------------------------- delay advanced
    pageList.push_back({ "delay", "DELAY", { 820, 340, 660, 330 } });
    page("delay");
    sectionAt("delayPage", "DELAY  -  ADVANCED", 0, 0, 660, 330);
    utilityAt("delayClose", 620, 3, 34, 20);
    utilityAt("delayPresets", 20, 44, 250, 26);
    const std::pair<const char*, const char*> delayFaders[5] = { { "fx.dly.wow", "WOW" }, { "fx.dly.flutter", "FLUTTER" },
        { "fx.dly.age", "AGE" }, { "fx.dly.spread", "SPREAD" }, { "fx.dly.duck", "DUCK" } };
    for (int k = 0; k < 5; ++k)
        fader(delayFaders[k].first, 24 + k * 50, 100, delayFaders[k].second);
    heading("DELAY TYPES (TYPE on the panel)", 300, 44, 340);
    const char* delayTypes[6] = {
        "DIGITAL   Clean repeats; TONE shapes the feedback.",
        "RE-201    Space Echo tape: saturation, wow & flutter, hiss.",
        "EP-3      Echoplex tape: warmer, darker, more wow and drive.",
        "DM-2      Bucket-brigade: dark, soft, chorusing clock wobble.",
        "DIFFUSE   Each repeat smears into the next.",
        "LO-FI     Sample-rate and bit crush, telephone band." };
    for (int k = 0; k < 6; ++k)
        text(delayTypes[k], 300, 68 + k * 26, 350, 22, juce::Justification::centredLeft);
    text("WOW / FLUTTER / AGE: 0.5 = the type as voiced.  SPREAD widens the stereo image.  DUCK lowers the echoes while you play.",
         300, 232, 350, 60, juce::Justification::topLeft);

    // ------------------------------------------------------ reverb advanced
    pageList.push_back({ "reverb", "REVERB", { 780, 300, 760, 370 } });
    page("reverb");
    sectionAt("reverbPage", "REVERB  -  ADVANCED", 0, 0, 760, 370);
    utilityAt("reverbClose", 720, 3, 34, 20);
    utilityAt("reverbPresets", 20, 44, 300, 26);
    const std::pair<const char*, const char*> reverbFaders[6] = { { "fx.rev.decay", "DECAY" }, { "fx.rev.lowcut", "LO CUT" },
        { "fx.rev.diff", "DIFF" }, { "fx.rev.mod", "MOD" }, { "fx.rev.early", "E / L" }, { "fx.rev.width", "WIDTH" } };
    for (int k = 0; k < 6; ++k)
        fader(reverbFaders[k].first, 24 + k * 50, 100, reverbFaders[k].second);
    heading("REVERB TYPES (TYPE on the panel)", 340, 44, 400);
    const char* reverbTypes[9] = {
        "HALL          The original ZYNTH hall.",
        "ROOM          Small space, strong early reflections.",
        "EMT 140       Dense, bright plate; instant build-up.",
        "CHAMBER       Darker echo chamber with clear reflections.",
        "SPRING        Dispersive spring tank: the classic boing.",
        "LEXICON 224   Grainy, chorused 80s digital hall.",
        "LEXICON 480L  Larger, smoother, richly modulated.",
        "BRICASTI M7   Dense, natural, realistic reflections.",
        "SHIMMER       Hall with an octave-up feedback loop." };
    for (int k = 0; k < 9; ++k)
        text(reverbTypes[k], 340, 68 + k * 26, 410, 22, juce::Justification::centredLeft);
    text("DECAY trims SIZE's decay time.  DIFF = echo density.  MOD = tail chorusing.  E / L = early reflections vs tail.",
         340, 308, 410, 48, juce::Justification::topLeft);

    // ------------------------------------------------------- sequencer steps
    pageList.push_back({ "seq", "STEP SEQUENCER", { 400, 300, 1120, 340 } });
    page("seq");
    sectionAt("seqPage", "STEP SEQUENCER", 0, 0, 1120, 340);
    utilityAt("seqClose", 1080, 3, 34, 20);
    utilityAt("seqLeds", 80, 40, 16 * 62, 12);
    text("VALUE", 10, 120, 64, 16, juce::Justification::centredLeft, true);
    text("GATE", 10, 232, 64, 16, juce::Justification::centredLeft, true);
    text("ACCENT", 10, 254, 64, 16, juce::Justification::centredLeft, true);
    text("SLIDE", 10, 276, 64, 16, juce::Justification::centredLeft, true);
    text("Mod mode: step values drive the 'Seq Step' matrix source.  Note mode: hold a key; values transpose +/-12 semitones.",
         80, 304, 1000, 18, juce::Justification::centredLeft);
    for (int s = 0; s < 16; ++s)
    {
        const juce::String n(s + 1);
        const int x = 80 + s * 62;
        add("seq.s" + n + ".val", K::StepSlider, x, 58, n);
        add("seq.s" + n + ".gate", K::StepToggle, x, 232, "");
        add("seq.s" + n + ".acc", K::StepToggle, x, 254, "");
        add("seq.s" + n + ".slide", K::StepToggle, x, 276, "");
    }
}

} // namespace zsynth::ui
