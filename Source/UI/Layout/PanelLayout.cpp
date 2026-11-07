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
    juce::String currentSection, currentPage = kMainPage, currentView;

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
    auto view = [&](const juce::String& id) { currentView = id; };
    auto sectionAt = [&](const juce::String& id, const juce::String& title, int x, int y, int w, int h, bool sub = false) {
        sectionList.push_back({ id, title, { x, y, w, h }, sub, currentPage });
        currentSection = id;
    };
    auto inSection = [&](const juce::String& id) { currentSection = id; };
    auto add = [&](const juce::String& id, ControlKind kind, int x, int y, const juce::String& label, int w = -1, int h = -1) {
        const auto size = controlSize(kind);
        controlList.push_back({ stableId(id), kind, { x, y, w > 0 ? w : size.x, h > 0 ? h : size.y }, label, currentSection, currentPage,
                                currentView });
    };
    auto fader = [&](const juce::String& id, int x, int y, const juce::String& label) { add(id, ControlKind::Fader, x, y, label); };
    auto shortFader = [&](const juce::String& id, int x, int y, const juce::String& label) { add(id, ControlKind::ShortFader, x, y, label); };
    auto compact = [&](const juce::String& id, int x, int y, const juce::String& label, int w = 38) { add(id, ControlKind::Switch, x, y, label, w, 14); };
    auto utilityAt = [&](const juce::String& id, int x, int y, int w, int h) { utilityList.push_back({ id, { x, y, w, h }, currentPage }); };
    auto text = [&](const juce::String& t, int x, int y, int w, int h, juce::Justification j = juce::Justification::centred, bool heading = false) {
        labelList.push_back({ t, { x, y, w, h }, j, heading, currentPage, currentView });
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
    utilityAt("panic", 1744, 6, 84, 26);
    utilityAt("musicalTyping", 1836, 6, 74, 26); // in line with PANIC
    // PITCH TRACK: play from a guitar / bass on the audio input; the arrow reopens its window.
    add("trk.on", K::Switch, 1744, 36, "PITCH TRACK", 138, 20);
    utilityAt("openTrack", 1886, 36, 24, 20);
    // HARMONY: in-key harmony notes; the arrow opens its window.
    add("harm.on", K::Switch, 1744, 60, "HARMONY", 138, 20);
    utilityAt("openHarm", 1886, 60, 24, 20);
    utilityAt("version", 1836, 82, 74, 13);

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
        fader("osc" + n + ".level", x + 108, 206, "LEVEL");
        fader("osc" + n + ".fm",    x + 144, 206, i == 0 ? "FM 3>1" : "FM 1>" + n);
        // Slot 1: PW, or DETUNE (SuperSaw / HyperSaw), or the oscillator's WT POS (Wavetable / ROM waves).
        view("osc" + n + ":std");
        fader("osc" + n + ".pw",    x + 72,  206, "PW");
        view("osc" + n + ":super");
        fader("osc" + n + ".detune", x + 72, 206, "DETUNE");
        view("osc" + n + ":wt");
        fader("osc" + n + ".wt.pos", x + 72, 206, "WT POS");
        // Slot 2: RING, or MIX (SuperSaw / HyperSaw).
        if (i > 0)
        {
            view("osc" + n + "r:ring");
            fader("osc" + n + ".xmod", x + 180, 206, "RING");
        }
        view("osc" + n + "r:mix");
        fader("osc" + n + ".smix",   x + 180, 206, "MIX");
        view({});
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
    add("wt.pos",    K::ShortFader, 884, 246, "POS", -1, 124);
    add("wt.spread", K::ShortFader, 932, 246, "SPRD", -1, 124);
    utilityAt("openWtEngine", 876, 372, 102, 20);
    utilityAt("openWave", 876, 396, 102, 18);

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
    add("voice.glidemode", K::Selector, 536, 512, "GLIDE MODE", 76);
    add("voice.glidetype", K::Selector, 616, 512, "GLIDE TYPE", 104);
    add("voice.unison",    K::Switch,   724, 512, "UNISON", 44);
    utilityAt("voiceLeds", 772, 512, 120, 40);
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
    // OUTPUT is one L-shaped plate: this column (valve, transformer, output) and, below
    // it, the effects row from the EQ to the compressor ("outputRow", drawn with it).
    sectionAt("output", "OUTPUT", 1538, 432, 372, 328);
    compact("out.limiter", 1546, 462, "LIMITER", 106);
    utilityAt("outputInfo", 1546, 482, 106, 74);
    fader("out.macro",     1546, 562, "MACRO");
    fader("out.width",     1582, 562, "WIDTH");
    fader("out.character", 1618, 562, "CHAR");
    utilityAt("meter", 1842, 464, 60, 282);

    // The valve stage: a window onto the valve plus DRIVE; the rest is on the "valve" page.
    sectionAt("valve", "VALVE", 1658, 456, 178, 194, true);
    compact("out.valve.on", 1792, 457, "ON");
    utilityAt("valveDisplay", 1664, 476, 112, 170);
    add("out.valve.drive", K::Knob, 1780, 482, "DRIVE");
    utilityAt("openValve", 1778, 590, 54, 40);

    // Output transformer, straight after the valve: ON, TYPE and AMOUNT.
    sectionAt("xfmr", "TRANSFORMER", 1658, 654, 178, 92, true);
    compact("out.xfmr.on", 1792, 655, "ON");
    compact("out.xfmr.hot", 1744, 655, "+HOT", 44);
    add("out.xfmr.type", K::Selector, 1664, 682, "TYPE", 112);
    add("out.xfmr.amt", K::Knob, 1780, 676, "AMOUNT", 52);

    // ------------------------------------------------------------ effects
    // EFFECTS: drive and modulation; the rest of the row belongs to OUTPUT.
    sectionAt("fx", "EFFECTS", 10, 760, 490, 204);
    sectionAt("outputRow", "", 508, 760, 1402, 204);
    // Left to right: drive, modulation, graphic EQ, SMOOTH, delay, reverb, tape,
    // compressor. (The EQ runs before delay and reverb unless POST FX is on; the
    // compressor's PRE FX moves it before them.)
    struct Module { const char* id; const char* title; int width; };
    enum { mDrive, mMod, mEq, mSmooth, mDelay, mReverb, mTape, mComp };
    const Module modules[8] = { { "fxDrive", "DRIVE", 216 }, { "fxMod", "MODULATION", 252 }, { "peqModule", "EQ", 0 },
                                { "smoothModule", "SMOOTH", 104 }, { "fxDelay", "DELAY", 256 }, { "fxReverb", "REVERB", 264 },
                                { "tapeModule", "TAPE", 140 }, { "compModule", "OUTPUT COMP", 390 } };
    int fixedWidth = 0;
    for (const auto& m : modules)
        fixedWidth += m.width;
    constexpr int plateGap = 18; // between the EFFECTS and OUTPUT plates
    const int peqWidth = 1884 - 7 * 6 - plateGap - fixedWidth; // the EQ takes what is left
    int mx[8], mw[8];
    for (int i = 0, x = 18; i < 8; ++i)
    {
        if (i == mEq)
            x += plateGap;
        mx[i] = x;
        mw[i] = modules[i].width > 0 ? modules[i].width : peqWidth;
        sectionAt(modules[i].id, modules[i].title, mx[i], 788, mw[i], 170, true);
        x += mw[i] + 6;
    }
    auto fxFader = [&](int m, int k, const char* id, const char* label) { shortFader(id, mx[m] + 8 + k * 36, 810, label); };
    auto fxOn = [&](int m, const char* id) { compact(id, mx[m] + mw[m] - 42, 789, "ON"); };

    inSection("fxDrive");
    fxOn(0, "fx.drive.on");
    add("fx.drive.type", K::Selector, mx[0] + 8, 812, "TYPE", 90);
    shortFader("fx.drive.amt",  mx[0] + 104, 810, "DRIVE");
    shortFader("fx.drive.tone", mx[0] + 140, 810, "TONE");
    shortFader("fx.drive.mix",  mx[0] + 176, 810, "MIX");

    // Modulation: one block, the EFFECT selector chooses which effect's controls
    // are shown. Each effect keeps its own ON, so several can run at once.
    inSection("fxMod");
    utilityAt("modSelector", mx[1] + 8, 812, 90, 40);
    struct ModFx { const char* view; const char* on; std::vector<std::pair<const char*, const char*>> faders; };
    const ModFx modFx[4] = {
        { "mod:chorus",   "fx.cho.on", { { "fx.cho.rate", "RATE" }, { "fx.cho.depth", "DEPTH" }, { "fx.cho.mix", "MIX" } } },
        { "mod:ensemble", "fx.ens.on", { { "fx.ens.rate", "RATE" }, { "fx.ens.depth", "DEPTH" }, { "fx.ens.mix", "MIX" } } },
        { "mod:phaser",   "fx.pha.on", { { "fx.pha.rate", "RATE" }, { "fx.pha.depth", "DEPTH" }, { "fx.pha.fb", "FDBK" }, { "fx.pha.mix", "MIX" } } },
        { "mod:flanger",  "fx.fla.on", { { "fx.fla.rate", "RATE" }, { "fx.fla.depth", "DEPTH" }, { "fx.fla.fb", "FDBK" }, { "fx.fla.mix", "MIX" } } } };
    for (const auto& fx : modFx)
    {
        view(fx.view);
        fxOn(1, fx.on);
        if (juce::String(fx.view) == "mod:chorus")
            add("fx.cho.type", K::Selector, mx[1] + 8, 872, "TYPE", 90);
        for (size_t k = 0; k < fx.faders.size(); ++k)
            shortFader(fx.faders[k].first, mx[1] + 104 + (int) k * 36, 810, fx.faders[k].second);
    }
    view({});

    inSection("fxDelay");
    fxOn(mDelay, "fx.dly.on");
    compact("fx.dly.pp", mx[mDelay] + mw[mDelay] - 88, 789, "P.PONG", 42);
    compact("fx.dly.sync", mx[mDelay] + mw[mDelay] - 130, 789, "SYNC", 38);
    fxFader(mDelay, 0, "fx.dly.time", "TIME");
    fxFader(mDelay, 1, "fx.dly.div", "DIV");
    fxFader(mDelay, 2, "fx.dly.fb", "FDBK");
    fxFader(mDelay, 3, "fx.dly.tone", "TONE");
    fxFader(mDelay, 4, "fx.dly.mix", "MIX");
    add("fx.dly.type", K::Selector, mx[mDelay] + 188, 812, "TYPE", 62);
    utilityAt("openDelay", mx[mDelay] + 188, 862, 62, 24);

    inSection("fxReverb");
    fxOn(mReverb, "fx.rev.on");
    fxFader(mReverb, 0, "fx.rev.size", "SIZE");
    fxFader(mReverb, 1, "fx.rev.damp", "DAMP");
    fxFader(mReverb, 2, "fx.rev.pre", "PRE");
    fxFader(mReverb, 3, "fx.rev.mix", "MIX");
    add("fx.rev.type", K::Selector, mx[mReverb] + 158, 812, "TYPE", 100);
    utilityAt("openReverb", mx[mReverb] + 158, 862, 100, 24);
    utilityAt("reverbEqCurve", mx[mReverb] + 158, 892, 100, 58);

    // Output EQ: the model, the curve at a glance; ADVANCED opens the model's front panel.
    inSection("peqModule");
    compact("peq.on", mx[mEq] + mw[mEq] - 42, 789, "ON");
    compact("peq.pre", mx[mEq] + mw[mEq] - 100, 789, "POST FX", 54);
    add("eq.model", K::Selector, mx[mEq] + 8, 808, "", mw[mEq] - 16, 24);
    utilityAt("peqCurve", mx[mEq] + 8, 838, mw[mEq] - 16, 84);
    utilityAt("openPeq", mx[mEq] + 8, 926, mw[mEq] - 16, 24);

    // SMOOTH: fixed 2.2 kHz dynamic compressor; AMOUNT and its GR ladder.
    inSection("smoothModule");
    fxOn(mSmooth, "out.smooth.on");
    add("out.smooth", K::Knob, mx[mSmooth] + 6, 840, "AMOUNT");
    utilityAt("smoothMeter", mx[mSmooth] + 64, 808, 36, 144);

    // Tape: the machine, how hard it is hit and how worn the transport is.
    inSection("tapeModule");
    fxOn(6, "tape.on");
    add("tape.model", K::Selector, mx[6] + 8, 812, "MACHINE", mw[6] - 16);
    add("tape.drive", K::Knob, mx[6] + 8, 858, "DRIVE");
    add("tape.wow", K::Knob, mx[6] + mw[6] - 60, 858, "WOW/FLUT");
    utilityAt("openTape", mx[6] + 8, 926, mw[6] - 16, 24);

    // Output compressor: model, the gain-reduction VU and the everyday controls.
    const int cx = mx[7], cw = mw[7];
    inSection("compModule");
    compact("comp.on", cx + cw - 42, 789, "ON");
    compact("comp.pre", cx + cw - 96, 789, "PRE FX", 50);
    utilityAt("compMeter", cx + 8, 808, 152, 144);
    // Knobs, as on the hardware (their style follows the MODEL); output gain last.
    add("comp.thresh", K::LargeKnob, cx + 180, 806, "THRESH", 72, 86);
    add("comp.mix",    K::Knob, cx + 162, 892, "MIX");
    add("comp.makeup", K::Knob, cx + 218, 892, "MAKEUP");
    add("comp.model", K::Selector, cx + 278, 812, "MODEL", cw - 286);
    utilityAt("openComp", cx + 278, 862, cw - 286, 24);

    // ----------------------------------------------------------- keyboard
    sectionAt("wheels", "", 10, 972, 150, 218);
    utilityAt("pitchWheel", 26, 986, 48, 160);
    utilityAt("modWheel", 88, 986, 48, 160);
    utilityAt("kbdOctDown", 22, 1154, 56, 28);
    utilityAt("kbdOctUp", 84, 1154, 56, 28);
    sectionAt("keys", "", 168, 972, 1742, 218);
    utilityAt("auditionBar", 176, 976, 1240, 30);
    // KEYBOARD: SINGLE / SPLIT / LAYER, which part is on the panel, and the SPLIT window.
    add("split.mode", K::Segmented, 1426, 980, "KEYBOARD", 186, 22);
    utilityAt("partLower", 1620, 978, 92, 26); // LOWER left, UPPER right, as on the keyboard
    utilityAt("partUpper", 1716, 978, 92, 26);
    utilityAt("openSplit", 1814, 978, 88, 26);
    utilityAt("keyboard", 172, 1010, 1734, 176);

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

    // ------------------------------------------- wave sequence and vector
    pageList.push_back({ "wave", "WAVE SEQUENCE & VECTOR", { 560, 140, 740, 340 } });
    page("wave");
    sectionAt("wavePage", "WAVE SEQUENCE  &  VECTOR", 0, 0, 740, 340);
    utilityAt("waveClose", 700, 3, 34, 20);
    heading("WAVE SEQUENCE", 20, 34, 300);
    add("wt.seq.mode", K::Selector, 20, 56, "MODE", 120);
    add("wt.seq.div",  K::Selector, 150, 56, "DIVISION", 84);
    add("wt.seq.sync", K::Switch,   244, 56, "SYNC");
    fader("wt.seq.steps", 24, 116, "STEPS");
    fader("wt.seq.time",  70, 116, "TIME");
    fader("wt.seq.xfade", 116, 116, "XFADE");
    text("Steps through the WAVETABLE section's table from every note-on, like a Wavestation wave sequence.  "
         "The waves are spread evenly through the table starting at POS (SPRD offsets osc 2 and 3).  "
         "XFADE 0 = hard, rhythmic steps; 1 = continuous blending.  "
         "The SEQ tables hold 32 different waves made for sequencing.",
         164, 116, 176, 200, juce::Justification::topLeft);
    heading("VECTOR", 360, 34, 200);
    add("vec.on", K::Switch, 650, 56, "ON");
    utilityAt("vectorPad", 370, 60, 220, 220);
    add("vec.y", K::VSlider, 600, 60, "Y", 28, 220);
    add("vec.x", K::HSlider, 370, 290, "X", 220, 24);
    text("A  OSC 1\nB  OSC 2\nC  OSC 3\nD  FM part\n\nDrag the joystick, or move it with an LFO / ENV 3 (VECTOR X / Y).",
         640, 110, 92, 200, juce::Justification::topLeft);

    // ------------------------------------------------------- keyboard split
    pageList.push_back({ "split", "KEYBOARD SPLIT", { 1180, 640, 720, 330 } });
    page("split");
    sectionAt("splitPage", "KEYBOARD  SPLIT  /  LAYER", 0, 0, 720, 330);
    utilityAt("splitClose", 680, 3, 34, 20);
    utilityAt("splitMap", 20, 34, 680, 92);
    heading("LOWER", 20, 138, 200);
    add("split.lo.oct", K::Knob, 20, 158, "OCTAVE");
    add("split.lo.vol", K::Knob, 80, 158, "LEVEL");
    utilityAt("editLower", 140, 170, 120, 26);
    heading("SPLIT POINT", 290, 138, 140);
    add("split.point", K::Knob, 300, 158, "KEY");
    utilityAt("splitLearn", 360, 170, 70, 26);
    heading("UPPER", 460, 138, 200);
    add("split.up.oct", K::Knob, 460, 158, "OCTAVE");
    add("split.up.vol", K::Knob, 520, 158, "LEVEL");
    utilityAt("editUpper", 580, 170, 120, 26);
    text("Two complete sounds, each with its own oscillators, filter, envelopes and effects.  SPLIT: LOWER plays below the "
         "split point, UPPER from it up.  LAYER: both on every key.  SINGLE: just the sound on the panel.  "
         "UPPER / LOWER (next to KEYBOARD, under the panel) choose which sound the panel edits: load a preset or tweak "
         "anything and it goes to that part.  LEARN: press it, then play the key that should start the UPPER zone.",
         20, 236, 680, 84, juce::Justification::topLeft);

    // ------------------------------------------------------ wavetable engine
    pageList.push_back({ "wtengine", "WAVETABLE ENGINE", { 340, 130, 1220, 480 } });
    page("wtengine");
    sectionAt("wtEnginePage", "WAVETABLE  ENGINE", 0, 0, 1220, 480);
    utilityAt("wtengineClose", 1180, 3, 34, 20);
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);
        const int x0 = 20 + i * 300;
        heading("OSC " + n, x0, 32, 200);
        utilityAt("wt3d" + n, x0, 52, 284, 156);
        add("osc" + n + ".wt.table", K::Selector, x0, 216, "TABLE", 186);
        add("osc" + n + ".wt.warp", K::Selector, x0 + 194, 216, "WARP", 90);
        const std::pair<const char*, const char*> knobs[5] = { { ".wt.wamt", "WARP AMT" }, { ".wt.voices", "STACK" },
            { ".wt.det", "DETUNE" }, { ".wt.pspr", "SPREAD" }, { ".wt.width", "WIDTH" } };
        for (int k = 0; k < 5; ++k)
            add("osc" + n + knobs[k].first, K::Knob, x0 + k * 58, 266, knobs[k].second);
    }
    heading("MOTION", 930, 32, 200);
    add("wt.evo.mode", K::Selector, 930, 56, "EVOLVE", 120);
    add("wt.evo.amt",  K::Knob, 1060, 52, "AMOUNT");
    add("wt.evo.time", K::Knob, 1124, 52, "TIME");
    add("wt.drift",    K::Knob, 1060, 128, "DRIFT");
    add("wt.drate",    K::Knob, 1124, 128, "RATE");
    text("EVOLVE moves the scan position from every note-on: SWEEP once over TIME (then holds), "
         "PING-PONG back and forth.  DRIFT makes it wander at random, differently for every voice and every "
         "stack voice: pads that never repeat.",
         930, 204, 270, 110, juce::Justification::topLeft);
    text("TABLE: SECTION follows the panel's WAVETABLE; or give each oscillator its own.  WT POS (on the panel) offsets "
         "its position.  WARP reshapes how the table is read: BEND, SYNC, FORMANT (sync without clicks), MIRROR, SQUEEZE "
         "(PWM on any wave), FOLD, SELF FM, QUANTIZE.  STACK plays up to 8 detuned copies; SPREAD puts each at a "
         "different place in the table, WIDTH spreads them across the stereo field.  The 3D views show each "
         "table, with the position playing now.",
         20, 344, 880, 70, juce::Justification::topLeft);
    text("For pads: a ROM or PADS & AMBIENT table, STACK 5-7, DETUNE 0.2-0.4, SPREAD 0.2, WIDTH 0.8, a slow EVOLVE "
         "and a touch of DRIFT.  Every new control can also be modulated (Osc WT Pos, WT Warp in the matrix).",
         20, 418, 880, 50, juce::Justification::topLeft);
    text("Positions: OSC 1 gold, OSC 2 blue, OSC 3 rose.", 930, 318, 270, 30, juce::Justification::topLeft);

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
    pageList.push_back({ "reverb", "REVERB", { 780, 250, 760, 600 } });
    page("reverb");
    sectionAt("reverbPage", "REVERB  -  ADVANCED", 0, 0, 760, 600);
    utilityAt("reverbClose", 720, 3, 34, 20);
    utilityAt("reverbPresets", 20, 44, 300, 26);
    const std::pair<const char*, const char*> reverbFaders[5] = { { "fx.rev.decay", "DECAY" },
        { "fx.rev.diff", "DIFF" }, { "fx.rev.mod", "MOD" }, { "fx.rev.early", "E / L" }, { "fx.rev.width", "WIDTH" } };
    for (int k = 0; k < 5; ++k)
        fader(reverbFaders[k].first, 24 + k * 50, 100, reverbFaders[k].second);
    heading("REVERB TYPES (TYPE on the panel)", 340, 44, 400);
    // Two columns of short notes, one per type.
    const char* reverbTypes[15] = {
        "HALL       original ZYNTH hall", "ROOM       small, strong reflections", "EMT 140    dense, bright plate",
        "CHAMBER    darker echo chamber", "SPRING     the classic boing", "LEX 224    grainy 80s digital hall",
        "LEX 480L   large, richly modulated", "BRICASTI   dense and natural", "SHIMMER    octave-up feedback",
        "NONLIN     AMS gated burst", "REVERSE    AMS reverse swell", "REV7       bright 80s digital",
        "SP2016     very dense rooms/plates", "QRS        rich room simulator", "PCM70      lush Lexicon hall" };
    for (int k = 0; k < 15; ++k)
        text(reverbTypes[k], k < 8 ? 340 : 548, 68 + (k % 8) * 26, 204, 22, juce::Justification::centredLeft);
    text("DECAY trims SIZE's decay time (on NONLIN it tilts the gate: low = falling, high = rising).  DIFF = density.  "
         "MOD = tail chorusing.  E / L = early reflections vs tail.  HPF / LPF are in the EQ below.",
         340, 280, 410, 80, juce::Justification::topLeft);

    // Parametric EQ on the wet signal: drag the nodes, or use the knobs.
    sectionAt("reverbEq", "REVERB EQ  (wet signal only)", 16, 366, 728, 220, true);
    utilityAt("reverbEqFlat", 668, 368, 70, 16);
    utilityAt("reverbEqCurveBig", 28, 390, 300, 186);
    struct EqCol { const char* title; const char* f; const char* gn; const char* q; };
    const EqCol eqCols[4] = { { "LOW SHELF", "fx.rev.eq.lsf", "fx.rev.eq.lsg", nullptr },
                              { "BELL 1", "fx.rev.eq.p1f", "fx.rev.eq.p1g", "fx.rev.eq.p1q" },
                              { "BELL 2", "fx.rev.eq.p2f", "fx.rev.eq.p2g", "fx.rev.eq.p2q" },
                              { "HIGH SHELF", "fx.rev.eq.hsf", "fx.rev.eq.hsg", nullptr } };
    for (int k = 0; k < 4; ++k)
    {
        const int x = 340 + k * 100;
        heading(eqCols[k].title, x, 392, 96);
        add(eqCols[k].f, K::Knob, x - 2, 414, "FREQ", 48);
        add(eqCols[k].gn, K::Knob, x + 46, 414, "GAIN", 48);
        if (eqCols[k].q != nullptr)
            add(eqCols[k].q, K::Knob, x + 22, 500, "Q", 48);
        else
            add(k == 0 ? "fx.rev.lowcut" : "fx.rev.eq.lpf", K::Knob, x + 22, 500, k == 0 ? "HPF" : "LPF", 48);
    }

    // --------------------------------------------------------- pitch track
    pageList.push_back({ "track", "PITCH TRACK", { 640, 250, 640, 340 } });
    page("track");
    sectionAt("trackPage", "PITCH TRACK  -  GUITAR / BASS IN", 0, 0, 640, 340);
    utilityAt("trackClose", 600, 3, 34, 20);
    utilityAt("trackDisplay", 20, 40, 300, 156);
    add("trk.input", K::Selector, 340, 44, "INPUT", 130);
    add("trk.range", K::Selector, 484, 44, "RANGE", 130);
    add("trk.bend", K::Switch, 340, 96, "BEND FOLLOW", 92);
    add("trk.clean", K::Knob, 470, 88, "CLEAN UP");
    add("trk.sus", K::Knob, 548, 88, "SUSTAIN");
    const std::pair<const char*, const char*> trackKnobs[5] = { { "trk.gain", "GAIN" }, { "trk.gate", "GATE" },
        { "trk.thresh", "TRACKING" }, { "trk.vel", "VELOCITY" }, { "trk.dry", "DRY" } };
    for (int k = 0; k < 5; ++k)
        add(trackKnobs[k].first, K::Knob, 20 + k * 62, 214, trackKnobs[k].second);
    text("Play one note at a time.  GAIN: strong picking should reach well past the gate line on the meter.  "
         "GATE: raise it if hum or string noise starts notes.  TRACKING: loose = fastest, strict = steadiest.  "
         "CLEAN UP: removes stray short low notes (doubtful readings must be confirmed; picked notes stay instant).  "
         "SUSTAIN: notes ring on as the string dies away; full = HOLD (until the next note; a muted pick stops it).  "
         "Choose the audio interface and input in Options > Audio/MIDI Settings.  While PITCH TRACK is on the "
         "keyboards, QWERTY and MIDI notes are off (MIDI controllers such as sustain still work).",
         340, 162, 280, 170, juce::Justification::topLeft);

    // ------------------------------------------------------------ harmony
    pageList.push_back({ "harm", "HARMONY", { 1100, 110, 780, 410 } });
    page("harm");
    sectionAt("harmPage", "HARMONY  -  INTELLIGENT HARMONISER", 0, 0, 780, 410);
    utilityAt("harmClose", 740, 3, 34, 20);
    add("harm.key", K::Selector, 20, 40, "KEY", 80);
    add("harm.scale", K::Selector, 108, 40, "SCALE", 150);
    utilityAt("harmDetect", 266, 54, 104, 26);
    utilityAt("harmInfo", 386, 36, 374, 70);
    heading("VOICES", 20, 120, 200);
    add("harm.voices", K::Knob, 20, 140, "VOICES");
    utilityAt("harmStyle", 80, 156, 112, 26);
    for (int v = 0; v < 3; ++v)
    {
        const juce::String n(v + 1);
        const int x = 214 + v * 186;
        heading("VOICE " + n, x, 120, 160);
        add("harm.v" + n + ".int", K::Selector, x, 144, "INTERVAL", 96);
        add("harm.v" + n + ".dir", K::Selector, x + 102, 144, "UP / DOWN", 70);
    }
    heading("PLAYING", 20, 222, 200);
    add("harm.mode", K::Selector, 20, 244, "HARMONISE", 116);
    add("harm.outkey", K::Selector, 144, 244, "OUT OF KEY", 120);
    add("harm.part", K::Selector, 272, 244, "APPLIES TO", 96);
    add("harm.level", K::Knob, 400, 236, "LEVEL");
    add("harm.strum", K::Knob, 460, 236, "STRUM");
    add("harm.lo", K::Knob, 540, 236, "LOWEST");
    add("harm.hi", K::Knob, 600, 236, "HIGHEST");
    text("RANGE", 540, 222, 112, 14, juce::Justification::centred, true);
    text("Every note you play (keyboard, MIDI or PITCH TRACK) gets up to three more, in KEY: intervals count scale steps, "
         "so a 3rd above is always a note of the scale.  DETECT: play a few bars and it finds the key.  STYLE fills the voices "
         "in (thirds, triad, power, sus4, open).  HARMONISE: EVERY NOTE, or the TOP NOTE only (the melody of a chord: much "
         "cleaner on keyboard chords).  OUT OF KEY: what happens to a played note outside the scale.  STRUM staggers the "
         "harmony notes; RANGE folds them back by octaves.  Harmony notes use voices: raise POLY if notes drop out.",
         20, 318, 740, 84, juce::Justification::topLeft);

    // ---------------------------------------------------------- tape page
    pageList.push_back({ "tape", "TAPE", { 600, 400, 720, 170 } });
    page("tape");
    sectionAt("tapePage", "TAPE  -  ADVANCED", 0, 0, 720, 170);
    utilityAt("tapeClose", 680, 3, 34, 20);
    add("tape.speed", K::Selector, 24, 56, "SPEED", 110);
    add("tape.bias", K::LargeKnob, 156, 50, "BIAS");
    add("tape.hiss", K::LargeKnob, 244, 50, "HISS");
    add("tape.out", K::LargeKnob, 332, 50, "OUTPUT");
    text("MACHINE, DRIVE and WOW / FLUTTER are on the panel.  DRIVE pushes the tape: clean at 0 dB, warm from +6, "
         "clearly saturated by +12 and heavy at +24.  Slower speeds: more head bump in the bass, a darker top and more wow.  BIAS 0.5 = calibrated.",
         430, 44, 270, 116, juce::Justification::topLeft);

    // ------------------------------------------------------- comp advanced
    pageList.push_back({ "comp", "OUTPUT COMP", { 1050, 300, 850, 380 } });
    page("comp");
    sectionAt("compPage", "OUTPUT COMP  -  ADVANCED", 0, 0, 850, 380);
    utilityAt("compClose", 810, 3, 34, 20);
    utilityAt("compMeterBig", 20, 44, 260, 190);
    add("comp.ratio", K::Selector, 300, 48, "RATIO", 120);
    add("comp.autorel", K::Switch, 430, 48, "AUTO REL", 70);
    const std::pair<const char*, const char*> compFaders[5] = { { "comp.attack", "ATTACK" }, { "comp.release", "RELEASE" },
        { "comp.knee", "KNEE" }, { "comp.emph", "EMPH" }, { "comp.color", "COLOUR" } };
    for (int k = 0; k < 5; ++k)
        add(compFaders[k].first, K::LargeKnob, k < 3 ? 300 + k * 92 : 346 + (k - 3) * 92, k < 3 ? 104 : 214, compFaders[k].second);
    heading("MODELS (MODEL on the panel)", 590, 44, 250);
    const char* compNotes[4] = {
        "LA-2A OPTO   Optical leveller: smooth, program-dependent two-stage release. THRESH = peak reduction; ratio up to 4:1 = COMPRESS, 8:1+ = LIMIT; EMPH = HF emphasis (bright sounds compressed harder).",
        "SSL BUS      VCA bus compressor for glue: ratio, attack 0.1-30 ms, release 0.1-1.2 s or AUTO, knee. Very clean.",
        "1176 FET     Fast FET limiter: attack 20-800 us, release 50 ms-1.1 s, 4/8/12/20:1 or ALL buttons (aggressive, gritty).",
        "FAIRCHILD    Variable-mu valve: very soft knee, ratio rises with level, slow and smooth; rich valve colour." };
    for (int k = 0; k < 4; ++k)
        text(compNotes[k], 590, 66 + k * 70, 250, 66, juce::Justification::topLeft);
    text("GAIN REDUCTION", 20, 240, 260, 16, juce::Justification::centred, true);
    text("COLOUR adds the model's valve / FET / VCA saturation: gentle at 0.5, clearly driven at full.  MIX on the panel blends in the dry signal for parallel compression.",
         20, 262, 260, 70, juce::Justification::topLeft);

    // ---------------------------------------------------------- graphic EQ
    pageList.push_back({ "peq", "EQ", { 880, 300, 1000, 430 } });
    page("peq");
    sectionAt("peqPage", "EQ  -  OUTPUT", 0, 0, 1000, 430);
    utilityAt("peqClose", 960, 3, 34, 20);
    utilityAt("peqFlat", 884, 4, 66, 18);
    utilityAt("peqCurveBig", 20, 36, 960, 200);
    // Shared by every model: the output level and (classic models) the drive.
    heading("OUTPUT", 836, 248, 120);
    add("peq.out", K::Knob, 822, 270, "LEVEL");
    add("eq.drive", K::Knob, 886, 270, "DRIVE");
    text("ON, PRE FX and MODEL are on the panel.", 822, 346, 158, 40, juce::Justification::topLeft);

    // PRISTINE: the clean parametric (drag the curve's nodes or use the knobs).
    view("eq:pristine");
    heading("LOW CUT", 20, 248, 80);
    add("peq.hpf", K::Knob, 24, 270, "FREQ", 48);
    for (int b = 0; b < 5; ++b)
    {
        const juce::String n(b + 1);
        const int x = 116 + b * 120;
        heading("BAND " + n, x, 248, 110);
        add("peq.b" + n + ".f", K::Knob, x - 2, 270, "FREQ", 48);
        add("peq.b" + n + ".g", K::Knob, x + 50, 270, "GAIN", 48);
        add("peq.b" + n + ".q", K::Knob, x + 24, 346, "Q", 48);
    }
    heading("HIGH CUT", 716, 248, 80);
    add("peq.lpf", K::Knob, 720, 270, "FREQ", 48);

    // PULTEC EQP-1A, laid out as the front panel.
    view("eq:pultec");
    heading("LOW FREQUENCY", 24, 248, 300);
    heading("HIGH FREQUENCY", 312, 248, 300);
    heading("HIGH ATTENUATION", 600, 248, 200);
    const std::pair<const char*, const char*> pultec[8] = { { "pul.lf", "CPS" }, { "pul.lb", "BOOST" }, { "pul.la", "ATTEN" },
        { "pul.hf", "KCS" }, { "pul.hb", "BOOST" }, { "pul.bw", "BANDWIDTH" }, { "pul.af", "ATTEN SEL" }, { "pul.ha", "ATTEN" } };
    for (int k = 0; k < 8; ++k)
        add(pultec[k].first, K::LargeKnob, 20 + k * 98 - (k >= 3 ? 0 : 0), 272, pultec[k].second);
    text("Boost and atten at the same LOW frequency: the Pultec trick (sub lift, low-mid dip).", 20, 380, 560, 20, juce::Justification::centredLeft);

    // NEVE 1073.
    view("eq:neve");
    const std::pair<const char*, const char*> neve[6] = { { "nev.hpf", "HPF" }, { "nev.lf", "LOW FREQ" }, { "nev.lg", "LOW" },
        { "nev.mf", "MID FREQ" }, { "nev.mg", "MID" }, { "nev.hg", "HIGH 12K" } };
    heading("HIGH PASS", 24, 248, 110);
    heading("LOW SHELF", 150, 248, 220);
    heading("MID", 390, 248, 220);
    heading("HIGH", 630, 248, 110);
    for (int k = 0; k < 6; ++k)
        add(neve[k].first, K::LargeKnob, 30 + k * 120, 272, neve[k].second);

    // API 550A: three bands, stepped.
    view("eq:api550a");
    const char* a550names[3] = { "LOW", "MID", "HIGH" };
    const char* a550ids[3] = { "a5.l", "a5.m", "a5.h" };
    for (int b = 0; b < 3; ++b)
    {
        const int x0 = 24 + b * 250;
        heading(a550names[b], x0, 248, 220);
        add(juce::String(a550ids[b]) + "f", K::LargeKnob, x0, 272, "FREQ");
        add(juce::String(a550ids[b]) + "g", K::LargeKnob, x0 + 82, 272, "GAIN");
    }
    add("a5.ls", K::Switch, 24 + 168, 290, "SHELF", 48);
    add("a5.bp", K::Switch, 24 + 250 + 168, 290, "BAND PASS", 64);
    add("a5.hs", K::Switch, 24 + 500 + 168, 290, "SHELF", 48);

    // API 550B: four bands, stepped.
    view("eq:api550b");
    const char* b550names[4] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
    const char* b550ids[4] = { "b5.l", "b5.lm", "b5.hm", "b5.h" };
    for (int b = 0; b < 4; ++b)
    {
        const int x0 = 20 + b * 196;
        heading(b550names[b], x0, 248, 180);
        add(juce::String(b550ids[b]) + "f", K::LargeKnob, x0, 266, "FREQ");
        add(juce::String(b550ids[b]) + "g", K::LargeKnob, x0 + 78, 266, "GAIN");
    }
    add("b5.ls", K::Switch, 20 + 50, 364, "SHELF", 48);
    add("b5.hs", K::Switch, 20 + 3 * 196 + 50, 364, "SHELF", 48);

    // API 560: ten-band graphic.
    view("eq:api560");
    const char* g560names[10] = { "31", "63", "125", "250", "500", "1K", "2K", "4K", "8K", "16K" };
    for (int b = 0; b < 10; ++b)
        add("g560.b" + juce::String(b + 1), K::Fader, 40 + b * 74, 246, g560names[b]);
    view({});

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
