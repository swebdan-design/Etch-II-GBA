# ETCH II — Testing Report

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

Every result below was measured; raw logs are in `docs/test_logs/`. What could **not** be verified is stated at the end.

## Environment

| | |
|---|---|
| ROM tested | `ETCH_II_by_Synth_Centric.gba`, 262 144 bytes (padded to 256 KB), SHA-256 `a7a955bc006fbb9f243f2b2a0751be2e0162fee09d59cb01ec1e65b15819fc97` (a clean rebuild reproduces the same hash) |
| Toolchain | arm-none-eabi-gcc 13.2.1 (Ubuntu), newlib 4.4.0, devkitPro crt0 / linker script, gbafix 1.05 |
| Emulator | **mGBA 0.10.2** (libmgba), driven headlessly by `tests/harness/mgba_harness.c` (frames, buttons, memory, single-step trace, audio capture, screenshots, file-backed SRAM) |
| Reference | ETCH 2.5 source (commit `62ac4c6`), its release ROM, GBDK-2020 4.3.0 / SDCC 4.4.1 |
| Physical hardware | **None. Not tested on a Game Boy Advance, SP or Micro, nor on a flash cartridge.** |
| Listening | No human listening test in this environment. Audio was verified objectively (pitch by FFT, spectra, RMS, silence/dropout and clipping detection). |

## 1. CORROSION compatibility — `make compat` — 12 / 12

| Check | Result |
|---|---|
| `corrosion.c`, `pattern.c`, `names.c`, `corrosion.h`, `etch.h` identical to ETCH's | PASS ×5 |
| Host: original vs ETCH II, N = 4096 corpus, 22 334 848 bytes (180 224 generations, 98 304 mutations, 32 768 REGENs, 16 384 fills, sanitising, helpers, decoding) | **byte-identical** |
| SDCC `corrosion.o` == the object in the ETCH 2.5 release build | identical |
| ORIGINAL CORROSION compiled by SDCC, run on an emulated Game Boy, 7 corpus sections | CRCs == host |
| ETCH II CORROSION compiled for the GBA, run on an emulated GBA | CRCs == host |
| ETCH release ROM == rebuild from source | PASS |
| **Released ETCH 2.5 ROM**: GEN ALL from 24 codes | **24 / 24** identical |
| **ETCH II ROM**: GEN ALL from the same 24 codes | **24 / 24** identical |

During development this suite caught an argument-evaluation-order dependency in the *test corpus* (x86 and ARM GCC evaluate function arguments in different orders). CORROSION itself was audited and has no such dependency.

## 2. Host tests — `make hosttest`

- **ETCH's original suite** (`tests/host/test_generator_etch.c`, unmodified) against ETCH II's copy: **44 checks, 0 failures** — determinism, 1 296-pattern scale/range sweep, density (6.1 → 13.7 notes), repetition 0.88, STABLE, 8 000 locked mutations with 0 violations, DRIFT changes every channel in 480/480 phrases with divergence bounded at 12, CORRODE diverges further, REGEN exactly 8 steps in 200 runs, seed-code round trip, fills.
- **ETCH II suite** (`tests/host/test_etch2.c`): **16 checks, 0 failures** — pattern formats (64 / 12 / 152 bytes; project fits a slot), **CORROSION's PRNG sequence unchanged when the expansion generator, codes, patches and defaults run between draws**, generation unaffected by expansion, expansion deterministic / never writes the core / in scale / valid lengths over 300 runs, expansion respects locks and per-track selection, 2 000 extended codes whose first 12 digits decode identically with ETCH's decoder, default patches in range.

## 3. Emulator integration — `make emutest` — 75 / 75

### Boot, screens, controls
ROM header (logo, fixed byte, complement, `SRAM_V113`) · title shows "A Generative Synthesis Machine" and "by Synth Centric", PRESS START blinks · START enters LIVE and starts playback · R walks all eight screens and wraps, L goes back, SELECT+→ works, playback continues throughout.

### Audio
| Check | Result |
|---|---|
| Output during playback | RMS 19 101 |
| Dropouts | longest digital silence 0.0 ms |
| Clipping | 0.24 % of samples at full scale (limiter) |
| Stereo | side RMS 1 505 |
| Pitch | A2 **110.00 Hz**, A3 **220.00 Hz** |
| Engines | all five sound; spectral centroids SUB 2 524, FM 3 147, WT 2 332, PD 1 819, RES 4 544 Hz |
| Filter | 600–4 000 Hz / 60–400 Hz energy 0.008 → 0.151 → 0.217 as CUTOFF rises; stable at maximum cutoff + resonance + drive |
| Resonator | pluck 2 707 RMS → 0 after 1 s |
| Paraphonic chord | C3, G3, C4 present |
| Percussion | all 16 kit models produce sound; render cache active |

### Timing
| Tempo / swing | Steps in 20 s (expected) | Gates | Skipped gates |
|---|---|---|---|
| 124 / 0 | 166 (166.1) | 166 | 0 |
| 220 / 8 | 295 (294.7) | 294 | 0 |
| 60 / 0 | 80 (80.4) | 81 | 0 |

10-minute run: 5 280 steps against 5 279.9 expected.

**Defect found and fixed:** before the fix the same test measured 295 steps but only 148 gates at 220 BPM / swing 8 after a swing change — the inherited ETCH clock could double-fire the odd step (see `ETCH_COMPATIBILITY_SPEC.md` §5). A tempo/swing sweep in seven orders now gives steps == gates.

### Album
For each of the eight pieces: pattern 01 equals CORROSION's output for its published seed (the ETCH I core) · starts in DRIFT, EXPANDED · plays 25 s with sound, **0 underruns**, no silence (RMS 5 127–18 857).

### Evolution, locks, undo, REGEN, fill
DRIFT 4 commits in 16 bars · CORRODE 8 in 16 · STABLE none, pattern unchanged · 24 bars of CORRODE at MUTATE 15 with two locked steps and two locked lanes: locks untouched (pattern reached generation 24) · SELECT+A evolves, SELECT+B restores · REGEN on LIVE changed 8 steps · FILL latches on and off without modifying the pattern.

### Stale evolution (ETCH known issue)
Six forced races — a candidate READY, then the target pattern replaced through the paste path — the replaced pattern was never overwritten by a stale candidate (the main loop discarded each candidate on the epoch mismatch before the commit point).

### Storage
Save to slot 2 verified · load restores the project exactly · the 32 KB battery file holds the `ETC2` header · survives an emulator restart · one corrupted byte is detected by CRC and the slot is refused, current project untouched.

### Seed entry and modes
FILE > SEED CODE import of a legacy code → CLASSIC + identical groove · CLASSIC: only the original bass and drums sound (extension voices silent).

### Long run
10 minutes of OXIDE in DRIFT / song mode: **0 underruns**, average audio CPU 58 %, worst frame 95 %, 82 evolutions, no tempo drift.

## 4. CPU and polyphony — `tests/perf_probe.py`, 2 minutes per piece

| Piece | Avg CPU | Worst frame | Underruns | Voices freed | of which sounding notes |
|---|---|---|---|---|---|
| OXIDE | 57 % | 94 % | 0 | 164 | 20 |
| FERRIC | 58 % | 85 % | 0 | 146 | 13 |
| VESSEL | 55 % | 86 % | 0 | 216 | 22 |
| DEBRIS | 60 % | 93 % | 0 | 130 | 52 |
| STATIC | 58 % | 85 % | 0 | 117 | 60 |
| PATINA | 60 % | 82 % | 0 | 103 | 46 |
| FRACTURE | 53 % | 89 % | 0 | 145 | 78 |
| REMNANT | 55 % | 77 % | 0 | 115 | 57 |

"Voices freed" are mostly release tails below audibility; "sounding notes" were cut short by the governor — predominantly TEXTURE hits (the lowest priority), some DRONE and CHORD. Peak stack use measured ≤ 704 bytes with 5.3 KB free.

**Development history:** the first mixer measured 259 scanlines per frame (113 %); profiling per stage and per voice led to ARM-in-IWRAM restructuring, removal of per-block software division, an assembly mix-out, a percussion render cache, paraphonic chords, the 15 768 Hz / 24-sample configuration and the self-calibrating governor. 18 157 Hz was measured and rejected for this voice set.

## 5. Recorded audio — `release/audio/`

Captured from the ROM in mGBA, analysed for problems:

| File | Seconds | RMS | Clipped | Longest silence |
|---|---|---|---|---|
| album_preview.wav | 189.5 | 12 105 | 0.07 % | 0 ms |
| 01_subtractive_acid | 12 | 19 373 | 0.40 % | 0 ms |
| 02_fm_metallic | 12 | 18 669 | 0.36 % | 0 ms |
| 03_wavetable_vocal | 12 | 15 027 | 0.15 % | 0 ms |
| 04_phase_distortion_reso | 12 | 19 421 | 0.39 % | 0 ms |
| 05_resonator_pluck | 12 | 19 475 | 0.43 % | 0 ms |
| 06_percussion_16_kits | 28.7 | 15 892 | 0.21 % | 23.5 ms (between hits) |

## 6. Not verified

- **Physical hardware** (GBA, GBA SP, Game Boy Micro): DMA restart at the buffer wrap, interrupt latency, SRAM timing, actual audio quality through the GBA speaker and headphone amplifier, button feel.
- **Flash cartridges**: save-type detection from the `SRAM_V113` marker and battery behaviour.
- **Other emulators** than mGBA 0.10.2.
- **ETCH on hardware**: the cross-machine seed tests ran ETCH in mGBA's Game Boy core, not on a Game Boy.
- **Human listening**: musical quality was not judged by a listener in this environment.
