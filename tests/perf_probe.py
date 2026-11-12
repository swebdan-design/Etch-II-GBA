"""ETCH II -- by Synth Centric.  Quick CPU probe: play each album piece and
report mixer load (scanlines per frame, 228 = 100 %), overruns and voice
steals.  usage: perf_probe.py ROM ELF [seconds]"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "harness"))
from emu import Emu

rom, elf = sys.argv[1], sys.argv[2]
secs = float(sys.argv[3]) if len(sys.argv) > 3 else 30
e = Emu(rom, elf)
e.run(60); e.press("start"); e.run(60)
rows = []
for piece in range(8):
    # load piece through the app (writes proj, restarts playback)
    e.poke("probe_album", bytes([piece + 1])); e.run(10)
    e.poke("mix_lines_max", b"\0\0")
    o0, s0, f0 = e.u16("mix_overruns"), e.u16("voices_stolen"), e.u32("mix_frames")
    ss0 = e.u16("voices_stolen_sounding")
    l0 = e.u32("mix_lines_sum")
    e.run(int(secs * 60))
    f1 = e.u32("mix_frames")
    avg = (e.u32("mix_lines_sum") - l0) / max(1, f1 - f0)
    rows.append((piece + 1, avg, e.u16("mix_lines_max"), e.u16("mix_overruns") - o0, e.u16("voices_stolen") - s0, e.u16("voices_stolen_sounding") - ss0))
    print("piece %d: avg %.0f%%  max %.0f%%  underruns %d  voices freed %d (sounding notes %d)" % (rows[-1][0], avg / 2.28, rows[-1][2] / 2.28, rows[-1][3], rows[-1][4], rows[-1][5]))
e.close()
