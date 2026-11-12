# ETCH II — Compatibility Guide

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

*One composition. Two machines. Two sonic interpretations.*

## The short version

- **A 12-digit seed code works on both machines and gives the same groove.** Enter an ETCH code into ETCH II, or an ETCH II legacy code into ETCH, and press GEN ALL: the bass notes, rests, ties, accents, slides, probabilities, conditions and drum hits are identical. Only the sound differs.
- **CLASSIC** mode is ETCH's music exactly — the bass line and four drum lanes — performed by ETCH II's synthesizers. Importing a 12-digit code selects CLASSIC.
- **EXPANDED** mode adds ETCH II's LEAD, CHORD, DRONE and TEXTURE tracks. They are extra; they never replace or alter the original bass or drums, and ETCH simply doesn't have them.
- **Evolution** (DRIFT, CORRODE) follows exactly the same rules on both machines, but every listen evolves differently, because the random choices come from the timing of your button presses.

## What a code carries

| | 12-digit legacy code | 16-digit extended code | Saved project |
|---|---|---|---|
| Core bass + drums (initial groove) | ✔ | ✔ | ✔ |
| Works on ETCH (Game Boy) | ✔ | first 12 digits | — |
| Extension tracks (generated) | — | ✔ | ✔ |
| Your edits, locks | — | — | ✔ |
| Sounds, effects, mix | — | — | ✔ |
| Song chain, tempo, swing | — | — | ✔ |
| Evolution history | — | — | the current state of every pattern |

A code describes how to *generate* the starting point. A project stores what you actually have.

## Reading a code

`0A51-430B-6B90` = seed `0A51`, key `4` (E), scale `3` (Phrygian), style `0` (Acid House), density `B` (11), syncopation `6`, accent `B`, slide `9`, octave `0` (1). Keys run 0–B (C to B), scales and styles 0–5, octave 0–2. An extended code adds four digits: the expansion seed.

## Moving music between the machines

**ETCH → ETCH II.** On ETCH, read the CODE row on GEN. On ETCH II: FILE → SEED CODE, enter the 12 digits, press A. Or GEN → CODE, enter the digits, then GEN ALL.

**ETCH II → ETCH.** On ETCH II, read GEN → CODE (or FILE → SEED CODE, which also shows the extended code). On ETCH: GEN → CODE, enter the 12 digits, GEN ALL.

**The album on ETCH.** Every album piece's first pattern is exactly its legacy code's groove:

| Piece | Code |
|---|---|
| OXIDE | `0A51-430B-6B90` |
| FERRIC | `FE26-1049-7940` |
| VESSEL | `7E55-5227-5630` |
| DEBRIS | `DEB5-743A-B961` |
| STATIC | `5A71-201A-6751` |
| PATINA | `9A71-A256-3361` |
| FRACTURE | `F7AC-B33C-DA50` |
| REMNANT | `2E77-0025-4441` |

## How this was verified

The same 24 codes (these eight and sixteen others covering every field) were entered into the **released ETCH 2.5 ROM** and into the **ETCH II ROM**, each running in an emulator; the generated patterns were read back from memory and were byte-identical to each other and to a reference build of CORROSION. Millions of generations, mutations, REGENs and fills were also compared between ETCH's original source and ETCH II's copy, and the original code was compiled with ETCH's own compiler and run on an emulated Game Boy. Details: `ETCH_COMPATIBILITY_SPEC.md`. Not yet confirmed on physical consoles.
