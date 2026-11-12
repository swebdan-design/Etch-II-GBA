# ETCH II — CORROSION Compatibility Specification

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

This document records exactly what ETCH II takes from the original ETCH, what it adapts for the Game Boy Advance, and what is new — and how compatibility was verified. It was written from the original source, not from a description of it.

## 1. Reference

| | |
|---|---|
| Original project | ETCH — A Generative Acid Machine, `swebdan-design/Etch---GB` |
| Commit inspected | `62ac4c6` (ETCH 2.5; recorded in `src/corrosion/legacy/UPSTREAM_COMMIT`) |
| CORROSION version | v2.4. `git diff 302648b 62ac4c6 -- src/generator include/corrosion.h` is empty apart from the version string in `etch.h`: ETCH 2.5 changed the fill latch and UI only, so the generator **is** CORROSION v2.4. |
| Release ROM | `release/ETCH_by_Synth_Centric.gb`, SHA-256 `bf2c82a4…0b32` |
| Reproducibility | Rebuilt from source with GBDK-2020 4.3.0 / SDCC 4.4.1 in this environment: **byte-identical** to the release ROM. Its symbol map therefore describes the released ROM exactly. |
| Original tests | `tests/test_generator.c`, 44 host checks |

## 2. Preserved verbatim

These files are copied **unmodified** into `src/corrosion/legacy/` (SHA-256 recorded in `src/corrosion/legacy/SHA256SUMS`, re-checked by `tests/compat/run_compat.py`):

| File | Contents |
|---|---|
| `corrosion.c` | PRNG (16-bit xorshift, `corr_seed` / `corr_rand` / `corr_randn`), six style grammars, six scales, vocabulary, motif/phrase-answer/kick-avoidance/articulation generation, all 11 mutation operators and their weights, DRIFT anchors and per-channel bounds, CORRODE motif propagation and homeostasis, REGEN (material + apply), FILL, drum presets, seed-code encode/decode |
| `pattern.c` | `pattern_clear`, `pattern_sanitize`, `pattern_note_count` |
| `names.c` | style / scale / mode names |
| `corrosion.h`, `etch.h` | the CORROSION API and the ETCH data model (`pattern_t` 64 bytes, `gen_t`, flags, conditions, locks) |

Nothing in ETCH II calls into CORROSION differently from ETCH: GEN ALL / BASS / DRUMS, GROOVE, EVOLVE, UNDO, REGEN, FILL and the seed code use the same functions with the same arguments.

### 2.1 What that preserves

- The six styles (ACID HOUSE, ELECTRO, MINIMAL, BROKEN, INDUSTRIAL, SPARSE) and six scales (MINOR, MAJOR, DORIAN, PHRYGIAN, MIN PENT, CHROMATIC).
- Seed interpretation and the random-number consumption order (drums first, then the bass, the full stream consumed regardless of locks).
- Motif vocabulary and repetition, pitch selection, rhythmic weighting, density and syncopation, accents, slides, ties, ghost notes, kick avoidance, phrase answers, sanitising, locking.
- STABLE / DRIFT / CORRODE, the eleven operators (ACC_MOVE, REST_TOGGLE, NOTE_STEP, OCTAVE, SLIDE, DRUM, MEMORY, MOTIF, ROTATE, PROB, RECALL) with their DRIFT weights `{3,2,2,2,2,3,1,1,0,0,1}` and CORRODE weights `{2,2,2,2,2,3,3,5,1,1,0}`, DRIFT's single-step restriction, bounds `2+MUTATE/6` (bass) and `1+(MUTATE+4)/8` (each lane), recall at the bound, protected downbeat, density and accent homeostasis.
- REGEN (exactly 8 of 16 steps), FILL masks, the generation counter (`pattern_t.gen`), step/bass/lane/pattern locks.
- The 12-digit seed code `SSSS-KCYD-PALO` (seed, Key, sCale, stYle, Density, syncoP, Accent, sLide, Octave) — encoder and decoder unchanged, including the validation that rejects invalid fields.

### 2.2 Evaluation-order audit

CORROSION was audited for statements that consume the PRNG more than once in an unsequenced way (C leaves function-argument evaluation order unspecified, and SDCC, x86 GCC and ARM GCC differ). All four statements that draw twice are properly sequenced (`&&`, separate init-declarators, `?:`). The ported code is therefore deterministic on every compiler; this was confirmed by the cross-compiler corpus below. (The audit did find such a hazard in an early version of ETCH II's *test corpus* — not in CORROSION — and it was fixed.)

## 3. Ported (same behaviour, GBA plumbing)

| ETCH (`src/main.c`, `src/sequencer/seq.c`) | ETCH II | Notes |
|---|---|---|
| `evo_service`, per-pattern anchors (8), `anchor_for`, `anchors_forget` | `src/corrosion/bridge/evolve.c` | Same algorithm and arguments: the candidate is prepared for the pattern that plays after the next bar line, with `proj.gen.mutation` and `evo_random() + ++evo_ctr`. STABLE + request uses DRIFT, as ETCH. |
| entropy pool `evo_random` (button timing + DIV) | same, stirred with the GBA's 32-bit cycle counter (timers 2+3) | More entropy than the DMG's DIV register; the 16-bit xorshift is the same. Only evolution and REGEN seeds use it, never generation from a code. |
| sound drift after each commit | same bounds (±2 / ±5 tone steps, ±1 / ±2 drum tune) | Applied to the GBA voices' tone and drum tuning. Never saved; reset in STABLE. |
| REGEN material prepared in the background | same | |
| `evo_commit_point` at the gate of step 16; DRIFT every 4 bars, CORRODE every 2 | same, in `seq.c` | Plus the epoch check (§5). |
| sequencer step semantics | `src/sequencer/seq.c` | NOTE/TIE/REST, SLIDE legato, ACCENT, PROB rolled live with ETCH's separate 16-bit live dice, conditions 1:2…4:4, FILL / !FILL, latched fill, gate lengths 6/9/12/15 sixteenths, swing 0–8 (+4 % of a pair per unit), BPM 60–220, chain with repeats and jumps. |
| timing | 32-bit pair-phase accumulator advanced per 24-sample mixer block | ETCH used a 16-bit accumulator at 256 Hz. Quantisation is now 1.5 ms instead of 3.9 ms; long-run tempo exactness is kept. |

## 4. New in ETCH II (outside CORROSION)

- **Rendering**: five synthesis engines and procedural percussion consume the same note/accent/slide/tie/drum events. Changing an engine never touches pattern data.
- **Extension tracks** (LEAD, CHORD, DRONE, TEXTURE) in a separate `xpattern_t` array that CORROSION never reads or writes. They play only in **EXPANDED** mode.
- **EXPANSION generator** (`src/expansion/expand.c`): composes extension tracks from its own xorshift32 stream, seeded from (legacy seed, expansion seed, pattern). It may read the core pattern and call CORROSION's pure helpers (`corr_in_scale`, `corr_range`), which consume no randomness. It never calls `corr_seed`, `corr_rand` or `corr_randn`.
- **Extended share code**: the 12 legacy digits followed by 4 expansion-seed digits. The first 12 digits are always a valid ETCH code.
- **Edit epochs**: every pattern carries a runtime epoch bumped by edits, loads, pastes, clears and undo; a candidate is committed only if its epoch still matches (§5).
- **Sequencer swing fix** (§5).
- Mutation types: **none added**. ETCH II adds no operators and does not alter CORROSION's.

## 5. Known ETCH issues investigated

| ETCH documentation | Finding in ETCH II | Resolution |
|---|---|---|
| Pending evolution during project loading | Loading resets the hand-off state, but a candidate computed in the main loop could race with a load or paste of its target pattern. | Each candidate records its pattern's **edit epoch**; `evo_commit_point` refuses a candidate whose epoch no longer matches, and `evo_service` discards it earlier when it notices. Test: 6 forced races (paste while a candidate is READY) — the pasted pattern was never overwritten by a stale candidate. |
| Pending evolution during pattern copying | Same mechanism (SONG > PASTE / CLEAR bump the epoch). | As above. |
| High-tempo, high-swing gate timing | **Reproduced a real defect** in the step clock ETCH II ported: when SWING was raised while the phase sat between the old and new swing points, the odd step fired twice; step parity then fell out of phase with the clock and every other gate point was never reached (measured: 295 steps, 148 gates at 220 BPM swing 8). | Steps now fire on transitions between the two halves of the step pair, only when the next step has the matching parity. Measured afterwards for 7 tempo/swing combinations in arbitrary order: steps == gates, 0 skipped gates. |

## 6. Compatibility verification (measured)

`make compat` (`tests/compat/run_compat.py`) — **12 checks, 0 failures**:

| # | Check | Result |
|---|---|---|
| 1–5 | Each legacy file byte-identical to ETCH's source | PASS |
| 6 | Host: ORIGINAL sources vs ETCH II copy, full corpus N=4096: 22 334 848-byte streams (180 224 generations, 98 304 mutations, 32 768 REGENs, 16 384 fills, helpers, sanitise, 16 384 code decodes) | **identical** |
| 7 | SDCC `corrosion.o` built by the test == the object inside the ETCH v2.5 release build | **identical** |
| 8 | SDCC build of the ORIGINAL sources running on an emulated **Game Boy** (mGBA GB core), corpus N=64 (7 sections) | CRCs == host |
| 9 | ETCH II's copy built for the **GBA** (arm-none-eabi-gcc, Thumb, as in the ROM) running on an emulated GBA, corpus N=64 | CRCs == host |
| 10 | Release ROM == rebuild (symbols valid) | PASS |
| 11 | **The released ETCH v2.5 ROM**: 24 seed codes poked into `proj.gen`, GEN ALL pressed on its GEN screen, pattern read from WRAM | **24/24 identical** to host generation |
| 12 | **The ETCH II ROM**: the same 24 codes through its GEN screen | **24/24 identical** |

The 24 codes are the eight album core seeds plus sixteen pseudo-random valid codes covering every field. In addition `tests/emu_test.py` imports a code through FILE > SEED CODE on the ETCH II ROM and checks that the groove is identical and the mode switches to CLASSIC, and checks that each album piece's pattern 01 equals CORROSION's output for its published seed.

Host tests: ETCH's own 44 checks, **unmodified**, built against ETCH II's copy: **44/44**. ETCH II's host tests (`tests/host/test_etch2.c`): **16/16**, including that the expansion generator, the extended code, patches and project defaults consume **no** CORROSION random numbers (the PRNG sequence after `corr_seed` is identical with and without them interleaved).

### Statement

**Original ETCH seed compatibility is verified against the source and against the released ETCH ROM**: the same 12-digit code produces byte-identical pattern data (notes, flags including accents/slides/ties/locks, probability and conditions, drum masks, kits, style) on ETCH and on ETCH II, on both the host and the emulated consoles. Compatibility has **not** been verified on physical Game Boy or Game Boy Advance hardware.

## 7. What a legacy code does and does not carry

- It reproduces the **initial** core groove (bass + four drum lanes) exactly. It does not carry manual edits, locks, synth settings, extension tracks, song chains or evolution history.
- Evolution after that is deterministic *given the same random draws*, but those draws come from the listener-influenced entropy pool, so two listens diverge — on ETCH and on ETCH II alike. The **algorithms and rules** of evolution are the original ones.
- ETCH II-only material (extension tracks, sounds) is never folded into the four original drum lanes or the bass. ETCH I simply does not have it. A complete ETCH II arrangement travels as a saved project; the extended 16-digit code carries the core seed plus the expansion seed (and so the generated extension tracks), but not hand edits or sounds.
