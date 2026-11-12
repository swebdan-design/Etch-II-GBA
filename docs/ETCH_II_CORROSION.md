# ETCH II — CORROSION

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

CORROSION is the generative composer of ETCH, and of ETCH II. ETCH II runs **CORROSION v2.4 unchanged**: the C source from ETCH 2.5 (`src/generator/corrosion.c`, `pattern.c`, `names.c`) is copied byte for byte into `src/corrosion/legacy/`. This document explains what it does and how ETCH II drives it. Where this text and the source differ, the source is authoritative.

## 1. Settings (GEN)

| Setting | Range | Meaning |
|---|---|---|
| KEY | C–B | tonal centre |
| SCALE | MINOR, MAJOR, DORIAN, PHRYGIAN, MIN PENT, CHROMATIC | allowed pitch classes |
| STYLE | ACID HOUSE, ELECTRO, MINIMAL, BROKEN, INDUSTRIAL, SPARSE | rhythmic grammar |
| OCTAVE | 1–3 | register of the root; lines span a fourth below to two octaves above |
| DENSITY | 0–15 | how many steps sound; drum variation density; the note-count target evolution steers toward |
| SYNCOP | 0–15 | pushes notes off the beat; hat syncopation |
| ACCENT | 0–15 | accent probability; the accent target |
| SLIDE | 0–15 | slide probability (scaled by the style's habit) |
| MUTATE | 0–15 | evolution budget |
| MODE | STABLE, DRIFT, CORRODE | evolution behaviour |
| SEED | 0000–FFFF | the composition seed |

## 2. Generation

Grammar-driven, in this order, every random draw from a 16-bit xorshift seeded by SEED:

1. **Drums**: the style's template plus variation hits with probability DENSITY/2 (off-beat hats also respond to SYNCOP); very low density thins the hats.
2. **Vocabulary**: root, then octave, fifth, minor third, minor seventh, fourth, second, sixth, a seventh below, minor second — each snapped into the scale, some skipped at random — up to the style's vocabulary size (2–7 notes).
3. **Motif** (2–16 steps by style): a note sounds with probability `weight × (DENSITY+4)/14`, adjusted by SYNCOP; strong positions prefer the root; occasional octave jumps; accents by `accent weight × ACCENT`. The downbeat carries the root unless density is very low.
4. **Repetition**: the motif is tiled across the bar (odd motif lengths in BROKEN cross the bar line).
5. **Phrase answer**: beat 4 answers the motif — transposed up the scale, a note swapped for a rest and an accented pickup, or lifted an octave (A-A-A-B).
6. **Kick avoidance**: bass notes on kicks (except the downbeat) may drop out, by style.
7. **Articulation**: slides between consecutive notes, ties after notes, ghost notes (probability 5/8 or 6/8) off the beat.
8. A guaranteed accent when ACCENT > 5.
9. **Sanitising**: slides must lead into notes, ties must follow sound, ranges clamped.

The whole random stream is consumed even when steps, lanes, the bass or the pattern are locked, so the unlocked material is exactly what the seed produces.

## 3. Evolution

The main loop prepares a mutated copy of the pattern that will play after the next bar line; the audio interrupt commits it at the gate point of step 16, so it takes effect on a downbeat. DRIFT commits every 4 bars, CORRODE every 2, and SELECT+A requests one at the next bar.

**DRIFT** touches every channel each phrase: 1 bass operation (2 when MUTATE ≥ 12), single-step operators only, and one hit in each unlocked drum lane. Each channel is bounded around its **anchor** (bass: 2 + MUTATE/6 steps; each lane: 1 + (MUTATE+4)/8 hits); at the bound, the change becomes a recall toward the anchor. Anchors are kept per pattern (8 slots) and reset by your edits.

**CORRODE** applies 2 + MUTATE/3 operators drawn with weights `ACC_MOVE 2, REST_TOGGLE 2, NOTE_STEP 2, OCTAVE 2, SLIDE 2, DRUM 3, MEMORY 3, MOTIF 5, ROTATE 1, PROB 1, RECALL 0`. MOTIF changes a step of the beat-1 motif and every repetition of it, so the line transforms while staying repetitive. DRIFT's weights: `3 2 2 2 2 3 1 1 0 0 1`.

**Invariants** (all from the source): the downbeat keeps its pitch; DRIFT never removes template kicks or backbeats; DENSITY and ACCENT set targets the rest and accent operators steer toward (homeostasis); after every mutation, locked material is copied back bit for bit and the pattern is re-sanitised; the generation counter `pattern.gen` increments.

**Sound drift** follows each commit (bounded tone and drum-tuning steps, never saved).

## 4. REGEN, FILL, UNDO, GROOVE

- **REGEN** composes a complete new bar with the current settings and a fresh seed (in the background), then copies **exactly 8** randomly chosen unlocked steps from it (bass, drums or both). Only inaudible tidy-ups (a slide that no longer leads into a note, a tie after new silence) can change outside those steps.
- **FILL** derives temporary drum masks from the groove and style (ACID snare roll and open-hat lift, INDUSTRIAL machine-gun snare, ELECTRO flams…). The pattern is not modified. Latched with SELECT+START or B on LIVE, with a fresh variation every bar.
- **UNDO** (SELECT+B) restores the previous generation — one level, as ETCH.
- **GROOVE** loads the style's drum template.

## 5. Randomness

| Stream | Used for | Seeded by |
|---|---|---|
| CORROSION PRNG (in `corrosion.c`) | generation, mutation, REGEN, fills | the seed (generation) or the entropy pool (evolution, REGEN, fills) — exactly as ETCH |
| Entropy pool (`evolve.c`) | evolution seeds, REGEN seeds, fill seeds, sound drift | button timing + the GBA cycle counter |
| Live dice (ETCH's 16-bit xorshift) | step probability of the ORIGINAL bass | the moment START is pressed |
| Extension dice (xorshift32) | probability of extension steps | START + a constant |
| EXPANSION stream (xorshift32) | composing extension tracks | legacy seed + expansion seed + pattern |
| Voice streams (xorshift32 per voice) | LFO sample & hold, resonator excitation, drum noise | fixed per voice |

Only the first touches CORROSION's state, and ETCH II calls it exactly where ETCH does. Host test `test_etch2.c` verifies that everything else leaves CORROSION's PRNG sequence untouched.

## 6. Determinism and listening

A seed code reproduces the initial groove exactly, on ETCH and ETCH II. Everything that happens after that — which steps play, which operators fire — draws from the entropy pool, so each listen is different, on either machine, while following the same rules.
