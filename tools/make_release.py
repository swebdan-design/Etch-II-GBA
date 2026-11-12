#!/usr/bin/env python3
"""ETCH II -- A Generative Synthesis Machine -- by Synth Centric.
Assemble release/: ROM, documentation (Markdown + PDF manual), artwork,
audio previews recorded from the ROM, the source archive and checksums.
usage: make_release.py [--no-audio] [--no-pdf]"""
import hashlib
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL = os.path.join(ROOT, "release")
ROM = os.path.join(ROOT, "build", "ETCH_II_by_Synth_Centric.gba")
os.makedirs(REL, exist_ok=True)
log = []


def step(msg):
    print("==", msg)
    log.append(msg)


step("ROM")
shutil.copy(ROM, os.path.join(REL, "ETCH_II_by_Synth_Centric.gba"))
subprocess.run([sys.executable, os.path.join(ROOT, "tools", "check_rom.py"), os.path.join(REL, "ETCH_II_by_Synth_Centric.gba")], check=True)

step("documentation")
for name in ("Manual", "Overview", "Compatibility_Guide", "Sound_Engine", "CORROSION", "Track_Notes", "Controls",
             "Testing_Report", "Build_Instructions"):
    shutil.copy(os.path.join(ROOT, "docs", "ETCH_II_%s.md" % name), os.path.join(REL, "ETCH_II_%s.md" % name))

step("artwork")
if not os.path.exists(os.path.join(REL, "artwork", "ETCH_II_Cover.png")):
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "make_artwork.py"), os.path.join(REL, "artwork")], check=True)

if "--no-audio" not in sys.argv and not os.path.exists(os.path.join(REL, "audio", "album_preview.wav")):
    step("audio previews (recorded from the ROM in mGBA)")
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "record_previews.py"), ROM, os.path.join(REL, "audio")], check=True)

if "--no-pdf" not in sys.argv:
    step("PDF manual")
    try:
        env = dict(os.environ)
        env.setdefault("NODE_PATH", subprocess.run(["npm", "root", "-g"], capture_output=True, text=True).stdout.strip())
        subprocess.run([sys.executable, os.path.join(ROOT, "tools", "make_pdf_manual.py"), os.path.join(REL, "ETCH_II_Manual.pdf")], check=True, env=env)
    except Exception as ex:  # noqa: BLE001
        print("PDF NOT GENERATED (pandoc / Node Playwright unavailable):", ex)

step("source archive")
files = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split()
zpath = os.path.join(REL, "ETCH_II_source.zip")
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for f in files:
        if f.startswith("release/"):
            continue                      # the release package is not source
        z.write(os.path.join(ROOT, f), "etch-ii/" + f)
    # generated sources, for reference (the build regenerates them)
    for g in ("tables.c", "tables.h", "gfx.c", "gfx.h", "album.c"):
        p = os.path.join(ROOT, "build", "gen", g)
        if os.path.exists(p):
            z.write(p, "etch-ii/generated/" + g)

step("checksums")
out = []
for dp, _, fs in os.walk(REL):
    for f in sorted(fs):
        if f == "SHA256SUMS.txt":
            continue
        p = os.path.join(dp, f)
        out.append("%s  %s" % (hashlib.sha256(open(p, "rb").read()).hexdigest(), os.path.relpath(p, REL)))
open(os.path.join(REL, "SHA256SUMS.txt"), "w").write("\n".join(sorted(out, key=lambda l: l.split("  ")[1])) + "\n")
print("\n".join(l for l in out if "ETCH_II_by" in l or "source.zip" in l))
