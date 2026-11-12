#!/usr/bin/env python3
"""ETCH II -- A Generative Synthesis Machine -- by Synth Centric.

Album artwork, generated entirely by this script (original work; no external
images or fonts).  The motif: sixteen oscilloscope traces stacked like
generations -- a clean wave at the top corroding, step by step, into a
pitted, quantised one -- etched into a weathered copper-and-graphite plate.
The lettering uses ETCH II's own pixel glyphs (tools/gen_gfx.py).

Outputs (OUTDIR):
  ETCH_II_Cover.png              3000 x 3000
  ETCH_II_Cover_Small.png        1000 x 1000
  editable_source/layer_*.png    the separate layers (RGBA, 3000 x 3000)
  editable_source/ETCH_II_Cover.svg   vector traces + lettering over the plate
  editable_source/README.md
usage: make_artwork.py OUTDIR
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1]
SRC = os.path.join(OUT, "editable_source")
os.makedirs(SRC, exist_ok=True)
N = 3000
rng = np.random.RandomState(0x2601)

# ---- glyphs from the ROM font (parsed out of gen_gfx.py) -------------------------------
gfx_src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen_gfx.py")).read()
F = {}
exec(gfx_src[gfx_src.index("F = {"):gfx_src.index("tiles = [[")], {}, F)
FONT = F["F"]
LOGO = []
exec(gfx_src[gfx_src.index("LOGO = ["):gfx_src.index("LW = ")], {}, F)
LOGO = F["LOGO"]


def noise(octaves, base, seed):
    r = np.random.RandomState(seed)
    acc = np.zeros((N, N))
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        n = base * 2 ** o
        g = r.rand(n + 1, n + 1)
        img = Image.fromarray((g * 255).astype(np.uint8)).resize((N, N), Image.BICUBIC)
        acc += amp * np.asarray(img, dtype=np.float64) / 255.0
        tot += amp
        amp *= 0.55
    return acc / tot


# ---- layer 1: the plate ------------------------------------------------------------------
print("plate...")
n1 = noise(7, 4, 1)
n2 = noise(6, 6, 2)
n3 = noise(5, 24, 3)
graphite = np.array([22, 24, 27], float)
steel = np.array([58, 60, 64], float)
copper = np.array([150, 82, 46], float)
rust = np.array([110, 52, 32], float)
verdi = np.array([70, 128, 112], float)
t = np.clip((n1 - 0.35) * 2.2, 0, 1)[..., None]
plate = graphite * (1 - t) + steel * t
cu = np.clip((n2 - 0.52) * 4.0, 0, 1)[..., None] * np.clip(1.15 - np.linspace(0, 1, N)[:, None, None] * 0.6, 0, 1)
plate = plate * (1 - cu * 0.85) + copper * cu * 0.85
ru = np.clip((n3 - 0.6) * 5, 0, 1)[..., None] * cu
plate = plate * (1 - ru * 0.7) + rust * ru * 0.7
vg = np.clip((n2 - 0.66) * 6, 0, 1)[..., None] * np.clip((n1 - 0.45) * 3, 0, 1)[..., None]
plate = plate * (1 - vg * 0.6) + verdi * vg * 0.6
# brushed grain
grain = rng.randn(N, 1) * 3 + rng.randn(N, N) * 2.5
plate = plate + grain[..., None]
# vignette
yy, xx = np.mgrid[0:N, 0:N] / N - 0.5
plate *= (1 - 0.55 * (xx ** 2 + yy ** 2))[..., None]
plate_img = Image.fromarray(np.clip(plate, 0, 255).astype(np.uint8), "RGB")
# scratches
d = ImageDraw.Draw(plate_img)
for _ in range(260):
    x0, y0 = rng.rand(2) * N
    a = rng.rand() * 0.5 - 0.25
    ln = 80 + rng.rand() * 600
    c = int(70 + rng.rand() * 60)
    d.line([(x0, y0), (x0 + ln * math.cos(a), y0 + ln * math.sin(a))], fill=(c, c, c + 4), width=1)
plate_img = plate_img.filter(ImageFilter.GaussianBlur(0.6))
plate_img.save(os.path.join(SRC, "layer_1_plate.png"))

# ---- layer 2: the oscilloscope generations --------------------------------------------------
print("traces...")
traces = Image.new("RGBA", (N, N), (0, 0, 0, 0))
td = ImageDraw.Draw(traces)
# graticule
for k in range(0, N + 1, 150):
    td.line([(k, 300), (k, 2400)], fill=(200, 190, 160, 22), width=2)
for k in range(300, 2401, 150):
    td.line([(150, k), (N - 150, k)], fill=(200, 190, 160, 22), width=2)
svg_paths = []
GEN = 16
x = np.linspace(0, 1, 1400)
for g in range(GEN):
    c = g / (GEN - 1)                                  # corrosion 0 .. 1
    y = np.sin(2 * math.pi * 3 * x)
    steps = 2 + int((1 - c) ** 2 * 60)
    if c > 0.15:
        y = np.round(y * steps) / steps                # quantised
    if c > 0.35:
        pit = (np.sin(x * 211 + g * 1.7) > 1.25 - c) * (0.35 + 0.4 * c)
        y = y * (1 - pit)                              # pitting
    if c > 0.6:
        y = y + (rng.rand(len(x)) - 0.5) * 0.25 * (c - 0.6)
    y0 = 430 + g * 118
    px = 150 + x * (N - 300)
    py = y0 - y * 46
    col = (int(244 - 60 * c), int(214 - 120 * c), int(150 - 100 * c), int(235 - 80 * c))
    pts = list(zip(px.tolist(), py.tolist()))
    td.line(pts, fill=col, width=5 if g in (0, GEN - 1) else 3)
    svg_paths.append(("M" + " L".join("%.1f %.1f" % p for p in pts[::3]), "#%02x%02x%02x" % col[:3], col[3] / 255.0))
glow = traces.filter(ImageFilter.GaussianBlur(9))
traces_g = Image.alpha_composite(glow, traces)
traces_g.save(os.path.join(SRC, "layer_2_traces.png"))

# ---- layer 3: lettering ------------------------------------------------------------------------
print("lettering...")
letters = Image.new("RGBA", (N, N), (0, 0, 0, 0))
ld = ImageDraw.Draw(letters)
svg_rects = []


def block_text(rows, x0, y0, px, fill, shadow=None, tag="logo"):
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == "#":
                X, Y = x0 + x * px, y0 + y * px
                if shadow:
                    ld.rectangle([X + px // 5, Y + px // 5, X + px + px // 5 - 1, Y + px + px // 5 - 1], fill=shadow)
                ld.rectangle([X, Y, X + px - 1, Y + px - 1], fill=fill)
                svg_rects.append((tag, X, Y, px, fill))


def pixel_text(s, x0, y0, px, fill, tag="text"):
    w = 0
    for ch in s:
        g = FONT.get(ch, FONT[" "]).split("|")
        block_text(g, x0 + w, y0, px, fill, tag=tag)
        w += 6 * px
    return w


def text_width(s, px):
    return len(s) * 6 * px - px


lw = len(LOGO[0]) * 42
block_text(LOGO, (N - lw) // 2, 2330, 42, (238, 226, 200, 255), shadow=(150, 82, 46, 255))
sub = "A GENERATIVE SYNTHESIS MACHINE"
pixel_text(sub, (N - text_width(sub, 11)) // 2, 2830, 11, (236, 186, 96, 255))
by = "SYNTH CENTRIC"
pixel_text(by, (N - text_width(by, 14)) // 2, 150, 14, (238, 226, 200, 235))
cat = "CORROSION V2.4  /  GAME BOY ADVANCE"
pixel_text(cat, (N - text_width(cat, 7)) // 2, 260, 7, (184, 188, 192, 160))
letters.save(os.path.join(SRC, "layer_3_lettering.png"))

# ---- composite ----------------------------------------------------------------------------------
print("composite...")
cover = plate_img.convert("RGBA")
cover = Image.alpha_composite(cover, traces_g)
cover = Image.alpha_composite(cover, letters)
cover = cover.convert("RGB")
cover.save(os.path.join(OUT, "ETCH_II_Cover.png"), optimize=True)
cover.resize((1000, 1000), Image.LANCZOS).save(os.path.join(OUT, "ETCH_II_Cover_Small.png"), optimize=True)

# ---- editable SVG: plate as a linked raster, traces and lettering as vectors --------------------
with open(os.path.join(SRC, "ETCH_II_Cover.svg"), "w") as f:
    f.write('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="3000" height="3000" viewBox="0 0 3000 3000">\n')
    f.write('<title>ETCH II -- A Generative Synthesis Machine -- by Synth Centric</title>\n')
    f.write('<g id="plate"><image xlink:href="layer_1_plate.png" width="3000" height="3000"/></g>\n')
    f.write('<g id="traces" fill="none" stroke-linejoin="round">\n')
    for dd, col, op in svg_paths:
        f.write('<path d="%s" stroke="%s" stroke-opacity="%.2f" stroke-width="3"/>\n' % (dd, col, op))
    f.write('</g>\n')
    for tag in ("logo", "text"):
        f.write('<g id="%s">\n' % tag)
        for tg, X, Y, px, fill in svg_rects:
            if tg == tag:
                f.write('<rect x="%d" y="%d" width="%d" height="%d" fill="#%02x%02x%02x" fill-opacity="%.2f"/>\n' % (X, Y, px, px, fill[0], fill[1], fill[2], fill[3] / 255.0))
        f.write('</g>\n')
    f.write('</svg>\n')
with open(os.path.join(SRC, "README.md"), "w") as f:
    f.write("""# ETCH II cover -- editable source

Generated by `tools/make_artwork.py` (run it to rebuild everything here).

| Layer | File | Notes |
|---|---|---|
| 1 | `layer_1_plate.png` | procedural corroded plate (fractal noise, copper/rust/verdigris, scratches) |
| 2 | `layer_2_traces.png` | sixteen oscilloscope "generations", clean sine -> corroded wave, with glow |
| 3 | `layer_3_lettering.png` | ETCH II logo and lettering in the ROM's pixel glyphs |
| -- | `ETCH_II_Cover.svg` | the same composition with traces and lettering as editable vectors over the plate |

All of it is original Synth Centric artwork; no external images or fonts are used.
""")
print("done")
