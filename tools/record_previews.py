#!/usr/bin/env python3
"""ETCH II -- A Generative Synthesis Machine -- by Synth Centric.

Records the release audio from the actual ROM running in mGBA (headless):
  release/audio/album_preview.wav          ~25 s of each of the 8 pieces
  release/audio/synthesis_examples/*.wav   each engine and the percussion
All audio is the ROM's own Direct Sound output, captured by the emulator
(48 kHz stereo 16-bit; the GBA itself mixes at 15 768 Hz, 8-bit).

usage: record_previews.py ROM OUTDIR
"""
import os
import struct
import sys
import wave

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tests", "harness"))
sys.path.insert(0, os.path.join(ROOT, "tests"))
from emu import Emu  # noqa: E402

ROM, OUT = sys.argv[1], sys.argv[2]
ELF = ROM.replace(".gba", ".elf")
os.makedirs(os.path.join(OUT, "synthesis_examples"), exist_ok=True)
FPS = 16777216 / 280896.0
PAT = int(open(os.path.join(ROOT, "build", "gen", "album.c")).read().split("offsetof(project2_t, pat) == ")[1].split(",")[0])
XPAT = PAT + 64 * 32
NAMES = ["OXIDE", "FERRIC", "VESSEL", "DEBRIS", "STATIC", "PATINA", "FRACTURE", "REMNANT"]


def read(path):
    w = wave.open(path)
    return np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2).astype(np.float64), w.getframerate()


def write(path, x, sr):
    w = wave.open(path, "wb")
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
    w.writeframes(np.clip(x, -32767, 32767).astype(np.int16).tobytes())
    w.close()


tmp = os.path.join(ROOT, "build", "rec_tmp.wav")
e = Emu(ROM, ELF)
e.run(90); e.press("start"); e.run(30)

# ---- album preview: 25 s of each piece, 1.5 s crossfades ------------------------------
parts = []
for i in range(8):
    e.poke("probe_album", bytes([i + 1]))
    e.run(int(FPS * 6))                       # let the drum renders settle, start of piece
    e.wav_start(tmp); e.run(int(FPS * 25)); e.wav_stop()
    x, sr = read(tmp)
    write(os.path.join(OUT, "synthesis_examples", "piece_%d_%s.wav" % (i + 1, NAMES[i].lower())), x, sr)
    parts.append(x)
fade = int(sr * 1.5)
out = parts[0]
for p in parts[1:]:
    r = np.linspace(0, 1, fade)[:, None]
    out = np.concatenate([out[:-fade], out[-fade:] * (1 - r) + p[:fade] * r, p[fade:]])
write(os.path.join(OUT, "album_preview.wav"), out, sr)
print("album_preview.wav: %.1f s" % (len(out) / sr))

# ---- per-engine examples: OXIDE's core groove, BASS played by each engine --------------
P = e.sym("proj")
e.poke("probe_album", bytes([1])); e.run(int(FPS * 3))
e.poke(P + 10, bytes([0]))                    # CLASSIC: the ETCH core only
e.poke(P + 18 + 9, bytes([0]))                # STABLE: the same notes every time
examples = [
    ("01_subtractive_acid", 0, [0, 0, 6, 13, 13, 5, 0, 9, 12, 2, 13, 6, 4, 2, 8]),
    ("02_fm_metallic", 1, [3, 9, 14, 0, 13, 5, 0, 9, 10, 2, 10, 4, 1, 2, 8]),
    ("03_wavetable_vocal", 2, [1, 0, 10, 12, 12, 7, 0, 10, 12, 3, 8, 4, 1, 2, 8]),
    ("04_phase_distortion_reso", 3, [3, 0, 10, 0, 14, 6, 0, 9, 10, 2, 10, 4, 2, 2, 8]),
    ("05_resonator_pluck", 4, [0, 0, 20, 11, 6, 6, 0, 9, 4, 4, 6, 0, 1, 2, 8]),
]
for name, eng, pv in examples:
    base = P + 106 + 24 * 0
    e.poke(base, bytes([eng] + pv))
    e.wav_start(tmp); e.run(int(FPS * 12)); e.wav_stop()
    x, sr = read(tmp)
    write(os.path.join(OUT, "synthesis_examples", name + ".wav"), x, sr)
    print(name)

# ---- percussion: each lane, each kit, in turn ------------------------------------------
e.press("start"); e.run(5)
cur = e.u8("seq_cur")
pb = P + PAT + 64 * cur
e.poke(P + 15, bytes([0x01 | 0x02 | 0x04 | 0x08 | 0x80]))      # only the drum tracks
e.poke(pb + 60, bytes([0]))
e.press("start"); e.run(10)
chunks = []
for lane in range(4):
    for kit in range(4):
        mask = [0, 0, 0, 0]
        mask[lane] = 0x1111 if lane != 3 else 0x0101
        e.poke(pb + 48, b"".join(struct.pack("<H", v) for v in mask))
        e.poke(pb + 56 + lane, bytes([kit]))
        e.run(30)
        e.wav_start(tmp); e.run(int(FPS * 1.8)); e.wav_stop()
        chunks.append(read(tmp)[0])
write(os.path.join(OUT, "synthesis_examples", "06_percussion_16_kits.wav"), np.concatenate(chunks), sr)
print("06_percussion_16_kits")
e.close()
os.remove(tmp)
