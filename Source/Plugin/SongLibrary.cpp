#include "SongLibrary.h"

namespace zsynth
{
namespace
{
    // ZS_CH_PSEQ_DIV indexes.
    constexpr int kDiv16 = 2, kDiv8 = 4, kDiv4 = 6, kDiv2 = 7, kDivBar = 8;

    int noteNumber(const juce::String& name)
    {
        // "C4" = 60 (middle C), "F#2", "Bb3".
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
        const auto letter = juce::CharacterFunctions::toUpperCase(name[0]);
        if (letter < 'A' || letter > 'G')
            return -1;
        int n = base[letter - 'A'];
        int i = 1;
        if (name[i] == '#') { ++n; ++i; }
        else if (name[i] == 'b') { --n; ++i; }
        return n + 12 * (name.substring(i).getIntValue() + 1);
    }

    std::array<float, 2 * kPseqRowParams> blankPattern()
    {
        // Every row parameter at its default, every step a rest.
        std::array<float, 2 * kPseqRowParams> v {};
        for (int k = 0; k < 2 * kPseqRowParams; ++k)
            v[(size_t) k] = paramDef(P::PseqAOn + k).def;
        for (int r = 0; r < 2; ++r)
            for (int st = 0; st < kPseqSteps; ++st)
                v[(size_t) (r * kPseqRowStride + P::PseqAGate1 - P::PseqAOn + st * kPseqStepStride)] = 0.0f;
        return v;
    }

    /** One row of a pattern. Steps: a note ("F#4", "!" after it = accent), "~" = the
        note before is held on (tied), "." = rest. LENGTH = the number of steps. */
    void setRow(std::array<float, 2 * kPseqRowParams>& v, int row, int div, float gatePercent, int velocity, const char* steps)
    {
        auto at = [&](int field) -> float& { return v[(size_t) (row * kPseqRowStride + field - P::PseqAOn)]; };
        const auto tokens = juce::StringArray::fromTokens(steps, " ", "");
        at(P::PseqAOn) = 1.0f;
        at(P::PseqADiv) = (float) div;
        at(P::PseqAGateLen) = gatePercent;
        at(P::PseqAVel) = (float) velocity;
        at(P::PseqALen) = (float) juce::jlimit(1, kPseqSteps, tokens.size());
        int last = paramDef(P::PseqAPitch1 + row * kPseqRowStride).def > 0 ? (int) paramDef(P::PseqAPitch1 + row * kPseqRowStride).def : 60;
        for (int st = 0; st < tokens.size() && st < kPseqSteps; ++st)
        {
            const int f = P::PseqAPitch1 + st * kPseqStepStride;
            auto t = tokens[st];
            float gate = 1.0f, accent = 0.0f;
            if (t == ".")
                gate = 0.0f;
            else if (t == "~")
            {
                if (st > 0)
                    at(f - kPseqStepStride + 3) = 1.0f; // the step before slides into this one
            }
            else
            {
                if (t.endsWithChar('!'))
                {
                    accent = 1.0f;
                    t = t.dropLastCharacters(1);
                }
                const int n = noteNumber(t);
                jassert(n >= 21 && n <= 108);
                last = juce::jlimit(21, 108, n);
            }
            at(f) = (float) last;
            at(f + 1) = gate;
            at(f + 2) = accent;
            at(f + 3) = 0.0f;
        }
    }

    struct Builder
    {
        SongData song;
        Builder(const char* name, float tempo)
        {
            song.name = name;
            song.tempo = tempo;
            for (auto& p : song.patterns)
                p = blankPattern();
        }
        /** pattern 0..7 = A..H */
        Builder& rows(int pattern, int divA, float gateA, int velA, const char* a, int divB, float gateB, int velB, const char* b)
        {
            setRow(song.patterns[(size_t) pattern], 0, divA, gateA, velA, a);
            setRow(song.patterns[(size_t) pattern], 1, divB, gateB, velB, b);
            return *this;
        }
        /** "A B Cx2 ... END" / "LOOP": a pattern letter with an optional repeat count. */
        Builder& chain(const char* slots)
        {
            const auto tokens = juce::StringArray::fromTokens(slots, " ", "");
            for (int i = 0; i < tokens.size() && i < SongData::kSlots; ++i)
            {
                const auto t = tokens[i];
                if (t == "END")
                    song.slots[(size_t) i] = 10;
                else if (t == "LOOP")
                    song.slots[(size_t) i] = 9;
                else
                {
                    song.slots[(size_t) i] = 1 + (t[0] - 'A');
                    const int times = t.fromFirstOccurrenceOf("x", false, false).getIntValue();
                    song.repeats[(size_t) i] = juce::jlimit(0, 15, times - 1);
                }
            }
            return *this;
        }
    };

    SongData bachPrelude()
    {
        // J. S. Bach: Prelude in C major, BWV 846 (Well-Tempered Clavier I), bars 1-8:
        // each bar's broken chord played twice in sixteenths over its bass note.
        Builder b("Bach - Prelude in C (BWV 846)", 66.0f);
        const char* bars[8][2] = {
            { "C4 E4 G4 C5 E5 G4 C5 E5 C4 E4 G4 C5 E5 G4 C5 E5", "C3" },
            { "C4 D4 A4 D5 F5 A4 D5 F5 C4 D4 A4 D5 F5 A4 D5 F5", "C3" },
            { "B3 D4 G4 D5 F5 G4 D5 F5 B3 D4 G4 D5 F5 G4 D5 F5", "B2" },
            { "C4 E4 G4 C5 E5 G4 C5 E5 C4 E4 G4 C5 E5 G4 C5 E5", "C3" },
            { "C4 E4 A4 E5 A5 A4 E5 A5 C4 E4 A4 E5 A5 A4 E5 A5", "C3" },
            { "C4 D4 F#4 A4 D5 F#4 A4 D5 C4 D4 F#4 A4 D5 F#4 A4 D5", "C3" },
            { "B3 D4 G4 D5 G5 G4 D5 G5 B3 D4 G4 D5 G5 G4 D5 G5", "B2" },
            { "B3 C4 E4 G4 C5 E4 G4 C5 B3 C4 E4 G4 C5 E4 G4 C5", "B2" },
        };
        for (int i = 0; i < 8; ++i)
            b.rows(i, kDiv16, 85.0f, 92, bars[i][0], kDivBar, 100.0f, 84, bars[i][1]);
        b.chain("A B C D E F G H LOOP");
        return b.song;
    }

    SongData grieg()
    {
        // Edvard Grieg: In the Hall of the Mountain King (Peer Gynt), the theme in
        // B minor over a pizzicato pedal; then again an octave up, accented.
        Builder b("Grieg - In the Hall of the Mountain King", 132.0f);
        const char* pedal = "B1 . F#2 . B1 . F#2 . B1 . F#2 . B1 . F#2 .";
        const char* pedalHard = "B1! . F#2! . B1! . F#2! . B1! . F#2! . B1! . F#2! .";
        b.rows(0, kDiv8, 45.0f, 96, "B3 C#4 D4 E4 F#4 D4 F#4 ~ F4 C#4 F4 ~ E4 C4 E4 ~", kDiv8, 35.0f, 90, pedal);
        b.rows(1, kDiv8, 45.0f, 96, "B3 C#4 D4 E4 F#4 D4 F#4 B4 A4 F#4 D4 F#4 A4 ~ ~ ~", kDiv8, 35.0f, 90, pedal);
        b.rows(2, kDiv8, 45.0f, 110, "B4! C#5 D5 E5 F#5! D5 F#5 ~ F5! C#5 F5 ~ E5! C5 E5 ~", kDiv8, 35.0f, 100, pedalHard);
        b.rows(3, kDiv8, 45.0f, 110, "B4! C#5 D5 E5 F#5! D5 F#5 B5 A5! F#5 D5 F#5 A5! ~ ~ ~", kDiv8, 35.0f, 100, pedalHard);
        // The final stabs.
        b.rows(4, kDiv8, 40.0f, 120, "B4! . . . F#4! . . . B3! . . . . . . .", kDiv8, 40.0f, 110, "B1! . . . F#1! . . . B1! . . . . . . .");
        b.chain("A B A B C D C D E END");
        return b.song;
    }

    SongData pachelbel()
    {
        // Johann Pachelbel: Canon in D. The ground bass (D A Bm F#m G D G A) under
        // arpeggios, then the long-note melody, then the walking quarter-note variation.
        Builder b("Pachelbel - Canon in D", 76.0f);
        const char* bass1 = "D3 A2 B2 F#2";
        const char* bass2 = "G2 D2 G2 A2";
        b.rows(0, kDiv8, 70.0f, 84, "D4 F#4 A4 F#4 C#4 E4 A4 E4 D4 F#4 B4 F#4 C#4 F#4 A4 F#4", kDiv2, 96.0f, 88, bass1);
        b.rows(1, kDiv8, 70.0f, 84, "D4 G4 B4 G4 D4 F#4 A4 F#4 D4 G4 B4 G4 C#4 E4 A4 E4", kDiv2, 96.0f, 88, bass2);
        b.rows(2, kDiv2, 97.0f, 96, "F#5 E5 D5 C#5", kDiv2, 96.0f, 88, bass1);
        b.rows(3, kDiv2, 97.0f, 96, "B4 A4 B4 C#5", kDiv2, 96.0f, 88, bass2);
        b.rows(4, kDiv4, 80.0f, 96, "D5 F#5 A5 G5 F#5 D5 F#5 E5", kDiv2, 96.0f, 88, bass1);
        b.rows(5, kDiv4, 80.0f, 96, "D5 B4 D5 A4 G4 B4 A4 G4", kDiv2, 96.0f, 88, bass2);
        b.chain("A B A B C D C D E F E F C D A B END");
        return b.song;
    }

    SongData minuet()
    {
        // Christian Petzold: Minuet in G (from the Anna Magdalena Bach notebook),
        // the first half: two bars of 3/4 per pattern, the melody over the bass.
        Builder b("Petzold - Minuet in G", 116.0f);
        b.rows(0, kDiv8, 75.0f, 96, "D5 ~ G4 A4 B4 C5 D5 ~ G4 ~ G4 ~", kDiv4, 90.0f, 84, "G2 ~ ~ B2 ~ ~");
        b.rows(1, kDiv8, 75.0f, 96, "E5 ~ C5 D5 E5 F#5 G5 ~ G4 ~ G4 ~", kDiv4, 90.0f, 84, "C3 ~ ~ B2 ~ ~");
        b.rows(2, kDiv8, 75.0f, 96, "C5 ~ D5 C5 B4 A4 B4 ~ C5 B4 A4 G4", kDiv4, 90.0f, 84, "A2 ~ ~ G2 ~ ~");
        b.rows(3, kDiv8, 75.0f, 96, "F#4 ~ G4 A4 B4 G4 A4 ~ ~ ~ ~ ~", kDiv4, 90.0f, 84, "D3 ~ ~ D2 ~ ~");
        b.rows(4, kDiv8, 75.0f, 96, "A4 ~ B4 A4 G4 F#4 G4 ~ ~ ~ ~ ~", kDiv4, 90.0f, 84, "D3 ~ ~ G2 ~ ~");
        b.chain("A B C D A B C E END");
        return b.song;
    }
}

const std::vector<SongData>& factorySongs()
{
    static const std::vector<SongData> songs { bachPrelude(), grieg(), pachelbel(), minuet() };
    return songs;
}

std::unique_ptr<juce::XmlElement> songToXml(const SongData& s)
{
    auto xml = std::make_unique<juce::XmlElement>("ZynthSong");
    xml->setAttribute("version", 1);
    xml->setAttribute("name", s.name);
    xml->setAttribute("tempo", s.tempo);
    xml->setAttribute("swing", s.swing);
    for (const auto& p : s.patterns)
    {
        juce::String values;
        for (float v : p)
            values << juce::String(v, 4) << " ";
        xml->createNewChildElement("Pattern")->setAttribute("values", values.trimEnd());
    }
    juce::String slots, repeats;
    for (int i = 0; i < SongData::kSlots; ++i)
    {
        slots << s.slots[(size_t) i] << " ";
        repeats << s.repeats[(size_t) i] << " ";
    }
    xml->createNewChildElement("Slots")->setAttribute("values", slots.trimEnd());
    xml->createNewChildElement("Repeats")->setAttribute("values", repeats.trimEnd());
    return xml;
}

bool songFromXml(const juce::XmlElement& xml, SongData& s)
{
    if (! xml.hasTagName("ZynthSong"))
        return false;
    s = SongData();
    for (auto& p : s.patterns)
        p = blankPattern();
    s.name = xml.getStringAttribute("name");
    s.tempo = juce::jlimit(40.0f, 240.0f, (float) xml.getDoubleAttribute("tempo", 120.0));
    s.swing = juce::jlimit(50.0f, 75.0f, (float) xml.getDoubleAttribute("swing", 50.0));
    int pi = 0;
    for (auto* pat : xml.getChildWithTagNameIterator("Pattern"))
    {
        if (pi >= SongData::kPatterns)
            break;
        const auto tokens = juce::StringArray::fromTokens(pat->getStringAttribute("values"), " ", "");
        auto& dst = s.patterns[(size_t) pi++];
        for (int k = 0; k < (int) dst.size() && k < tokens.size(); ++k)
        {
            const auto& d = paramDef(P::PseqAOn + k);
            dst[(size_t) k] = juce::jlimit(d.min, d.max, tokens[k].getFloatValue());
        }
    }
    auto ints = [&](const char* tag, std::array<int, SongData::kSlots>& dst, int hi) {
        if (auto* e = xml.getChildByName(tag))
        {
            const auto tokens = juce::StringArray::fromTokens(e->getStringAttribute("values"), " ", "");
            for (int i = 0; i < SongData::kSlots && i < tokens.size(); ++i)
                dst[(size_t) i] = juce::jlimit(0, hi, tokens[i].getIntValue());
        }
    };
    ints("Slots", s.slots, 10);
    ints("Repeats", s.repeats, 15);
    return true;
}

} // namespace zsynth
