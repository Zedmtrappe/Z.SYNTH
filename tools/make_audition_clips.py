#!/usr/bin/env python3
"""Generates ZYNTH's built-in audition clips (Resources/Audition/*.mid).

Each clip is an original, loopable phrase written for auditioning a patch:
standard MIDI file format 0, one track on channel 1, 480 ticks per quarter.
Re-run after editing:  python3 tools/make_audition_clips.py
"""
import os
import struct

PPQ = 480
OUT = os.path.join(os.path.dirname(__file__), "..", "Resources", "Audition")


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


class Clip:
    def __init__(self, name, bpm, bars, beats_per_bar=4):
        self.name, self.bpm, self.bars, self.bpb = name, bpm, bars, beats_per_bar
        self.events = []  # (tick, order, bytes)

    def note(self, beat, length, pitch, vel=96):
        on = int(round(beat * PPQ))
        off = int(round((beat + length) * PPQ))
        self.events.append((on, 1, bytes([0x90, pitch, int(vel)])))
        self.events.append((off, 0, bytes([0x80, pitch, 0])))

    def chord(self, beat, length, pitches, vel=90, strum=0.0):
        for i, p in enumerate(pitches):
            self.note(beat + i * strum, length - i * strum, p, vel)

    def cc(self, beat, num, val):
        self.events.append((int(round(beat * PPQ)), 2, bytes([0xB0, num, val])))

    def write(self, filename):
        length = self.bars * self.bpb * PPQ
        tempo = int(round(60_000_000 / self.bpm))
        track = bytearray()
        track += vlq(0) + b"\xff\x03" + vlq(len(self.name)) + self.name.encode()
        track += vlq(0) + b"\xff\x51\x03" + tempo.to_bytes(3, "big")
        track += vlq(0) + b"\xff\x58\x04" + bytes([self.bpb, 2, 24, 8])
        last = 0
        for tick, _, data in sorted(self.events, key=lambda e: (e[0], e[1])):
            tick = min(tick, length)
            track += vlq(tick - last) + data
            last = tick
        track += vlq(length - last) + b"\xff\x2f\x00"  # end of track marks the loop length
        data = b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ) + b"MTrk" + struct.pack(">I", len(track)) + track
        with open(os.path.join(OUT, filename), "wb") as f:
            f.write(data)


def n(name):
    """Note name like 'A2', 'C#4', 'Bb3' -> MIDI number (C4 = 60)."""
    names = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
    p = names[name[0]]
    i = 1
    while name[i] in "#b":
        p += 1 if name[i] == "#" else -1
        i += 1
    return p + 12 * (int(name[i:]) + 1)


def clips():
    # 1 -- Mono bassline: driving 16ths in A minor with a few legato slides.
    c = Clip("Mono Bassline", 118, 4)
    pattern = ["A1", None, "A1", "A2", None, "A1", "G1", None, "A1", None, "C2", "A1", None, "E2", "D2", "C2"]
    roots = ["A1", "A1", "F1", "G1"]
    for bar in range(4):
        shift = n(roots[bar]) - n("A1")
        for step, name in enumerate(pattern):
            if name is None:
                continue
            slide = step in (13, 14)
            c.note(bar * 4 + step * 0.25, 0.3 if slide else 0.18, n(name) + shift, 110 if step % 4 == 0 else 88)
    c.write("01 Mono Bassline.mid")

    # 2 -- Acid line: 303-style with accents and slides (overlapping notes).
    c = Clip("Acid Line", 126, 4)
    seq = [("C2", 1, 0), ("C3", 0, 1), ("C2", 0, 0), ("D#2", 1, 0), ("C2", 0, 0), ("G2", 0, 1), ("A#2", 1, 0), ("C2", 0, 0),
           ("C2", 0, 0), ("F2", 1, 1), ("G2", 0, 0), ("C3", 1, 0), ("C2", 0, 0), ("D#2", 0, 1), ("F2", 0, 0), ("G2", 1, 0)]
    for bar in range(4):
        for step, (name, accent, slide) in enumerate(seq):
            if bar == 3 and step >= 12:
                name = ["A#2", "G2", "F2", "D#2"][step - 12]
            c.note(bar * 4 + step * 0.25, 0.3 if slide else 0.14, n(name), 120 if accent else 80)
    c.write("02 Acid Line.mid")

    # 3 -- Mono lead: a singing legato melody (overlaps trigger glide in legato modes).
    c = Clip("Mono Lead", 96, 8)
    melody = [(0, 1.5, "E4"), (1.5, 0.5, "G4"), (2, 1, "A4"), (3, 1, "C5"), (4, 2, "B4"), (6, 1, "A4"), (7, 1, "G4"),
              (8, 1.5, "A4"), (9.5, 0.5, "B4"), (10, 2, "C5"), (12, 1, "D5"), (13, 1, "E5"), (14, 2, "D5"),
              (16, 1, "E5"), (17, 1, "G5"), (18, 1.5, "E5"), (19.5, 0.5, "D5"), (20, 2, "C5"), (22, 1, "B4"), (23, 1, "C5"),
              (24, 1, "A4"), (25, 1, "G4"), (26, 1, "E4"), (27, 1, "G4"), (28, 4, "A4")]
    for beat, length, name in melody:
        c.note(beat, length + 0.06, n(name), 84 + (beat * 7) % 30)
    c.write("03 Mono Lead.mid")

    # 4 -- Poly chords: sustained four-voice progression (Am - F - C - G).
    c = Clip("Poly Chords", 100, 8)
    prog = [["A3", "C4", "E4", "A4"], ["F3", "A3", "C4", "F4"], ["C3", "G3", "C4", "E4"], ["G3", "B3", "D4", "G4"]]
    for bar in range(8):
        ch = prog[bar % 4]
        c.chord(bar * 4, 3.8, [n(p) for p in ch], 84 if bar % 2 == 0 else 76)
        c.note(bar * 4, 3.8, n(ch[0]) - 12, 92)
    c.write("04 Poly Chords.mid")

    # 5 -- Electric piano ballad: broken voicings, a bass line and the sustain pedal.
    c = Clip("E.Piano Ballad", 76, 4)
    prog = [("F2", ["A3", "C4", "E4", "G4"]), ("E2", ["G3", "B3", "D4", "E4"]), ("D2", ["F3", "A3", "C4", "E4"]),
            ("C2", ["E3", "G3", "B3", "D4"])]
    for bar, (bass, voicing) in enumerate(prog):
        b = bar * 4
        c.cc(b, 64, 127)
        c.note(b, 3.8, n(bass), 82)
        c.chord(b, 1.6, [n(p) for p in voicing], 72, strum=0.03)
        c.chord(b + 1.5, 1.0, [n(p) for p in voicing[1:]], 58)
        c.note(b + 2.5, 0.5, n(voicing[-1]) + 3, 76)
        c.chord(b + 3, 0.9, [n(p) for p in voicing[:3]], 64)
        c.cc(b + 3.95, 64, 0)
    c.write("05 E.Piano Ballad.mid")

    # 6 -- Pad swell: slow, wide, overlapping chords for pads and strings.
    c = Clip("Pad Swell", 70, 8)
    prog = [["D3", "A3", "E4", "F4"], ["Bb2", "F3", "D4", "A4"], ["F3", "C4", "G4", "A4"], ["C3", "G3", "E4", "G4"]]
    for i, ch in enumerate(prog):
        c.chord(i * 8, 8.4, [n(p) for p in ch], 70)
    c.write("06 Pad Swell.mid")

    # 7 -- Arpeggio: 16th-note arpeggios across a progression.
    c = Clip("Arpeggio", 122, 4)
    prog = [["A3", "C4", "E4", "A4"], ["F3", "A3", "C4", "F4"], ["C4", "E4", "G4", "C5"], ["G3", "B3", "D4", "G4"]]
    order = [0, 1, 2, 3, 2, 1, 2, 3]
    for bar, ch in enumerate(prog):
        for step in range(16):
            p = n(ch[order[step % 8]]) + (12 if step >= 8 else 0)
            c.note(bar * 4 + step * 0.25, 0.2, p, 100 if step % 4 == 0 else 78)
    c.write("07 Arpeggio.mid")

    # 8 -- Pluck pattern: syncopated poly plucks.
    c = Clip("Pluck Pattern", 124, 4)
    prog = [["E4", "G4", "B4"], ["C4", "E4", "G4"], ["D4", "F#4", "A4"], ["B3", "D4", "F#4"]]
    rhythm = [0, 0.75, 1.5, 2.5, 3.0, 3.5]
    for bar, ch in enumerate(prog):
        for i, t in enumerate(rhythm):
            c.chord(bar * 4 + t, 0.22, [n(p) for p in ch], 96 if i % 2 == 0 else 80)
    c.write("08 Pluck Pattern.mid")

    # 9 -- Brass stabs: short chord hits with a held ending.
    c = Clip("Brass Stabs", 108, 4)
    stabs = [(0, 0.3), (0.75, 0.3), (1.5, 0.6), (3, 0.3), (3.5, 0.3)]
    prog = [["C4", "E4", "G4", "C5"], ["Bb3", "D4", "F4", "Bb4"], ["Ab3", "C4", "Eb4", "Ab4"], ["G3", "B3", "D4", "G4"]]
    for bar, ch in enumerate(prog):
        if bar == 3:
            c.chord(bar * 4, 3.6, [n(p) for p in ch], 110)
            continue
        for t, length in stabs:
            c.chord(bar * 4 + t, length, [n(p) for p in ch], 112 if t == 0 else 96)
    c.write("09 Brass Stabs.mid")

    # 10 -- Bells and keys: a sparse melody over soft chords (FM bells, mallets).
    c = Clip("Bells and Keys", 88, 4)
    for bar, ch in enumerate([["C4", "E4", "G4"], ["A3", "C4", "E4"], ["F3", "A3", "C4"], ["G3", "B3", "D4"]]):
        c.chord(bar * 4, 3.5, [n(p) for p in ch], 60)
    for beat, length, name, vel in [(0, 1, "G5", 100), (1, 1, "E5", 84), (2, 1.5, "C6", 104), (4, 1, "A5", 96), (5, 1, "E5", 80),
                                    (6, 2, "C6", 100), (8, 1, "F5", 92), (9, 1, "A5", 96), (10, 2, "C6", 108), (12, 1, "B5", 96),
                                    (13, 1, "G5", 84), (14, 2, "D6", 110)]:
        c.note(beat, length, n(name), vel)
    c.write("10 Bells and Keys.mid")


def prog_notes(names):
    return [n(x) for x in names]


def more_clips():
    # 12 -- Trance lead: an anthem melody in 8ths and 16ths (SuperSaw leads).
    c = Clip("Trance Lead", 138, 8)
    phrase = ["A4", "C5", "E5", "A5", "G5", "E5", "D5", "E5", "C5", "D5", "E5", "G5", "E5", "D5", "C5", "B4"]
    for bar in range(8):
        root_shift = [0, 0, -4, -4, -9, -9, -2, -2][bar]
        for i, name in enumerate(phrase):
            if (bar % 2 == 1) and i >= 12:
                continue
            c.note(bar * 4 + i * 0.25, 0.22, n(name) + root_shift, 112 if i % 4 == 0 else 92)
        if bar % 2 == 1:
            c.note(bar * 4 + 3, 1.0, n("A5") + root_shift, 108)
    c.write("12 Trance Lead.mid")

    # 13 -- Trance gate chords: 16th-note gated chords (pads with a pumping feel).
    c = Clip("Gated Chords", 138, 4)
    prog = [["A3", "C4", "E4", "A4"], ["F3", "A3", "C4", "F4"], ["C4", "E4", "G4", "C5"], ["G3", "B3", "D4", "G4"]]
    gate = [1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1]
    for bar, ch in enumerate(prog):
        for step, on in enumerate(gate):
            if on:
                c.chord(bar * 4 + step * 0.25, 0.2, prog_notes(ch), 100 if step % 4 == 0 else 80)
    c.write("13 Gated Chords.mid")

    # 14 -- Hoover riff: the rave riff with long slides (legato).
    c = Clip("Hoover Riff", 135, 4)
    riff = [(0, 1.0, "C3"), (0.9, 0.6, "D#3"), (1.5, 0.6, "C3"), (2, 1.6, "G2"), (3.5, 0.6, "A#2"),
            (4, 1.0, "C3"), (4.9, 0.6, "F3"), (5.5, 0.6, "D#3"), (6, 2.0, "C3")]
    for rep in range(2):
        for beat, length, name in riff:
            c.note(rep * 8 + beat, length, n(name), 110)
    c.write("14 Hoover Riff.mid")

    # 15 -- Rave stabs: off-beat minor stabs, 90s style.
    c = Clip("Rave Stabs", 132, 4)
    prog = [["C4", "D#4", "G4"], ["C4", "D#4", "G4"], ["A#3", "D4", "F4"], ["G#3", "C4", "D#4"]]
    for bar, ch in enumerate(prog):
        for t in (0.5, 1.5, 2.25, 2.75, 3.5):
            c.chord(bar * 4 + t, 0.18, prog_notes(ch), 112)
    c.write("15 Rave Stabs.mid")

    # 16 -- Wobble bass: sustained low notes for LFO wobble patches.
    c = Clip("Wobble Bass", 140, 4)
    for bar, (name, length) in enumerate([("F1", 3.5), ("F1", 1.75), ("D#1", 3.5), ("C1", 3.5)]):
        c.note(bar * 4, length, n(name), 110)
        if length < 3:
            c.note(bar * 4 + 2, 1.75, n("G#1"), 104)
    c.write("16 Wobble Bass.mid")

    # 17 -- Ambient drone: two long, slowly moving chords (pads, wave sequences, vector).
    c = Clip("Ambient Drone", 60, 8)
    c.chord(0, 16.2, prog_notes(["D2", "A2", "E3", "F#3", "C#4"]), 64)
    c.chord(16, 16.2, prog_notes(["B1", "F#2", "D3", "E3", "A3"]), 60)
    c.note(6, 6, n("F#5"), 50)
    c.note(22, 8, n("E5"), 48)
    c.write("17 Ambient Drone.mid")

    # 18 -- Held chord: one chord for two bars, then another (wave sequences, rhythmic patches).
    c = Clip("Held Chords", 120, 4)
    c.chord(0, 7.9, prog_notes(["C3", "G3", "D4", "E4"]), 90)
    c.chord(8, 7.9, prog_notes(["A2", "E3", "B3", "C4"]), 90)
    c.write("18 Held Chords.mid")

    # 19 -- Choir: slow four-part hymn-like voicing (choirs, voices).
    c = Clip("Choir", 66, 8)
    prog = [["D3", "A3", "D4", "F4"], ["C3", "G3", "C4", "E4"], ["Bb2", "F3", "D4", "F4"], ["A2", "E3", "C#4", "E4"],
            ["D3", "A3", "D4", "F4"], ["G2", "D3", "Bb3", "D4"], ["A2", "E3", "A3", "C#4"], ["D3", "A3", "D4", "F#4"]]
    for bar, ch in enumerate(prog):
        c.chord(bar * 4, 4.05, prog_notes(ch), 72)
    c.write("19 Choir.mid")

    # 20 -- String section: moving inner voices with a melody on top.
    c = Clip("String Section", 84, 8)
    prog = [["C3", "G3", "E4"], ["A2", "E3", "C4"], ["F2", "C3", "A3"], ["G2", "D3", "B3"]] * 2
    melody = ["G4", "E4", "A4", "G4", "C5", "B4", "A4", "G4", "E5", "D5", "C5", "B4", "A4", "C5", "B4", "G4"]
    for bar, ch in enumerate(prog):
        c.chord(bar * 4, 4.0, prog_notes(ch), 70)
        c.note(bar * 4, 2.0, n(melody[bar * 2]), 84)
        c.note(bar * 4 + 2, 2.0, n(melody[bar * 2 + 1]), 80)
    c.write("20 String Section.mid")

    # 21 -- Harp arpeggio: rising and falling 8ths over two octaves (plucks, harps).
    c = Clip("Harp Arpeggio", 96, 4)
    prog = [["C3", "G3", "C4", "E4", "G4", "C5"], ["A2", "E3", "A3", "C4", "E4", "A4"],
            ["F2", "C3", "F3", "A3", "C4", "F4"], ["G2", "D3", "G3", "B3", "D4", "G4"]]
    for bar, ch in enumerate(prog):
        seq = ch + ch[-2:0:-1]
        for i, name in enumerate(seq[:8]):
            c.note(bar * 4 + i * 0.5, 1.5, n(name), 70 + i * 4)
    c.write("21 Harp Arpeggio.mid")

    # 22 -- Jazz chords: ii-V-I voicings with a walking bass (e-pianos, organs).
    c = Clip("Jazz ii-V-I", 110, 4)
    voicings = [["F3", "A3", "C4", "E4"], ["F3", "A3", "B3", "E4"], ["E3", "G3", "B3", "D4"], ["E3", "G3", "A3", "D4"]]
    bass = [["D2", "F2", "A2", "C3"], ["G2", "B2", "D3", "F3"], ["C2", "E2", "G2", "B2"], ["A1", "C2", "E2", "G2"]]
    for bar in range(4):
        c.chord(bar * 4 + 0.5, 1.2, prog_notes(voicings[bar]), 70)
        c.chord(bar * 4 + 2.5, 1.0, prog_notes(voicings[bar]), 64)
        for i, b in enumerate(bass[bar]):
            c.note(bar * 4 + i, 0.9, n(b), 86)
    c.write("22 Jazz ii-V-I.mid")

    # 23 -- Synthwave bass: driving 8ths with octave pops.
    c = Clip("Synthwave Bass", 100, 4)
    for bar, root in enumerate(["A1", "F1", "C2", "G1"]):
        for i in range(8):
            c.note(bar * 4 + i * 0.5, 0.4, n(root) + (12 if i in (3, 7) else 0), 104 if i % 2 == 0 else 90)
    c.write("23 Synthwave Bass.mid")

    # 24 -- Synthwave lead: long expressive notes with bends implied by legato.
    c = Clip("Synthwave Lead", 100, 8)
    for beat, length, name in [(0, 3, "E5"), (3, 1, "D5"), (4, 2, "C5"), (6, 2, "B4"), (8, 3, "A4"), (11, 1, "B4"),
                               (12, 4, "C5"), (16, 2, "E5"), (18, 2, "G5"), (20, 3, "A5"), (23, 1, "G5"), (24, 2, "E5"),
                               (26, 2, "D5"), (28, 4, "E5")]:
        c.note(beat, length + 0.05, n(name), 96)
    c.write("24 Synthwave Lead.mid")

    # 25 -- Techno bass: rolling off-beat 16ths on one note.
    c = Clip("Techno Rumble", 130, 4)
    for bar in range(4):
        for step in range(16):
            if step % 4 != 0:
                c.note(bar * 4 + step * 0.25, 0.16, n("F1") + (3 if bar == 3 and step > 11 else 0), 100 if step % 4 == 2 else 80)
    c.write("25 Techno Rumble.mid")

    # 26 -- House chords: classic off-beat organ / piano chords.
    c = Clip("House Chords", 124, 4)
    prog = [["F3", "A3", "C4", "E4"], ["D3", "F3", "A3", "C4"], ["G3", "Bb3", "D4", "F4"], ["C3", "E3", "G3", "Bb3"]]
    for bar, ch in enumerate(prog):
        for t in (0.5, 1.5, 2.5, 3.25, 3.75):
            c.chord(bar * 4 + t, 0.3, prog_notes(ch), 100)
    c.write("26 House Chords.mid")

    # 27 -- DnB reese: long, low notes for detuned bass patches.
    c = Clip("Reese Bass", 172, 4)
    for beat, length, name in [(0, 2.5, "E1"), (2.5, 1.5, "G1"), (4, 3.0, "D1"), (7, 1.0, "E1"),
                               (8, 2.5, "C1"), (10.5, 1.5, "D1"), (12, 4.0, "B0")]:
        c.note(beat, length, n(name), 108)
    c.write("27 Reese Bass.mid")

    # 28 -- Chiptune: fast arpeggiated chords and a square-wave melody.
    c = Clip("Chiptune", 150, 4)
    prog = [["C4", "E4", "G4"], ["A3", "C4", "E4"], ["F3", "A3", "C4"], ["G3", "B3", "D4"]]
    for bar, ch in enumerate(prog):
        for step in range(16):
            c.note(bar * 4 + step * 0.25, 0.2, n(ch[step % 3]) + 12, 90)
    c.write("28 Chiptune.mid")

    # 29 -- 80s pop chords: bright stabs and holds (polysynths, JP-8000, brass).
    c = Clip("80s Pop", 116, 4)
    prog = [["E3", "G#3", "B3", "E4"], ["C#3", "E3", "G#3", "C#4"], ["A2", "C#3", "E3", "A3"], ["B2", "D#3", "F#3", "B3"]]
    for bar, ch in enumerate(prog):
        c.chord(bar * 4, 1.4, prog_notes(ch), 104)
        c.chord(bar * 4 + 1.5, 0.4, prog_notes(ch), 90)
        c.chord(bar * 4 + 2.5, 1.4, prog_notes(ch), 96)
    c.write("29 80s Pop.mid")

    # 30 -- Octave bass: disco octaves in 8ths.
    c = Clip("Disco Octaves", 120, 4)
    for bar, root in enumerate(["E1", "E1", "A1", "B1"]):
        for i in range(8):
            c.note(bar * 4 + i * 0.5, 0.3, n(root) + (12 if i % 2 else 0), 104)
    c.write("30 Disco Octaves.mid")

    # 31 -- Funk bass: syncopated 16ths with ghost notes.
    c = Clip("Funk Bass", 104, 4)
    pattern = [(0, "E1", 120), (0.75, "E1", 60), (1.0, "E2", 100), (1.5, "D2", 90), (1.75, "B1", 96), (2.5, "G1", 104),
               (2.75, "A1", 96), (3.25, "B1", 110), (3.5, "E1", 70)]
    for bar in range(4):
        for t, name, vel in pattern:
            c.note(bar * 4 + t, 0.2, n(name) + (5 if bar == 2 else 0), vel)
    c.write("31 Funk Bass.mid")

    # 32 -- Slow arpeggio: a dreamy 8th-note arpeggio (ambient keys, bells).
    c = Clip("Slow Arpeggio", 80, 8)
    prog = [["D4", "F4", "A4", "C5"], ["Bb3", "D4", "F4", "A4"], ["F3", "A3", "C4", "E4"], ["C4", "E4", "G4", "B4"]]
    for bar in range(8):
        ch = prog[(bar // 2) % 4]
        for i in range(8):
            c.note(bar * 4 + i * 0.5, 1.8, n(ch[[0, 1, 2, 3, 2, 3, 1, 2][i]]), 64 + (i % 4) * 6)
    c.write("32 Slow Arpeggio.mid")

    # 33 -- Film swell: low drone with a rising cluster (cinematic pads, brass).
    c = Clip("Film Swell", 70, 8)
    c.note(0, 32, n("C1"), 90)
    c.note(0, 32, n("C2"), 80)
    for i, (beat, name) in enumerate([(4, "G3"), (8, "C4"), (12, "D4"), (16, "Eb4"), (20, "G4"), (24, "Bb4")]):
        c.note(beat, 32 - beat, n(name), 50 + i * 8)
    c.write("33 Film Swell.mid")

    # 34 -- Pentatonic melody: a call-and-response line for leads and flutes.
    c = Clip("Pentatonic Melody", 92, 8)
    for beat, length, name in [(0, 0.5, "A4"), (0.5, 0.5, "C5"), (1, 1, "D5"), (2, 2, "E5"), (4, 0.5, "G5"), (4.5, 0.5, "E5"),
                               (5, 1, "D5"), (6, 2, "C5"), (8, 1, "A4"), (9, 1, "G4"), (10, 2, "A4"),
                               (16, 0.5, "E5"), (16.5, 0.5, "G5"), (17, 1, "A5"), (18, 2, "G5"), (20, 1, "E5"), (21, 1, "D5"),
                               (22, 2, "C5"), (24, 1, "D5"), (25, 1, "C5"), (26, 2, "A4"), (28, 4, "A4")]:
        c.note(beat, length, n(name), 92)
    c.write("34 Pentatonic Melody.mid")

    # 35 -- Big piano chords: wide two-handed voicings with the pedal.
    c = Clip("Big Chords", 72, 4)
    prog = [("C2", ["G3", "C4", "E4", "G4", "C5"]), ("A1", ["E3", "A3", "C4", "E4", "A4"]),
            ("F1", ["C3", "F3", "A3", "C4", "F4"]), ("G1", ["D3", "G3", "B3", "D4", "G4"])]
    for bar, (bass, ch) in enumerate(prog):
        c.cc(bar * 4, 64, 127)
        c.note(bar * 4, 3.8, n(bass), 96)
        c.note(bar * 4, 3.8, n(bass) + 12, 88)
        c.chord(bar * 4, 3.8, prog_notes(ch), 84, strum=0.02)
        c.cc(bar * 4 + 3.95, 64, 0)
    c.write("35 Big Chords.mid")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    clips()
    more_clips()
    print("\n".join(sorted(os.listdir(OUT))))
