#!/usr/bin/env python3
"""ETCH II -- by Synth Centric.  Host tests:
  - ETCH's ORIGINAL test suite (tests/host/test_generator_etch.c, copied
    unmodified from ETCH v2.5) built against ETCH II's copy of CORROSION;
  - ETCH II's own host tests (tests/host/test_etch2.c);
  - the album recipe: every piece's pattern 01 is exactly its legacy seed."""
import os, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = os.path.join(ROOT, "build", "host")
os.makedirs(B, exist_ok=True)
L = os.path.join(ROOT, "src", "corrosion", "legacy")
legacy = [os.path.join(L, f) for f in ("corrosion.c", "pattern.c", "names.c")]
fails = 0


def build_run(name, srcs, extra=()):
    global fails
    exe = os.path.join(B, name)
    subprocess.run(["gcc", "-std=c99", "-O1", "-Wall", "-Wextra", "-DHOST_TOOL", "-I" + L, "-I" + os.path.join(ROOT, "include"),
                    "-o", exe] + list(srcs) + list(extra), check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout)
    if r.returncode:
        fails += 1
    return r.stdout


out = build_run("test_generator_etch", legacy + [os.path.join(ROOT, "tests", "host", "test_generator_etch.c")])
build_run("test_etch2", legacy + [os.path.join(ROOT, "tests", "host", "test_etch2.c"),
                                  os.path.join(ROOT, "src", "expansion", "expand.c"),
                                  os.path.join(ROOT, "src", "instruments", "instruments.c")])
sys.exit(1 if fails else 0)
