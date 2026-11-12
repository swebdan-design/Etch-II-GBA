#!/usr/bin/env python3
"""ETCH II -- by Synth Centric.  CORROSION compatibility certification.

  1. host:   ORIGINAL ETCH sources vs ETCH II copy, full corpus, byte compare
  2. SDCC:   ORIGINAL sources built with GBDK-2020 4.3.0 (ETCH's compiler), run
             on an emulated Game Boy (mGBA GB core)      vs host CRCs
  3. ARM:    ETCH II copy built for the GBA, run on an emulated GBA (mGBA)
                                                          vs host CRCs
  4. ROM:    the RELEASED ETCH v2.5 ROM generates from poked seed codes
             (GEN screen, GEN ALL) vs host generation    -- end to end
  5. ETCH II ROM: the same seed codes through ETCH II's GEN screen
usage: run_compat.py [ETCH_REPO] [--quick]
"""
import json, os, subprocess, sys, hashlib
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tests", "harness"))
from emu import Emu, load_symbols  # noqa: E402

ETCH = next((a for a in sys.argv[1:] if not a.startswith("--")), os.environ.get("ETCH_REPO", "/home/user/swebdan-design/etch---gb"))
GBDK = os.environ.get("GBDK_HOME", "/opt/gbdk")
B = os.path.join(ROOT, "build", "compat")
os.makedirs(B, exist_ok=True)
results = []


def rec(name, ok, detail=""):
    results.append({"check": name, "pass": bool(ok), "detail": detail})
    print("%s  %s  %s" % ("PASS" if ok else "FAIL", name, detail))


def sh(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def host_build(tag, src, inc, n):
    exe = os.path.join(B, "corpus_%s_%d" % (tag, n))
    sh(["gcc", "-std=c99", "-O1", "-DCORPUS_HOST", "-DCORPUS_N=%d" % n, "-I" + inc, "-o", exe,
        os.path.join(src, "corrosion.c"), os.path.join(src, "pattern.c"), os.path.join(src, "names.c"),
        os.path.join(HERE, "corpus.c"), os.path.join(HERE, "host_main.c")])
    return exe


def host_run(exe, dump=None):
    out = sh([exe] + ([dump] if dump else [])).stdout
    return json.loads(out)


orig_src, orig_inc = os.path.join(ETCH, "src", "generator"), os.path.join(ETCH, "include")
port = os.path.join(ROOT, "src", "corrosion", "legacy")

# ---- 0. provenance ----------------------------------------------------------------
for f in ("corrosion.c", "pattern.c", "names.c"):
    a = hashlib.sha256(open(os.path.join(orig_src, f), "rb").read()).hexdigest()
    b = hashlib.sha256(open(os.path.join(port, f), "rb").read()).hexdigest()
    rec("legacy %s identical to ETCH source" % f, a == b, a[:16])
for f in ("corrosion.h", "etch.h"):
    a = hashlib.sha256(open(os.path.join(orig_inc, f), "rb").read()).hexdigest()
    b = hashlib.sha256(open(os.path.join(port, f), "rb").read()).hexdigest()
    rec("legacy %s identical to ETCH source" % f, a == b, a[:16])

# ---- 1. host, full corpus -----------------------------------------------------------
N_FULL = 1024 if "--quick" in sys.argv else 4096
r_ref = host_run(host_build("ref", orig_src, orig_inc, N_FULL), os.path.join(B, "full_ref.bin"))
r_port = host_run(host_build("port", port, port, N_FULL), os.path.join(B, "full_port.bin"))
same = open(os.path.join(B, "full_ref.bin"), "rb").read() == open(os.path.join(B, "full_port.bin"), "rb").read()
gens = N_FULL * 16 * 2 + N_FULL * 4 * 4
rec("host: original vs ETCH II byte streams identical (N=%d, %d bytes)" % (N_FULL, sum(r_ref["bytes"])), same and r_ref["crc"] == r_port["crc"],
    "%d generations, %d mutations, %d regens, %d fills" % (N_FULL * 16 * 2 + N_FULL * 4 * 3, N_FULL * 2 * 12, N_FULL * 8, N_FULL * 4))

# ---- 2/3. reference CRCs at the emulator size ----------------------------------------
N_EMU = 64
ref64 = host_run(host_build("ref", orig_src, orig_inc, N_EMU))


def run_rom(rom, sym_file, sym, max_frames=60 * 60 * 15):
    e = Emu(rom, sym_file)
    fr = 0
    while e.u32(e.sym(sym)) != 0xC0DEC0DE and fr < max_frames:
        e.run(600)
        fr += 600
    a = e.sym(sym)
    crc = ["%08X" % e.u32(a + 8 + 4 * i) for i in range(7)]
    done = e.u32(a) == 0xC0DEC0DE
    e.close()
    return done, crc, fr


# SDCC / Game Boy: the ORIGINAL sources, ETCH's flags
gbo = os.path.join(B, "gb")
os.makedirs(gbo, exist_ok=True)
lcc = os.path.join(GBDK, "bin", "lcc")
try:
    for f in ("corrosion", "pattern", "names"):
        sh([lcc, "-I" + orig_inc, "-Wa-l", "-c", "-o", os.path.join(gbo, f + ".o"), os.path.join(orig_src, f + ".c")])
    sh([lcc, "-I" + orig_inc, "-DCORPUS_N=%d" % N_EMU, "-c", "-o", os.path.join(gbo, "corpus.o"), os.path.join(HERE, "corpus.c")])
    sh([lcc, "-I" + orig_inc, "-c", "-o", os.path.join(gbo, "gb_main.o"), os.path.join(HERE, "gb_main.c")])
    sh([lcc, "-Wl-yt0x1B", "-Wl-yo8", "-Wl-ya4", "-autobank", "-Wb-ext=.rel", "-Wm-yn" "CORPUS", "-Wl-m", "-Wl-j",
        "-o", os.path.join(gbo, "corpus_ref.gb")] + [os.path.join(gbo, x + ".o") for x in ("gb_main", "corpus", "corrosion", "pattern", "names")])
    rb = os.path.join(ETCH, "..", "etch-rebuild", "build", "src", "generator", "corrosion.o")
    if os.path.exists(rb):
        rec("SDCC: corrosion.o identical to the object inside the ETCH v2.5 release build",
            open(rb, "rb").read() == open(os.path.join(gbo, "corrosion.o"), "rb").read())
    done, crc, fr = run_rom(os.path.join(gbo, "corpus_ref.gb"), os.path.join(gbo, "corpus_ref.noi"), "gb_result")
    rec("SDCC on Game Boy (mGBA): ORIGINAL CORROSION corpus == host (7 sections, N=%d)" % N_EMU, done and crc == ref64["crc"],
        "%s after %d frames" % (",".join(crc), fr))
except (subprocess.CalledProcessError, FileNotFoundError) as ex:
    rec("SDCC reference build", False, "GBDK not available: %s" % ex)

# ARM / GBA: ETCH II's copy as built for the ROM
ao = os.path.join(B, "gba")
os.makedirs(ao, exist_ok=True)
cc = "arm-none-eabi-gcc"
crt = os.path.join(ROOT, "src", "main", "platform")
flags = ["-mcpu=arm7tdmi", "-mthumb", "-mthumb-interwork", "-O2", "-std=gnu11", "-I" + port, "-ffunction-sections"]
objs = []
for f in [os.path.join(port, x) for x in ("corrosion.c", "pattern.c", "names.c")] + [os.path.join(HERE, "corpus.c"), os.path.join(HERE, "gba_main.c")]:
    o = os.path.join(ao, os.path.basename(f) + ".o")
    sh([cc] + flags + ["-DCORPUS_N=%d" % N_EMU, "-c", "-o", o, f])
    objs.append(o)
sh([cc, "-mcpu=arm7tdmi", "-mthumb", "-c", "-o", os.path.join(ao, "crt0.o"), os.path.join(crt, "gba_crt0.s")])
pf = lambda n: sh([cc, "-mcpu=arm7tdmi", "-mthumb", "-print-file-name=" + n]).stdout.strip()
elf = os.path.join(ao, "corpus_port.elf")
sh([cc, "-mcpu=arm7tdmi", "-mthumb", "-nostartfiles", "-specs=nosys.specs", "-T", os.path.join(crt, "gba_cart.ld"), "-Wl,--gc-sections",
    "-o", elf, os.path.join(ao, "crt0.o"), pf("crti.o"), pf("crtbegin.o")] + objs + ["-lc", "-lgcc", pf("crtend.o"), pf("crtn.o")])
gba = os.path.join(ao, "corpus_port.gba")
sh(["arm-none-eabi-objcopy", "-O", "binary", elf, gba])
sh([os.path.join(ROOT, "build", "gbafix"), gba])
done, crc, fr = run_rom(gba, elf, "gba_result")
rec("ARM on GBA (mGBA): ETCH II CORROSION corpus == host (7 sections, N=%d)" % N_EMU, done and crc == ref64["crc"],
    "%s after %d frames" % (",".join(crc), fr))

# ---- 4/5. end to end through the released ETCH ROM and the ETCH II ROM -----------------
def valid_codes():
    """the eight album core seeds plus pseudo-random valid codes covering every field"""
    codes = []
    rc = os.path.join(ROOT, "docs", "generated_album_recipe.txt")
    if os.path.exists(rc):
        for line in open(rc):
            m = line.split()
            if len(m) == 3 and m[0].rstrip(".").isdigit() and len(m[2]) == 14:
                codes.append(m[2].replace("-", ""))
    x = 0x1234567
    while len(codes) < 24:
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        v = [(x >> 3) & 0xFFFF, (x >> 7) % 12, (x >> 11) % 6, (x >> 13) % 6, (x >> 15) & 15, (x >> 19) & 15,
             (x >> 23) & 15, (x >> 9) & 15, (x >> 5) % 3]
        codes.append("%04X%X%X%X%X%X%X%X%X" % tuple(v))
    return codes


CODES = valid_codes()


def decode(code):
    v = [int(ch, 16) for ch in code]
    return dict(seed=(v[0] << 12) | (v[1] << 8) | (v[2] << 4) | v[3], key=v[4], scale=v[5], style=v[6],
                density=v[7], sync=v[8], accent=v[9], slide=v[10], octave=v[11])


# host truth: a tiny generator that prints the pattern for a code
gen_c = os.path.join(B, "gen_one.c")
open(gen_c, "w").write(r'''
#include <stdio.h>
#include <stdlib.h>
#include "corrosion.h"
int main(int argc, char **argv) {
    gen_t g; pattern_t p; int i;
    memset(&g, 0, sizeof g);
    if (!corr_code_decode(&g, argv[1])) return 1;
    pattern_clear(&p); corr_generate(&p, &g, GEN_ALL);
    for (i = 0; i < 16; i++) printf("%02X", p.note[i]);
    for (i = 0; i < 16; i++) printf("%02X", p.flags[i]);
    for (i = 0; i < 16; i++) printf("%02X", p.prob[i]);
    for (i = 0; i < 4; i++) printf("%04X", p.drum[i]);
    for (i = 0; i < 4; i++) printf("%02X", p.kit[i]);
    printf("%02X\n", p.style);
    return 0;
}''')
sh(["gcc", "-std=c99", "-I" + orig_inc, "-o", os.path.join(B, "gen_one"), gen_c,
    os.path.join(orig_src, "corrosion.c"), os.path.join(orig_src, "pattern.c"), os.path.join(orig_src, "names.c")])
truth = {c: sh([os.path.join(B, "gen_one"), c]).stdout.strip() for c in CODES}


def pat_hex(raw, drum_be=False):
    note, flags, prob = raw[0:16], raw[16:32], raw[32:48]
    drum = [raw[48 + 2 * i] | (raw[49 + 2 * i] << 8) for i in range(4)]
    kit = raw[56:60]
    style = raw[61]
    return note.hex().upper() + flags.hex().upper() + prob.hex().upper() + "".join("%04X" % d for d in drum) + kit.hex().upper() + "%02X" % style


def gen_bytes(code):
    d = decode(code)
    # gen_t: key scale style density sync accent slide octave mutation mode seed(LE16)
    return bytes([d["key"], d["scale"], d["style"], d["density"], d["sync"], d["accent"], d["slide"], d["octave"], 5, 0,
                  d["seed"] & 255, d["seed"] >> 8])


etch_rom = os.path.join(ETCH, "release", "ETCH_by_Synth_Centric.gb")
noi = os.path.join(ETCH, "..", "etch-rebuild", "build", "ETCH_by_Synth_Centric.noi")
if os.path.exists(noi) and os.path.exists(etch_rom):
    rel_sha = hashlib.sha256(open(etch_rom, "rb").read()).hexdigest()
    reb_sha = hashlib.sha256(open(noi.replace(".noi", ".gb"), "rb").read()).hexdigest()
    rec("ETCH v2.5 release ROM == rebuild from source (symbols valid for the release)", rel_sha == reb_sha, rel_sha[:16])
    e = Emu(etch_rom, noi)
    e.run(240)
    e.press("start"); e.run(30)
    e.press("start"); e.run(10)                     # stop playback
    proj = e.sym("proj")
    GEN, PAT = proj + 26, proj + 73
    # SELECT+RIGHT to GEN (LIVE -> GEN), cursor to the GEN ALL row (12 rows down)
    e.hold("select"); e.run(2); e.press("right"); e.release("select"); e.run(20)
    for _ in range(12):
        e.press("down", 3, 3)
    ok_all, n = True, 0
    for c in CODES:
        e.poke(GEN, gen_bytes(c))
        e.run(4)
        e.press("a", 3, 20)
        cur = e.u8(e.sym("seq_cur"))
        raw = e.peek(PAT + 64 * cur, 64)
        got = pat_hex(raw)
        n += got == truth[c]
        if got != truth[c]:
            ok_all = False
            print("   mismatch", c, got, truth[c])
    e.close()
    rec("RELEASED ETCH v2.5 ROM (mGBA GB core): GEN ALL from %d seed codes == host generation" % len(CODES), ok_all, "%d/%d" % (n, len(CODES)))
else:
    rec("ETCH release ROM end-to-end", False, "rebuild symbols not found")

# ETCH II ROM: FILE > SEED CODE import path is exercised in tests/emu_test.py;
# here: poke the generator settings and use GEN ALL on ETCH II's GEN screen.
rom2 = os.path.join(ROOT, "build", "ETCH_II_by_Synth_Centric.gba")
elf2 = rom2.replace(".gba", ".elf")
if os.path.exists(rom2):
    e = Emu(rom2, elf2)
    e.run(60); e.press("start"); e.run(30); e.press("start"); e.run(10)
    proj = e.sym("proj")
    # project2_t: name10 compat bpm swing gate tune mutes start album => gen at 18
    GEN, PAT = proj + 18, proj + int(open(os.path.join(ROOT, "build", "gen", "album.c")).read().split("offsetof(project2_t, pat) == ")[1].split(",")[0])
    e.poke(proj + 10, bytes([0]))                   # CLASSIC
    for _ in range(4):
        e.press("r", 3, 4)                          # LIVE > TRACK > SYNTH > MOD > GEN
    for _ in range(14):
        e.press("down", 3, 3)                       # to the action rows (GEN ALL)
    ok_all, n = True, 0
    for c in CODES:
        e.poke(GEN, gen_bytes(c)); e.run(4)
        e.press("a", 3, 20)
        cur = e.u8(e.sym("seq_cur"))
        got = pat_hex(e.peek(PAT + 64 * cur, 64))
        n += got == truth[c]
        if got != truth[c]:
            ok_all = False
            print("   mismatch", c, got, truth[c])
    e.close()
    rec("ETCH II ROM (mGBA): GEN ALL from %d seed codes == host == ETCH" % len(CODES), ok_all, "%d/%d" % (n, len(CODES)))

json.dump(results, open(os.path.join(B, "compat_results.json"), "w"), indent=1)
fails = sum(not r["pass"] for r in results)
print("\n%d checks, %d failures" % (len(results), fails))
sys.exit(1 if fails else 0)
