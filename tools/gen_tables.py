#!/usr/bin/env python3
"""ETCH II -- A Generative Synthesis Machine -- by Synth Centric.

Generates every DSP lookup table used by the audio engine (original work):
band-limited oscillator tables, wavetable banks, sine, pitch/filter/envelope
coefficient tables and the soft-clip curve.  Rate-dependent tables are built
for the mixer rate passed on the command line.

usage: gen_tables.py SAMPLES_PER_FRAME BLOCK OUT.c OUT.h
"""
import math
import sys

import numpy as np

SPF = int(sys.argv[1])
CTRL = int(sys.argv[2])         # samples per control block
OUT_C, OUT_H = sys.argv[3], sys.argv[4]
CYCLES_PER_FRAME = 280896
CPU = 16777216
TIMER_RELOAD = CYCLES_PER_FRAME // SPF
assert TIMER_RELOAD * SPF == CYCLES_PER_FRAME, "SPF must divide a frame exactly"
assert SPF % CTRL == 0 and CTRL % 4 == 0, "the block must divide the frame"
FS = CPU / TIMER_RELOAD
FC = FS / CTRL                 # control rate
TL = 256                       # wavetable length
OTL = 1024                     # band-limited oscillator table length

c_out, h_out = [], []


def arr(ctype, name, vals, per_line=16, section=""):
    vals = [int(v) for v in vals]
    h_out.append("extern const %s %s[%d];" % (ctype, name, len(vals)))
    c_out.append("const %s %s[%d]%s = {" % (ctype, name, len(vals), section))
    for i in range(0, len(vals), per_line):
        c_out.append("  " + ",".join(str(v) for v in vals[i:i + per_line]) + ",")
    c_out.append("};")


def q(x, lo, hi):
    return max(lo, min(hi, int(round(x))))


# ---- constants ---------------------------------------------------------------
h_out.append("#define MIX_SPF %d" % SPF)
h_out.append("#define MIX_TIMER_RELOAD %d" % TIMER_RELOAD)
h_out.append("#define MIX_RATE %d   /* %.3f Hz */" % (round(FS), FS))
h_out.append("#define MIX_BLOCK %d" % CTRL)
h_out.append("#define MIX_BLOCKS_PER_FRAME %d" % (SPF // CTRL))
h_out.append("#define OSC_BANDS 11")

# ---- sine (Q15) -------------------------------------------------------------
arr("int16_t", "tab_sine", [q(32767 * math.sin(2 * math.pi * i / 1024), -32767, 32767) for i in range(1024)])

# ---- pitch: one octave from MIDI 120 in 1/64 semitone steps, 32-bit phase ---
arr("uint32_t", "tab_exp_inc",
    [q(2 ** 32 * 440.0 * 2 ** ((120 + r / 768.0 * 12 - 69) / 12.0) / FS, 0, 2 ** 32 - 1) for r in range(768)], 8)

# ---- band-limited oscillators: saw, square, triangle; band k is safe for
#      fundamentals up to MIDI 12*(k+1) ----------------------------------------
def bl_table(kind, top_midi):
    f_top = 440.0 * 2 ** ((top_midi - 69) / 12.0)
    nmax = max(1, int((FS * 0.5) / f_top))
    x = np.arange(OTL) * 2 * np.pi / OTL
    y = np.zeros(OTL)
    for n in range(1, min(nmax, 96) + 1):
        roll = 0.5 + 0.5 * math.cos(math.pi * n / (nmax + 1))   # gentle top roll-off, no Gibbs ringing
        if kind == "saw":
            y += roll * np.sin(n * x) / n
        elif kind == "square" and n % 2:
            y += roll * np.sin(n * x) / n
        elif kind == "tri" and n % 2:
            y += roll * ((-1) ** ((n - 1) // 2)) * np.sin(n * x) / (n * n)
    return y


for kind in ("saw", "square", "tri"):
    vals = []
    peak = max(np.max(np.abs(bl_table(kind, 12 * (k + 1)))) for k in range(11))
    for k in range(11):
        y = bl_table(kind, 12 * (k + 1)) / peak
        vals += [q(30000 * v, -32767, 32767) for v in y]
    arr("int16_t", "tab_bl_" + kind, vals)

# ---- wavetable banks: 8 banks x 8 frames x 3 mip levels x 256, int8 --------
#      Each bank is an original additive/formant design; frames morph smoothly.
WT_NAMES = ["GLASS", "VOICE", "METAL", "PULSAR", "ORGAN", "DIGIT", "RUST", "HOLLOW"]
MIP_H = [48, 16, 6]


def wt_spectrum(bank, frame):
    t = frame / 7.0
    amps = np.zeros(97)
    ph = np.zeros(97)
    rng = np.random.RandomState(1000 + bank)
    rph = rng.uniform(0, 2 * np.pi, 97)
    for n in range(1, 97):
        if bank == 0:      # GLASS: sine -> bright odd/even bell partials
            amps[n] = (1.0 if n == 1 else 0) + t * (0.6 / n ** 0.7) * (1 if n in (2, 3, 5, 7, 9, 12, 16) else 0.15)
        elif bank == 1:    # VOICE: moving vowel formant
            f1 = 2.5 + 7 * t
            f2 = 9 + 10 * (1 - t)
            amps[n] = (1 / n ** 0.5) * (math.exp(-((n - f1) / 1.8) ** 2) + 0.6 * math.exp(-((n - f2) / 3.0) ** 2) + 0.05)
        elif bank == 2:    # METAL: inharmonic-feeling clusters
            amps[n] = (1 / n ** 0.6) * (1 + math.cos(n * (0.9 + 2.2 * t))) * 0.5 + (0.3 if n == 1 else 0)
        elif bank == 3:    # PULSAR: pulse width sweep
            w = 0.5 - 0.46 * t
            amps[n] = abs(math.sin(math.pi * n * w)) / n
        elif bank == 4:    # ORGAN: drawbar registrations
            bars = {1: 1, 2: 0.7 * t, 3: 0.8 * (1 - t) + 0.2, 4: 0.5 * t, 6: 0.4 * t * t, 8: 0.35 * t}
            amps[n] = bars.get(n, 0.0)
        elif bank == 5:    # DIGIT: sync-like harsh digital spectra
            r = 1.5 + 4.5 * t
            amps[n] = abs(math.sin(math.pi * r)) * abs(1.0 / (n - r) if abs(n - r) > 0.05 else 10) / n ** 0.3 * 0.3
        elif bank == 6:    # RUST: noisy, gritty partials (fixed random phases)
            amps[n] = (1 / n ** (1.2 - 0.6 * t)) * (0.6 + 0.4 * math.sin(n * 1.7 + bank))
            ph[n] = rph[n] * t
        else:              # HOLLOW: square-ish with notch moving through
            amps[n] = (1.0 / n if n % 2 else 0.25 * t / n) * (0.3 + 0.7 * abs(math.sin(math.pi * n / (3 + 6 * t))))
    return amps, ph


wt_vals = []
for b in range(8):
    for f in range(8):
        amps, ph = wt_spectrum(b, f)
        for hmax in MIP_H:
            x = np.arange(TL) * 2 * np.pi / TL
            y = np.zeros(TL)
            for n in range(1, hmax + 1):
                y += amps[n] * np.sin(n * x + ph[n])
            y -= y.mean()
            pk = np.max(np.abs(y)) or 1
            wt_vals += [q(127 * v / pk, -127, 127) for v in y]
arr("int8_t", "tab_wt", wt_vals, 32)
h_out.append("#define WT_BANKS 8\n#define WT_FRAMES 8\n#define WT_MIPS 3")
h_out.append("extern const char * const wt_names[8];")
c_out.append("const char * const wt_names[8] = {%s};" % ",".join('"%-6s"' % n for n in WT_NAMES))

# ---- SVF coefficient, 2x oversampled, quarter-semitone index from MIDI 0 -----
FMAX = 0.17 * 2 * FS
arr("uint16_t", "tab_svf_f",
    [q(32768 * 2 * math.sin(math.pi * min(440 * 2 ** ((i / 4.0 - 69) / 12), FMAX) / (2 * FS)), 1, 65535) for i in range(4 * 136)])

# ---- 1x SVF coefficient (quarter-semitone index), capped for stability ------
FMAX1 = 0.19 * FS
arr("uint16_t", "tab_svf1_f",
    [q(32768 * 2 * math.sin(math.pi * min(440 * 2 ** ((i / 4.0 - 69) / 12), FMAX1) / FS), 1, 65535) for i in range(4 * 136)])

# ---- resonator delay length (Q16 samples) for the octave below MIDI 12 -------
#      len(p) = tab_res_len[r] >> o   (o = octave index of the pitch)
arr("uint32_t", "tab_res_len", [q((FS / (440.0 * 2 ** ((r / 768.0 * 12 - 69) / 12.0))) * 65536 / 1.0, 0, 2 ** 32 - 1) for r in range(768)], 8)

# ---- reciprocal: tab_recip[i] = 2^31 / (256 + i), i = 0..767 (Q31 / Q8 mantissa)
arr("uint32_t", "tab_recip", [q(2 ** 31 / (256 + i), 0, 2 ** 32 - 1) for i in range(768)], 8)

# ---- envelope decay multipliers per control block (Q16), 32 steps 2ms..8s --
def tau(i, lo=0.002, hi=8.0, n=32):
    return lo * (hi / lo) ** (i / (n - 1))


arr("uint16_t", "tab_env_decay", [q(65536 * math.exp(-1.0 / (tau(i) * FC)), 0, 65535) for i in range(32)])
# linear attack increment per control block (Q24 full scale), 32 steps 0.5ms..4s
arr("uint32_t", "tab_env_attack", [q((1 << 24) / max(1.0, tau(i, 0.0005, 4.0) * FC), 1, 1 << 24) for i in range(32)], 8)
# glide: per-block fraction of the remaining pitch distance (Q16), 16 steps
arr("uint16_t", "tab_glide", [q(65536 * (1 - math.exp(-1.0 / (tau(i, 0.004, 1.2, 16) * FC))), 1, 65535) for i in range(16)])
# LFO: phase increment per control block (32-bit), 32 rates 0.03..24 Hz
arr("uint32_t", "tab_lfo_inc", [q(2 ** 32 * (0.03 * (800 ** (i / 31.0))) / FC, 1, 2 ** 31) for i in range(32)], 8)

# ---- soft clip: tanh over [-4, 4), 1024 points, Q15 -------------------------
arr("int16_t", "tab_tanh", [q(32767 * math.tanh((i - 512) / 128.0), -32767, 32767) for i in range(1024)])

hdr = ["/* AUTOGENERATED by tools/gen_tables.py %d -- do not edit. */" % SPF,
       "#ifndef ETCH2_TABLES_H", "#define ETCH2_TABLES_H", "#include <stdint.h>"] + h_out + ["#endif"]
open(OUT_H, "w").write("\n".join(hdr) + "\n")
src = ["/* AUTOGENERATED by tools/gen_tables.py %d -- do not edit. */" % SPF,
       "/* ETCH II -- A Generative Synthesis Machine -- by Synth Centric */",
       '#include "tables.h"'] + c_out
open(OUT_C, "w").write("\n".join(src) + "\n")
print("tables: SPF=%d rate=%.1f Hz timer=%d" % (SPF, FS, TIMER_RELOAD))
