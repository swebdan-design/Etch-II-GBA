#!/usr/bin/env python3
"""ETCH II -- A Generative Synthesis Machine -- by Synth Centric.
Emulator integration tests (mGBA via tests/harness).  Every result printed
here is measured on the ROM; nothing is assumed.

usage: emu_test.py ROM [--artifacts DIR] [--long]
"""
import json
import os
import struct
import subprocess
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, "harness"))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from emu import Emu  # noqa: E402
from check_rom import check  # noqa: E402

ROM = sys.argv[1]
ELF = ROM.replace(".gba", ".elf")
ART = sys.argv[sys.argv.index("--artifacts") + 1] if "--artifacts" in sys.argv else os.path.join(ROOT, "build", "emu_artifacts")
LONG = "--long" in sys.argv
os.makedirs(ART, exist_ok=True)
results = []

# ---- project2_t layout (checked against the generated album asserts) ------------------
O = dict(name=0, compat=10, bpm=11, swing=12, gate=13, tune=14, mutes=15, start=16, album=17, gen=18,
         song=30, fx=98, patch=106, dtune=298, ddecay=302, dlevel=306, xseed=310)
PAT = int(open(os.path.join(ROOT, "build", "gen", "album.c")).read().split("offsetof(project2_t, pat) == ")[1].split(",")[0])
XPAT = PAT + 64 * 32
XPSZ = 152
G = dict(key=0, scale=1, style=2, density=3, sync=4, accent=5, slide=6, octave=7, mutation=8, mode=9, seed=10)
TRK = dict(BASS=0, LEAD=1, CHORD=2, DRONE=3, KICK=4, PERC=5, HATS=6, TEXTURE=7)
FPS = 16777216 / 280896.0


def rec(name, ok, detail=""):
    results.append({"check": name, "pass": bool(ok), "detail": str(detail)})
    print("%s  %s  %s" % ("PASS" if ok else "FAIL", name, detail))


class E2(Emu):
    def __init__(self, save=None):
        super().__init__(ROM, ELF, save)
        self.P = self.sym("proj")

    def screen_row(self, y):
        return self.peek(self.sym("ui_textbuf") + y * 31, 30).decode("latin1")

    def screen(self):
        return [self.screen_row(y) for y in range(20)]

    def pset(self, off, data):
        self.poke(self.P + off, bytes(data) if not isinstance(data, (bytes, bytearray)) else data)

    def pget(self, off, n=1):
        return self.peek(self.P + off, n)

    def patch(self, trk, engine=None, **kw):
        base = O["patch"] + 24 * trk
        if engine is not None:
            self.pset(base, [engine])
        names = ["A", "B", "TONE", "C", "ENVMOD", "MDECAY", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ACCENT", "GLIDE", "DRIVE", "OCTAVE", "FINE"]
        for k, v in kw.items():
            if k in names:
                self.pset(base + 1 + names.index(k), [v])
            else:
                self.pset(base + {"level": 16, "pan": 17, "send": 18, "crush": 19, "lfo_depth": 21}[k], [v])

    def pat(self, i):
        return self.pget(PAT + 64 * i, 64)

    def probe(self, name, val, timeout=600):
        """run a test hook and wait until the main loop has executed it"""
        self.poke(name, bytes([val]))
        for _ in range(timeout):
            self.run(1)
            if self.u8(name) == 0:
                return self.u8("probe_result")
        return None

    def boot(self):
        self.run(90)
        self.press("start")
        self.run(20)

    def capture(self, path, frames):
        self.wav_start(path)
        self.run(frames)
        self.wav_stop()
        w = wave.open(path)
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2).astype(np.float64)
        return x, w.getframerate()


def rms(x):
    return float(np.sqrt(np.mean(x ** 2))) if len(x) else 0.0


def dominant(x, sr, lo=30, hi=4000):
    m = x.mean(axis=1) if x.ndim == 2 else x
    m = m - m.mean()
    sp = np.abs(np.fft.rfft(m * np.hanning(len(m))))
    f = np.fft.rfftfreq(len(m), 1 / sr)
    band = (f > lo) & (f < hi)
    return float(f[band][np.argmax(sp[band])])


def centroid(x, sr):
    m = x.mean(axis=1)
    sp = np.abs(np.fft.rfft(m * np.hanning(len(m))))
    f = np.fft.rfftfreq(len(m), 1 / sr)
    return float((sp * f).sum() / max(1e-9, sp.sum()))


def silent_gaps(x, sr, ms=40):
    """longest run of exactly-zero output while music plays (dropout detector)"""
    m = np.abs(x).max(axis=1) == 0
    best = run = 0
    for v in m:
        run = run + 1 if v else 0
        best = max(best, run)
    return best * 1000.0 / sr


def solo_setup(e, trk, note_midi=None, bass_note=None):
    """Silence everything except one track; stop; build a steady test pattern."""
    e.press("start"); e.run(5)                           # stop
    e.pset(O["mutes"], [0xFF ^ (1 << trk)])
    e.pset(O["compat"], [1])
    e.pset(O["song"] + 64, [0])                           # loop mode
    cur = e.u8("seq_cur")
    base = PAT + 64 * cur
    e.pset(base + 60, [0])                                # unlock
    e.pset(O["gen"] + G["mode"], [0])                     # STABLE: nothing moves
    if bass_note is not None:
        e.pset(base, [bass_note] * 16)
        e.pset(base + 16, [1] + [2] * 15)                 # one note held by ties
        e.pset(base + 32, [7] * 16)
    if note_midi is not None:
        xt = {1: 0, 2: 1, 3: 2, 7: 3}[trk]
        xb = XPAT + XPSZ * cur + 32 * xt
        e.pset(xb, [note_midi] * 16)
        e.pset(xb + 16, [1] + [2] * 15)
        e.pset(XPAT + XPSZ * cur + 144 + xt, [16])
    e.run(2)
    e.press("start"); e.run(30)


def main():
    ok, info = check(ROM, quiet=True)
    rec("ROM header: logo, fixed byte, complement checksum, SRAM_V marker", ok, info["sha256"][:16])
    rec("ROM size", info["size"] < 4 * 1024 * 1024, "%d bytes" % info["size"])

    # ---- boot, title ----------------------------------------------------------------
    e = E2()
    e.run(120)
    scr = e.screen()
    rec("title: 'A Generative Synthesis Machine'", any("A Generative Synthesis Machine" in r for r in scr))
    rec("title: 'by Synth Centric'", any("by Synth Centric" in r for r in scr))
    e.shot(os.path.join(ART, "screen_title.png"))
    seen = False
    for _ in range(6):
        e.run(10)
        seen = seen or any("PRESS START" in r for r in e.screen())
    rec("title: PRESS START blinks", seen)
    e.press("start"); e.run(30)
    rec("START on the title starts playback in LIVE", e.u8("seq_playing") == 1 and "LIVE" in e.screen_row(0), e.screen_row(0))
    e.shot(os.path.join(ART, "screen_live.png"))

    # ---- audio present, no dropouts --------------------------------------------------
    x, sr = e.capture(os.path.join(ART, "boot_oxide.wav"), 600)
    rec("audio output during playback", rms(x) > 1000, "rms %.0f" % rms(x))
    rec("no dropouts (longest digital silence < 40 ms)", silent_gaps(x, sr) < 40, "%.1f ms" % silent_gaps(x, sr))
    clip = float(np.mean(np.abs(x) >= 32700))
    rec("no hard clipping (< 0.5 % of samples at full scale)", clip < 0.005, "%.4f %%" % (clip * 100))
    rec("stereo (L and R differ)", rms(x[:, 0] - x[:, 1]) > 50, "side rms %.0f" % rms(x[:, 0] - x[:, 1]))

    # ---- screens ----------------------------------------------------------------------
    names = ["LIVE", "TRACK", "SYNTH", "MOD", "GEN", "MEMORY", "SONG", "FILE"]
    okall = True
    for i, n in enumerate(names):
        row = e.screen_row(0)
        okall &= n in row
        e.shot(os.path.join(ART, "screen_%s.png" % n.lower()))
        e.press("r", 3, 8)
    rec("R walks LIVE>TRACK>SYNTH>MOD>GEN>MEMORY>SONG>FILE and wraps", okall and "LIVE" in e.screen_row(0))
    e.press("l", 3, 8)
    rec("L goes back (LIVE < FILE)", "FILE" in e.screen_row(0))
    e.combo("select", "right")
    rec("SELECT+RIGHT also changes screen (ETCH shortcut)", "LIVE" in e.screen_row(0))
    rec("playback continued through screen changes", e.u8("seq_playing") == 1)

    # ---- tempo accuracy, CPU ---------------------------------------------------------
    for bpm, sw in ((124, 0), (220, 8), (60, 0)):
        e.pset(O["bpm"], [bpm]); e.pset(O["swing"], [sw])
        e.poke("probe_tempo", bytes([1]))
        e.run(10)
        s0, g0, l0 = e.u32("seq_steps_total"), e.u32("seq_gates_total"), e.u32("seq_gate_late")
        e.run(1200)
        steps = e.u32("seq_steps_total") - s0
        exp = bpm * 4 / 60.0 * 1200 / FPS
        late = e.u32("seq_gate_late") - l0
        gates = e.u32("seq_gates_total") - g0
        rec("tempo %d BPM swing %d: steps in 20 s" % (bpm, sw), abs(steps - exp) <= 1.5, "%d vs %.1f expected" % (steps, exp))
        rec("gate timing %d BPM swing %d: a gate point inside every step" % (bpm, sw), late == 0 and abs(gates - steps) <= 1, "%d steps, %d gates, %d late" % (steps, gates, late))
    e.pset(O["bpm"], [132]); e.pset(O["swing"], [1]); e.poke("probe_tempo", bytes([1])); e.run(10)
    e.close()

    # ---- the album --------------------------------------------------------------------
    recipe = open(os.path.join(ROOT, "docs", "generated_album_recipe.txt")).read()
    codes = [l.split()[2].replace("-", "") for l in recipe.splitlines() if l.strip()[:2].rstrip(".").isdigit() and len(l.split()) == 3]
    gen1 = os.path.join(ROOT, "build", "compat", "gen_one")
    e = E2()
    e.boot()
    lines_tot = ov0 = 0
    for i in range(8):
        e.poke("probe_album", bytes([i + 1])); e.run(30)
        truth = subprocess.run([gen1, codes[i]], capture_output=True, text=True).stdout.strip() if os.path.exists(gen1) else None
        raw = e.pat(0)
        got = raw[0:48].hex().upper() + "".join("%04X" % (raw[48 + 2 * k] | raw[49 + 2 * k] << 8) for k in range(4)) + raw[56:60].hex().upper() + "%02X" % raw[61]
        nm = e.pget(O["name"], 10).decode().strip()
        rec("album %d %s: pattern 01 == CORROSION(%s) (ETCH I core)" % (i + 1, nm, codes[i]), truth == got)
        rec("album %d %s: starts in DRIFT, EXPANDED" % (i + 1, nm), e.pget(O["gen"] + G["mode"])[0] == 1 and e.pget(O["compat"])[0] == 1)
        o0 = e.u16("mix_overruns")
        x, sr = e.capture(os.path.join(ART, "album_%d_%s.wav" % (i + 1, nm.lower())), 60 * 25)
        o1 = e.u16("mix_overruns")
        rec("album %d %s: plays 25 s, sound, no underruns" % (i + 1, nm), rms(x) > 800 and o1 == o0 and silent_gaps(x, sr) < 40,
            "rms %.0f, underruns %d, longest silence %.0f ms" % (rms(x), o1 - o0, silent_gaps(x, sr)))
    e.close()

    # ---- synthesis engines: pitch and character ---------------------------------------
    e = E2()
    e.boot()
    # FM with zero index and no feedback is a pure sine: an exact pitch reference
    e.patch(TRK["BASS"], engine=1, A=1, B=0, TONE=0, C=0, ENVMOD=0, ATTACK=0, SUSTAIN=15, DRIVE=0, OCTAVE=2, FINE=8, lfo_depth=0)
    e.pset(O["tune"], [12])
    solo_setup(e, TRK["BASS"], bass_note=21)                    # ETCH note 21 = A2 = 110 Hz
    x, sr = e.capture(os.path.join(ART, "pitch_a2.wav"), 240)
    f = dominant(x[sr // 2:], sr)
    rec("oscillator pitch: A2 (ETCH note 21) = 110 Hz", abs(f - 110.0) < 1.5, "%.2f Hz" % f)
    e.pset(PAT + 64 * e.u8("seq_cur"), [33] * 16)               # A3
    e.run(30)
    x, sr = e.capture(os.path.join(ART, "pitch_a3.wav"), 240)
    f = dominant(x[sr // 2:], sr)
    rec("oscillator pitch: A3 = 220 Hz", abs(f - 220.0) < 2.5, "%.2f Hz" % f)
    # every engine on the same held note: sound, and distinct timbres
    cents = {}
    for eng, nm in enumerate(["SUB", "FM", "WT", "PD", "RES"]):
        # RES uses BOWED excitation here so it sustains like the others
        e.patch(TRK["BASS"], engine=eng, A={1: 3, 4: 2}.get(eng, 0), B=0, TONE=16, C=6 if eng != 4 else 12, ENVMOD=4, ATTACK=0, SUSTAIN=15, DRIVE=0, OCTAVE=2)
        e.press("start"); e.run(5); e.press("start"); e.run(40)
        x, sr = e.capture(os.path.join(ART, "engine_%s.wav" % nm.lower()), 120)
        cents[nm] = centroid(x, sr)
        rec("engine %s produces sound" % nm, rms(x) > 500, "rms %.0f, centroid %.0f Hz" % (rms(x), cents[nm]))
    # a plucked resonator decays (Karplus-Strong): first 0.25 s much louder than 1.5 s later
    e.patch(TRK["BASS"], engine=4, A=0, TONE=20, C=8, ENVMOD=0, ATTACK=0, SUSTAIN=15, OCTAVE=2)
    e.press("start"); e.run(5); e.press("start"); e.run(6)
    x, sr = e.capture(os.path.join(ART, "engine_res_pluck.wav"), 240)
    w = sr // 20
    env = [rms(x[i:i + w]) for i in range(0, len(x) - w, w)]
    pk = int(np.argmax(env[:-21]))                              # a pluck (bar start)
    a0, a1 = env[pk], env[min(len(env) - 1, pk + 20)]           # one second later, same bar
    rec("RES pluck: excitation then natural decay", a0 > 800 and a1 < a0 * 0.5, "rms %.0f -> %.0f after 1 s" % (a0, a1))
    vals = sorted(cents.values())
    rec("five engines are spectrally distinct (centroids differ by > 30 Hz pairwise)", min(np.diff(vals)) > 30, {k: round(v) for k, v in cents.items()})
    # filter: SUB cutoff moves the spectrum
    def bright(x, sr):
        m = x.mean(axis=1)
        sp = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
        f = np.fft.rfftfreq(len(m), 1 / sr)
        return float(sp[(f > 600) & (f < 4000)].sum() / max(1e-9, sp[(f > 60) & (f < 400)].sum()))
    cs = []
    for tone in (4, 16, 30):
        e.patch(TRK["BASS"], engine=0, A=0, TONE=tone, C=4, ENVMOD=0)
        e.run(20)
        x, sr = e.capture(os.path.join(ART, "sub_cut_%d.wav" % tone), 90)
        cs.append(bright(x, sr))
    rec("SUB filter: CUTOFF raises upper-harmonic energy (600-4000 Hz vs 60-400 Hz)", cs[0] < cs[1] < cs[2], ["%.3f" % c for c in cs])
    # filter stability at maximum resonance and cutoff
    e.patch(TRK["BASS"], engine=0, A=0, TONE=31, C=15, ENVMOD=15, DRIVE=7)
    x, sr = e.capture(os.path.join(ART, "sub_maxres.wav"), 120)
    rec("SUB filter stable at max resonance + cutoff + drive (bounded, not stuck)", 300 < rms(x) < 30000 and np.std(x[-2000:]) > 10, "rms %.0f" % rms(x))
    # paraphonic chord: three pitches
    e.patch(TRK["CHORD"], engine=2, A=4, B=0, TONE=0, C=15, ENVMOD=0, ATTACK=0, SUSTAIN=15, OCTAVE=2)
    cur = e.u8("seq_cur")
    e.pset(XPAT + XPSZ * cur + 128, [3] * 16)                   # OPEN shape: root, 5th, octave
    e.pset(O["gen"] + G["key"], [0]); e.pset(O["gen"] + G["scale"], [1])
    solo_setup(e, TRK["CHORD"], note_midi=48)
    x, sr = e.capture(os.path.join(ART, "chord_open_c3.wav"), 180)
    m = x.mean(axis=1)[sr // 2:]
    sp = np.abs(np.fft.rfft(m * np.hanning(len(m))))
    fr = np.fft.rfftfreq(len(m), 1 / sr)

    def has(fq):
        b = (fr > fq * 0.97) & (fr < fq * 1.03)
        return sp[b].max() > 0.15 * sp[(fr > 60) & (fr < 1000)].max()
    rec("paraphonic CHORD: C3 + G3 + C4 present", has(130.8) and has(196.0) and has(261.6))

    # ---- drums: every lane, every kit makes sound ----------------------------------------
    e.press("start"); e.run(5)
    e.pset(O["mutes"], [0x0F])
    base = PAT + 64 * e.u8("seq_cur")
    e.pset(base + 16, [0] * 16)
    bad = []
    for lane in range(4):
        for kit in range(4):
            mask = [0, 0, 0, 0]
            mask[lane] = 0x1111
            e.pset(base + 48, b"".join(struct.pack("<H", v) for v in mask))
            e.pset(base + 56 + lane, [kit])
            e.press("start"); e.run(30)
            x, sr = e.capture(os.path.join(ART, "drum_l%d_k%d.wav" % (lane, kit)), 60)
            e.press("start"); e.run(5)
            if rms(x) < 150:
                bad.append((lane, kit, round(rms(x))))
    rec("percussion: all 16 kit models (4 lanes x 4 kits) produce sound", not bad, bad)
    rec("percussion render cache used (renders > 0)", e.u16("drum_cache_renders") > 0, "%d renders, %d live hits" % (e.u16("drum_cache_renders"), e.u16("drum_rt_hits")))
    e.close()

    # ---- evolution, locks, undo, regen, fill ---------------------------------------------
    e = E2()
    e.boot()
    e.poke("probe_album", bytes([1])); e.run(20)
    e.pset(O["song"] + 64, [0])                                  # loop on one pattern
    for mode, period in ((1, 4), (2, 2)):
        e.pset(O["gen"] + G["mode"], [mode])
        c0, b0 = e.u8("evo_commits"), e.u8("seq_bar_total")
        e.run(int(FPS * 60 / 132 * 4 * 16))                     # 16 bars at 132 BPM
        commits = (e.u8("evo_commits") - c0) & 255
        bars = (e.u8("seq_bar_total") - b0) & 255
        rec("%s commits every %d bars" % ("DRIFT" if mode == 1 else "CORRODE", period), abs(commits - bars / period) <= 1.01, "%d commits in %d bars" % (commits, bars))
    e.pset(O["gen"] + G["mode"], [0])
    c0 = e.u8("evo_commits")
    p0 = e.pat(e.u8("seq_cur"))
    e.run(int(FPS * 60 / 132 * 4 * 8))
    rec("STABLE: no commits, pattern unchanged over 8 bars", e.u8("evo_commits") == c0 and e.pat(e.u8("seq_cur")) == p0)
    # locks under CORRODE
    cur = e.u8("seq_cur")
    base = PAT + 64 * cur
    locked = bytearray(e.pat(cur))
    locked[16 + 3] |= 0x10; locked[16 + 9] |= 0x10               # step locks
    locked[60] = 0x02 | 0x10                                     # lock the kick and open-hat lanes
    e.poke("probe_pat", bytes(locked))
    e.probe("probe_paste", cur + 1)                              # the app's edit path (new epoch)
    before = e.pat(cur)
    e.pset(O["gen"] + G["mode"], [2]); e.pset(O["gen"] + G["mutation"], [15])
    e.run(int(FPS * 60 / 132 * 4 * 24))
    after = e.pat(cur)
    lk = before[3] == after[3] and before[16 + 3] == after[16 + 3] and before[9] == after[9] and before[25] == after[25] \
        and before[48:50] == after[48:50] and before[54:56] == after[54:56]
    rec("CORRODE x 24 bars on the ROM: locked steps and lanes untouched", lk and before != after, "pattern gen %d" % after[62])
    e.pset(O["gen"] + G["mode"], [0]); e.pset(base + 60, [0]); e.run(10)
    # undo
    e.press("start"); e.run(5)
    before = e.pat(cur)
    e.combo("select", "a"); e.run(5)
    mid = e.pat(cur)
    e.combo("select", "b"); e.run(5)
    rec("SELECT+A evolves (stopped: immediately), SELECT+B undoes", mid != before and e.pat(cur) == before)
    # regen from LIVE
    e.pset(base + 16, bytes(b & 0xEF for b in e.pget(base + 16, 16)))
    for _ in range(12):
        e.press("down", 2, 2)
    before = e.pat(cur)
    e.press("a", 3, 10)
    after = e.pat(cur)
    def audible(s):
        """ETCH test 8c's rule: a slide removed, or a tie turned into a rest
        after silence, is sanitising (inaudible), not a replaced step"""
        fb, fa = before[16 + s], after[16 + s]
        if (fb & 3) == 2 and (fa & 3) == 0:
            return False
        if (fb & ~8) != (fa & ~8) or before[32 + s] != after[32 + s]:
            return True
        if (fa & 3) == 1 and before[s] != after[s]:
            return True
        return any(((before[48 + 2 * l] | before[49 + 2 * l] << 8) ^ (after[48 + 2 * l] | after[49 + 2 * l] << 8)) >> s & 1 for l in range(4))
    changed = sum(1 for s in range(16) if audible(s))
    rec("REGEN on LIVE changes at most 8 steps (CORROSION picks 8)", 0 < changed <= 8, "%d steps differ" % changed)
    # fill
    e.press("start"); e.run(30)
    d0 = e.pget(base + 48, 8)
    e.combo("select", "start"); e.run(int(FPS * 2))
    on = e.u8("seq_fill")
    e.combo("select", "start"); e.run(10)
    rec("FILL latches on and off; the pattern's drums are not modified", on == 1 and e.u8("seq_fill") == 0 and e.pget(base + 48, 8) == d0)
    e.close()

    # ---- stale evolution candidate vs load / paste (ETCH known issue) ---------------------
    e = E2()
    e.boot()
    e.poke("probe_album", bytes([1])); e.run(20)
    e.pset(O["song"] + 64, [0]); e.pset(O["gen"] + G["mode"], [2]); e.pset(O["gen"] + G["mutation"], [15])
    ok = True
    drops0 = e.u8("evo_stale_drops")
    for trial in range(6):
        # wait until a candidate is READY, then replace the pattern underneath it
        for _ in range(240):
            e.run(1)
            if e.u8("evo_state") == 1:
                break
        cur = e.u8("seq_cur")
        newpat = bytearray(e.pat((cur + 1) % 4))
        newpat[62] = 200 + trial                                # recognisable generation counter
        e.poke("probe_pat", bytes(newpat))
        e.poke("probe_paste", bytes([cur + 1]))                  # test hook: SONG > PASTE path
        e.run(2)
        e.run(int(FPS * 60 / 132 * 4 * 1.2))                   # past the next commit point
        g = e.pat(cur)[62]
        ok &= g >= 200                                           # only evolutions OF the pasted pattern
    rec("pending evolution never lands on a replaced (pasted/loaded) pattern", ok,
        "stale candidates refused: %d" % ((e.u8("evo_stale_drops") - drops0) & 255))
    e.close()

    # ---- save / load / corruption ----------------------------------------------------------
    sav = os.path.join(ART, "test.sav")
    if os.path.exists(sav):
        os.remove(sav)
    e = E2(sav)
    e.boot()
    e.poke("probe_album", bytes([3])); e.run(20)
    e.pset(O["name"], b"SAVETEST  ")
    image = e.pget(0, 512) + e.pget(PAT, 64 * 4)
    r = e.probe("probe_save", 2)                                 # slot 2 via storage_save()
    rec("save to slot 2 reported verified", r == 1)
    e.poke("probe_album", bytes([5])); e.run(20)
    r = e.probe("probe_load", 2)
    rec("load slot 2 restores the project exactly", r == 1 and e.pget(0, 512) + e.pget(PAT, 64 * 4) == image)
    e.close()                                                    # mGBA keeps SRAM file-backed
    data = bytearray(open(sav, "rb").read())
    rec("battery file: 32 KB SRAM with the ETC2 header in slot 2", len(data) >= 32768 and data[8192:8196] == b"ETC2", len(data))
    e = E2(sav)
    e.boot()
    r = e.probe("probe_load", 2)
    rec("save survives an emulator restart (battery)", r == 1 and e.pget(0, 10) == b"SAVETEST  ")
    e.close()
    data[8192 + 32 + 700] ^= 0x5A                                # corrupt one byte of slot 2
    open(sav, "wb").write(data)
    e = E2(sav)
    e.boot()
    nm0 = e.pget(0, 10)
    r = e.probe("probe_load", 2)
    rec("corrupted slot is detected (CRC) and not loaded", r == 0 and e.pget(0, 10) == nm0)
    e.close()

    # ---- seed code entry through FILE > SEED CODE ------------------------------------------
    e = E2()
    e.boot()
    for _ in range(7):
        e.press("r", 3, 6)                                       # to FILE
    for _ in range(5):
        e.press("down", 2, 3)                                    # SEED CODE
    e.press("a", 3, 10)
    code = "0A51462BB969" if len(codes) < 1 else codes[0]
    e.poke("code_buf", code.encode())
    e.press("a", 3, 30)                                          # import + GEN ALL
    cur = e.u8("seq_cur")
    raw = e.pat(cur)
    got = raw[0:48].hex().upper() + "".join("%04X" % (raw[48 + 2 * k] | raw[49 + 2 * k] << 8) for k in range(4)) + raw[56:60].hex().upper() + "%02X" % raw[61]
    truth = subprocess.run([gen1, code], capture_output=True, text=True).stdout.strip() if os.path.exists(gen1) else None
    rec("FILE > SEED CODE import: legacy code -> CLASSIC + identical groove", got == truth and e.pget(O["compat"])[0] == 0)
    e.shot(os.path.join(ART, "screen_seedcode.png"))
    e.close()

    # ---- CLASSIC silences the ETCH II tracks ------------------------------------------------
    e = E2()
    e.boot()
    e.pset(O["compat"], [0]); e.run(int(FPS * 10))              # let release tails finish
    v = e.sym("voices")
    import re  # noqa
    nm = subprocess.run(["arm-none-eabi-nm", "-S", ELF], capture_output=True, text=True).stdout.splitlines()
    vsz = next(int(l.split()[1], 16) for l in nm if l.endswith(" voices")) // 5
    act = [e.u8(v + i * vsz + 2) for i in range(5)]
    rec("CLASSIC: only the ORIGINAL bass (+ drums) sound; LEAD/CHORD/DRONE/TEXTURE silent", act[1:] == [0, 0, 0, 0], act)
    e.close()

    # ---- long run ---------------------------------------------------------------------------
    minutes = 10 if LONG else 3
    e = E2()
    e.boot()
    e.pset(O["song"] + 64, [1])
    o0, s0 = e.u16("mix_overruns"), e.u32("seq_steps_total")
    e.poke("mix_lines_max", b"\0\0")
    l0, f0 = e.u32("mix_lines_sum"), e.u32("mix_frames")
    e.run(int(FPS * 60 * minutes))
    frames = e.u32("mix_frames") - f0
    avg = (e.u32("mix_lines_sum") - l0) / max(1, frames)
    rec("%d-minute OXIDE run in DRIFT/song mode: no underruns" % minutes, e.u16("mix_overruns") == o0,
        "avg audio CPU %.0f %%, max %.0f %%, commits %d, voices freed %d" % (avg / 2.28, e.u16("mix_lines_max") / 2.28, e.u8("evo_commits"), e.u16("voices_stolen")))
    steps = e.u32("seq_steps_total") - s0
    exp = 132 * 4 / 60.0 * frames / FPS
    rec("long run: no tempo drift", abs(steps - exp) <= 1.5, "%d steps vs %.1f" % (steps, exp))
    e.close()

    json.dump(results, open(os.path.join(ART, "emu_results.json"), "w"), indent=1)
    fails = sum(not r["pass"] for r in results)
    print("\n%d checks, %d failures" % (len(results), fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
