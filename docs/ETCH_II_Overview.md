# ETCH II — Technical Overview

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

This document explains how ETCH II works: the hardware it runs on, how sound is produced and timed, how CORROSION is preserved, how data is stored, what it costs, how it was tested, and what is still limited.

## 1. Design in one paragraph

ETCH II separates **composition** from **rendering**. Composition is ETCH's CORROSION v2.4, copied byte for byte, operating on ETCH's own 64-byte pattern format; a separate EXPANSION generator writes ETCH II-only extension tracks that CORROSION never sees. Rendering is a software synthesizer running in the GBA's VBlank interrupt: a sample-accurate port of ETCH's sequencer turns pattern data into note events, and five synthesis engines plus procedural percussion turn events into sound. Nothing flows back from rendering to composition, and no rendering code touches CORROSION's random-number generator.

```
            seed code / GEN settings                      buttons, entropy pool
                     │                                           │
        ┌────────────▼─────────────┐   candidates   ┌───────────▼──────────┐
        │  CORROSION v2.4 (legacy) │◄──────────────│ bridge: evo_service, │  main loop
        │  pattern_t × 32          │               │ REGEN, FILL, UNDO     │  (Thumb, ROM)
        └────────────┬─────────────┘               └──────────────────────┘
                     │  EXPANSION (own PRNG) ──► xpattern_t × 32
        ═════════════╪═══════════════════════════════════════════════════════
                     ▼                                                         VBlank IRQ
        sequencer (ETCH semantics, 32-bit pair-phase clock, per 24-sample block)  (ARM, IWRAM)
                     │ note on / off, accent, glide, drum hits
                     ▼
        voices: SUB · FM · WT · PD · RES (+ paraphonic CHORD)  ·  4 drum lanes
                     │  per-voice gain ramps, pan, send  (assembly mix-out)
                     ▼
        delay bus  →  limiter / master drive  →  8-bit stereo  →  DMA → FIFO A/B
```

## 2. Hardware limitations that shaped it

| GBA | Consequence |
|---|---|
| ARM7TDMI at 16.78 MHz; every load ≥ 3 cycles, multiplies 2–5, branches 3 | Synthesis is per-sample work: it must be ARM code in the 32 KB IWRAM, written so the inner loops keep state in registers. |
| 32 KB IWRAM, 256 KB EWRAM (2 wait states), ROM (3/1 wait states) | IWRAM holds the DSP code (19 KB), voice state, the sine table and the stacks (peak measured stack use ≤ 704 B, 5.3 KB free). Large buffers (delay lines, resonator lines, drum renders, the project) live in EWRAM (205 KB used). |
| Direct Sound: two 8-bit FIFOs fed by DMA, clocked by a timer | 8-bit output (≈48 dB). A limiter keeps the mix near full scale so quiet passages stay above the quantisation floor. |
| No FPU, no hardware divider | Fixed-point everywhere; no division in any per-block or per-sample path (reciprocal and delay-length tables). |
| Battery SRAM via an 8-bit bus | Saves are written byte by byte, verified by read-back. |

## 3. Audio generation and mixing

**Output.** Timer 0 reloads every 1 064 cycles → **15 768 Hz**, exactly **264 samples per video frame** (280 896 cycles). DMA 1/2 in FIFO mode feed Direct Sound A (left) and B (right) from a double buffer of two frames. The frame lock lets the DMA be restarted at VBlank when the buffer wraps — the scheme used by commercial GBA sound drivers.

**Interrupt.** The only interrupt is VBlank. Its handler (ARM, IWRAM, entered through `src/main/platform/irq.s`, which switches to system mode so the handler uses the main stack) restarts the DMA if needed, then renders the next frame. Audio therefore can never be starved by the UI, saving, or CORROSION, which all run in the main loop.

**Blocks.** A frame is 11 blocks of 24 samples. Before each block the sequencer advances; then every sounding voice runs a control update (envelopes, glide, LFO, coefficients — a full refresh every other block) and renders 24 samples into a buffer, which a hand-written ARM routine (`mixer/mixout.S`) mixes into interleaved `[L, R, SEND]` accumulators with a linear gain ramp (one `LDMIA`, three `MLA`, one `STMIA` per sample). Then the delay bus runs and the master stage converts to 8-bit.

**Fixed point.** Phases are 32-bit (one cycle = 2³²); audio is int32 with a nominal ±32 767 range; envelopes Q24; gains Q15/Q12; the bus is "sample × 1024". Pitch is MIDI × 64 (1/64 semitone).

## 4. Timing

The sequencer is a port of ETCH's `seq.c`. A 32-bit phase accumulator spans a **pair** of sixteenth notes and advances once per block by `bpm × 1064 × 256 × 24 / 30`. The even sixteenth starts at the wrap, the odd one when the phase enters the swing region; gate points are fractions of each step's own length. Events are therefore quantised to one block (**1.5 ms**; ETCH: 3.9 ms) but never drift: measured 1 584 steps in 3 minutes against 1 583.9 expected.

ETCH II fixes a latent defect of the ETCH clock (see `ETCH_COMPATIBILITY_SPEC.md` §5): changing SWING mid-pair could fire the odd step twice and desynchronise step parity, after which every other gate point was skipped. Steps now fire on half-pair transitions with a parity check.

Musical events are independent of the screen: the clock is the audio clock.

## 5. CORROSION compatibility

`src/corrosion/legacy/` is ETCH's generator, unmodified. `src/corrosion/bridge/evolve.c` is ETCH's evolution scheduling (anchors, entropy pool, candidate preparation, REGEN pre-composition, sound drift) with one addition: **edit epochs** stop a stale candidate from landing on a pattern that was loaded, pasted, cleared, edited or undone after the candidate was prepared. Verification — host, SDCC-on-Game-Boy, ARM-on-GBA, and end-to-end on the released ETCH ROM and the ETCH II ROM — is described in `ETCH_COMPATIBILITY_SPEC.md`.

## 6. Synthesis engines

See `ETCH_II_Sound_Engine.md` for the algorithms. Summary: band-limited wavetable oscillators (11 octave bands × 1 024 samples) into a Chamberlin state-variable filter (2× oversampled on a resonant BASS), 2-operator FM with feedback, eight morphing wavetable banks (8 frames × 3 mip levels), CZ-style phase distortion with two resonant window modes, a Karplus–Strong resonator with fractional delay, and procedural percussion (pitch-swept sine, filtered noise, XOR ring-modulated metal bank, clicks, clap re-triggers) synthesised into a RAM render cache. Shared effects: tempo-synced ping-pong delay, per-voice drive and bit reduction, master drive and limiter.

## 7. CPU budget and the governor

Measured in mGBA (cycle-accurate timers) with per-stage profiling:

| Stage (per frame of 264 samples) | Cycles |
|---|---|
| Sequencer | 2–10 k |
| One melodic voice: control + mix-out | ≈ 15 k |
| … plus SUB / FM / WT / PD / RES synthesis | ≈ 25 / 12 / 17 / 17 / 26 k |
| Paraphonic chord (SUB/WT) | + ≈ 12 k |
| One drum lane playing its cached render | ≈ 3.5 k |
| One drum lane synthesised live | ≈ 25 k |
| Delay bus (half rate) | ≈ 15 k |
| Master stage | ≈ 16–19 k |
| **Frame** | **280 896** |

All five melodic voices plus drums at once cost ~290 k — more than a frame. ETCH II therefore runs a **governor** before every frame and after every sequencer step: it estimates the frame's cost (each voice's measured cost in the previous frame, or the model above for a voice that has just started; drums including live hits started this frame) and, if the estimate exceeds 80 % of the frame, frees the least important voice — release tails first, then TEXTURE, DRONE, CHORD, LEAD; never BASS or a drum lane. A freed voice fades over one block. A reactive backstop does the same if the previous frame used more than 90 %. Album playback measured 51–70 % average load, worst frame ≈ 95 %, and **0 underruns** in 8 minutes across all pieces plus a 3-minute soak (`tests/emu_test.py`, `tests/perf_probe.py`).

**Sample rate.** 18 157 Hz (304 samples/frame, 16-sample blocks) was implemented and measured first; with this voice set it overran the frame. 15 768 Hz with 24-sample blocks reduces per-sample work by 13 % and control work by half. 31 536 Hz is not achievable with five synthesised melodic voices. The rate is a build option (`make SPF=304 BLOCK=16`).

## 8. Pattern data

| Structure | Size | Owner |
|---|---|---|
| `pattern_t` | 64 B | ETCH / CORROSION — notes (0 = C1), flags (type, accent, slide, lock), prob (probability + condition), four drum masks, kits, lock byte, style, generation |
| `xpattern_t` | 152 B | ETCH II — LEAD/CHORD/DRONE/TEXTURE: MIDI note + flags (type, accent, slide, lock, probability class), chord shapes, per-track length 1–16 |
| `patch_t` | 24 B per track | engine, 15 parameters, level, pan, send, crush, LFO |
| `project2_t` | 7 230 B | name, compat mode, tempo/swing/gate/tune, mutes, gen settings, 32-position song, FX, 8 patches, drum voicing, expansion seed, 32 + 32 patterns |

## 9. Seed formats

- **Legacy** (ETCH): 12 hex digits `SSSS-KCYD-PALO`, encoded and decoded by CORROSION's own functions.
- **Extended** (ETCH II): 16 digits, the legacy 12 + the 4-digit expansion seed. The legacy part is always extractable and valid on ETCH.
- **Project** (ETCH II): a saved slot; the only format that carries everything.

## 10. Storage

32 KB SRAM (the `SRAM_V113` marker lets emulators and flash cartridges select it), 4 slots × 8 KB. Slot = 32-byte header (`ETC2`, version, album id, size, CRC-32, name, sequence number) + the project. A save takes a consistent snapshot without blocking the audio interrupt (copy, retry if an evolution committed during the copy), invalidates the slot, writes, verifies the CRC by reading SRAM back, writes the header last and re-checks. A load verifies before replacing the working project. Measured in the emulator: save/load round trip exact, persistence across restart, corruption of one byte detected and refused.

## 11. Tests

| Suite | Result |
|---|---|
| `make compat` — compatibility certification | 12 / 12 |
| `make hosttest` — ETCH's original 44 checks + ETCH II's 16 | 44 / 44, 16 / 16 |
| `make emutest` — emulator integration (`tests/emu_test.py`) | 75 / 75 |

Details: `ETCH_II_Testing_Report.md`.

## 12. Known limitations

- **No physical hardware testing.** Everything was verified in mGBA 0.10.2 (GBA and GB cores). Real-hardware behaviour of the DMA restart, SRAM timing and flash-cartridge save handling still needs to be confirmed on a GBA, GBA SP and Game Boy Micro.
- **8-bit output.** Direct Sound is 8-bit; quiet passages carry quantisation noise.
- **Polyphony is bounded by the CPU.** When all layers sound at once, the governor frees voices; in the album this mostly removes decorative TEXTURE hits (DEBRIS also loses some chord stabs). PD and FM paraphonic chords are expensive.
- **Event resolution** is 1.5 ms (24-sample blocks).
- **Drum renders** are cached per lane and kit; after a parameter change, the new render arrives within a few frames (the previous render of the same kit plays meanwhile). A kit never rendered before is synthesised live once.
- **32 / 64-step patterns** are not provided as single patterns; longer phrases are built with the song chain. Extension tracks can be 1–16 steps long (polymeter).
- **Undo** is one generation deep (as ETCH).
- **Save slots**: four.
