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


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    clips()
    print("\n".join(sorted(os.listdir(OUT))))
