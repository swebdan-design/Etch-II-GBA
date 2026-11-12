# ETCH II — Manual

**ETCH II — A Generative Synthesis Machine**
by Synth Centric

> One composition. Two machines. Two sonic interpretations.

---

## 1. What ETCH II is

ETCH II is a generative electronic music instrument and an album for the Game Boy Advance. Switch it on, press START, and a piece of music begins that writes itself: a bass line, drums and — in its expanded form — leads, chords, drones and textures, played by five software synthesizers that run on the GBA's processor.

It is the successor to **ETCH — A Generative Acid Machine**, Synth Centric's Game Boy instrument. Both machines share one composer: **CORROSION v2.4**. ETCH II did not rewrite it — the very same code runs inside both cartridges. What changed is the sound: the Game Boy plays CORROSION's music on its four chip channels; the Game Boy Advance plays it through subtractive, FM, wavetable, phase-distortion and physical-resonator synthesis.

ETCH II is also **an album**: eight living compositions — OXIDE, FERRIC, VESSEL, DEBRIS, STATIC, PATINA, FRACTURE, REMNANT — that play by themselves and are a little different every time you listen.

## 2. Quick start

1. Put the ROM on a flash cartridge or open it in an emulator (mGBA recommended).
2. Press **START** on the title screen. OXIDE begins immediately.
3. Press **R** to look around the screens, **L** to come back.
4. On LIVE, move down to **ENGINE** (D-pad ↓ to the ENGINE row), hold **A** and press → : the bass changes from a subtractive acid voice to FM. Same notes, new instrument.
5. Move to **MODE**, hold **A** and press → : DRIFT becomes CORRODE. Every two bars the music now rewrites itself more boldly.
6. Go to FILE (press **L** once from LIVE), choose **SAVE**, pick a slot, press **A**.

## 3. ETCH and ETCH II

| | ETCH (Game Boy) | ETCH II (Game Boy Advance) |
|---|---|---|
| Composer | CORROSION v2.4 | **the same CORROSION v2.4** (identical code) |
| Seed codes | 12 digits | the same 12 digits, plus an optional 16-digit extended code |
| Core music | bass + 4 drum lanes, 16 steps, 32 patterns | the same, bit for bit |
| Sound | wave-channel acid bass, square kick, pulse/noise drums | 5 synthesis engines, 16 procedural drum models, delay, drive, bit reduction, stereo |
| Extra tracks | — | LEAD, CHORD, DRONE, TEXTURE (EXPANDED mode) |
| Screens | 6 | 8 |

**A seed code from ETCH gives the same groove on ETCH II**, and a 12-digit code shown on ETCH II gives the same groove on ETCH. This was checked by running both cartridges in an emulator with 24 codes (see §11 and the Compatibility Guide).

## 4. What CORROSION does

CORROSION is a rule-based composer. It does not pick notes uniformly at random. Each **style** (ACID HOUSE, ELECTRO, MINIMAL, BROKEN, INDUSTRIAL, SPARSE) is a little grammar: which sixteenth notes like to sound, where accents fall, how long motifs are, how often the bass jumps an octave, slides, ties or plays ghost notes, and a drum template. From a **seed** and a few settings it:

1. builds the drums from the style's template plus density-dependent variations;
2. picks a small vocabulary of pitches from the key and scale — root first, then octave, fifth, third…;
3. composes a short motif (2–16 steps), tiles it across the bar and lets beat 4 answer it;
4. clears room for the kick, adds slides, ties and ghost notes;
5. tidies the result so every slide leads into a note and every tie follows a sound.

Then, while the music plays, it can **evolve** it (§9).

## 5. How generative music works here

There are two kinds of randomness and they are kept apart:

- **Composition** randomness comes from the seed. Same seed and settings → same groove, on any ETCH machine, forever.
- **Performance** randomness — evolution, step probabilities, REGEN, fills, the sound's slow drift — comes from an entropy pool stirred by the exact timing of your button presses, beginning with the moment you press START on the title screen. That is why every listen differs.

ETCH II's own additions (the extra tracks, the synthesizers, their LFOs) each have their own random streams and never touch CORROSION's.

## 6. The screens

The header on every screen shows: `ETCH II | SCREEN … P01 > 132 D X F 1 ▶`
— pattern number (`>` when another is queued), tempo, evolution mode (S/D/C), compatibility mode (C classic / X expanded), F while a fill plays, the beat, and play/stop. The bottom line explains the buttons or shows a message.

### 6.1 LIVE — the instrument

Eight track rows: **BASS, LEAD, CHORD, DRONE** (melodic roles) and **KICK, PERC, HATS, TEXTR**. BASS, KICK, PERC and HATS are ETCH's original material; the others are ETCH II extension tracks. Each row shows the track's engine, an activity light and the 16 steps with the playhead. Note symbols: small square = note, hollow square = accent, arrow = slide, bar = tie, diamond = drum hit (HATS: hollow = closed + open together, grey = open hat alone).

Below: **MODE** and the bars until the next evolution, the pattern's **GEN** (how many times it has evolved), **MUTATE**, style, **ENGINE** and **TONE** of the selected track, **REGEN**, the output level, the audio CPU load and an oscilloscope.

### 6.2 TRACK — the tracker

The selected track as a tracker, one row per step. For BASS: note, Accent, Slide, Lock, Probability (1–8 out of 8) and Condition (1:2 = first of every two loops, 3:4 = third of every four, FIL / !FL = only during / outside a fill). For drums: one column per lane, the lane's kit, and the lane lock. For extension tracks: note, A, S, L, probability and (CHORD) the chord shape, plus the track **length** — a 7-step TEXTURE against the 16-step core drifts across the bar (polymeter).

### 6.3 SYNTH — the sound

The selected track's engine and its parameters (§7). Drum tracks show their voicing, tone, decay, drive and tune, and per-lane TUNE / DECAY / LEVEL.

### 6.4 MOD — movement and mix

LFO rate, depth, destination (PITCH, TONE, AMP, PAN) and shape (triangle, square, saw, sample & hold) for the selected track; its LEVEL, PAN, DLY SEND and BITCRUSH; and the shared master effects: DLY TIME (1/16 … 1/2, tempo-synced), DLY FDBK, DLY TONE, PINGPONG and MSTR DRV (master saturation).

### 6.5 GEN — composition

CORROSION's controls, exactly as on ETCH: KEY, SCALE, STYLE, OCTAVE, DENSITY, SYNCOP, ACCENT, SLIDE, MUTATE, MODE, SEED and the 12-digit CODE; then ETCH II's COMPAT (CLASSIC / EXPANDED) and X-SEED; and the actions GEN ALL, GEN BASS, GEN DRUM, GROOVE (the style's drum template), EVOLVE, UNDO, LOCK BAS, LOCK PAT, GEN EXT (compose the extension tracks).

### 6.6 MEMORY — what CORROSION remembers

DRIFT keeps an **anchor** for each of the last eight patterns it touched: the pattern as it was when DRIFT first met it, or when you last edited it. MEMORY lists them with their pattern, generation and how far the music has wandered from them, against DRIFT's limits. It also shows the evolution candidate waiting to be committed, how many evolutions have happened, how many stale candidates were refused, whether UNDO is available, the live sound drift and the entropy pool. FORGET THIS PATTERN makes the current version the new memory — exactly what an edit does on ETCH.

### 6.7 SONG — arrangement

A chain of up to 32 positions, each a pattern played 1–16 bars. MODE switches between song (follow the chain) and loop (repeat the current pattern; SELECT+↑/↓ queues another). BPM 60–220, SWING 0–8, GATE (note length), TUNE (±12 semitones) and START pattern. COPY / PASTE / CLEAR act on the current pattern (all undoable with SELECT+B).

### 6.8 FILE

ALBUM, LOAD, SAVE, NEW, NAME, SEED CODE, ABOUT and DIAGNOSTICS (§12, §15).

## 7. The synthesizers

Every melodic track can use any engine; the CHORD track plays three notes through one engine (paraphonic: one envelope, three oscillators). Shared controls: TONE (the main timbre), ENVMOD (how much the envelope moves TONE) and its decay, ATTACK / DECAY / SUSTAIN / RELEASE (loudness), ACCENT (how much accents add), GLIDE (slide time), DRIVE, OCTAVE and FINE tune.

| Engine | Character | A | B | TONE | C |
|---|---|---|---|---|---|
| **SUBTRACT** | the acid voice: band-limited saw / square / pulse / triangle into a resonant low-pass filter | WAVE | DETUNE (pulse: width) | CUTOFF | RESO |
| **FM 2-OP** | metallic bass, bells, hollow keys | RATIO (modulator) | FEEDBACK | INDEX | CARRIER ratio |
| **WAVETABL** | eight morphing tables: GLASS, VOICE, METAL, PULSAR, ORGAN, DIGIT, RUST, HOLLOW | TABLE | SPREAD (2nd oscillator) | POSITION | BRIGHT |
| **PHASE DS** | phase distortion: SAW, SQUARE, PULSE, RESO1, RESO2, DOUBLE | SHAPE | SKEW | AMOUNT | — |
| **RESONATR** | plucked / struck / bowed string or tube (Karplus–Strong) | EXCITE: PLUCK, MALLET, BOWED, GRAIN | — | BRIGHTNESS | SUSTAIN |

**Accent** works like ETCH's: an accented note is louder, sweeps TONE further and closes faster; consecutive accents build up a charge. **Slide** holds the gate and glides into the next note without restarting the envelopes.

**Drums.** The four original lanes keep ETCH's kit numbers, so CORROSION's kit choices mean the same thing on both machines: KICK — DEEP, PUNCH, THUD, CLICK · SNARE — SHORT, GRIT, CLAP, RIM · CLOSED HAT — CLOSD, TICK, SHAKE, SHARP · OPEN HAT — OPEN, METAL, BLIP, CRASH. Each is synthesised from a pitch-swept sine, filtered noise, a ring-modulated metal bank and clicks. VOICING: ANALOG, FM METAL (the tone is frequency-modulated) or DIGITAL (bit-reduced and driven). The closed hat chokes the open hat.

## 8. Modulation and effects

- **LFO** per track: four shapes, 0.03–24 Hz, to pitch (vibrato up to an octave), TONE (filter / index / morph sweeps), AMP (tremolo) or PAN (auto-pan). Sample & hold uses the track's own random stream.
- **DLY SEND** per track into one shared, tempo-synced stereo delay with a low-pass in its feedback (DLY TONE) and PINGPONG (0–7 stereo echoes, 8–15 ping-pong).
- **BITCRUSH** per track (sample-rate and bit reduction), **DRIVE** per voice, **MSTR DRV** on the whole mix, **PAN** per track.
- A limiter keeps the 8-bit output from clipping.

## 9. STABLE, DRIFT and CORRODE

| Mode | When | What happens |
|---|---|---|
| **STABLE** | never by itself | The pattern is not rewritten. Probability and conditions still vary playback. SELECT+A evolves once (as DRIFT). |
| **DRIFT** — "living song" | every 4 bars | Every channel moves a little: one or two small bass changes and one hit in each drum lane. Each channel stays within a bound of its anchor; at the bound, the change becomes a step back toward the anchor. The music breathes around itself and never wanders off. |
| **CORRODE** | every 2 bars | 2 + MUTATE/3 operations, including motif mutations that change a figure *and every repetition of it*, beat rotations, ghost notes and restoring repetition. The line keeps turning into new music descended from the original. |

Evolutions take effect exactly on a downbeat. After each one, the sound drifts too: the bass's tone and each drum's tuning take a small random step within narrow limits (never saved). **SELECT+B** always restores the previous generation.

### Understanding mutations

| Operator | What it does |
|---|---|
| ACC_MOVE | moves, adds or removes an accent, steered toward the ACCENT target |
| REST_TOGGLE | turns a note into a rest or a rest into a note (never the downbeat), steered toward the DENSITY target |
| NOTE_STEP | moves a note one scale step |
| OCTAVE | moves a note an octave |
| SLIDE | toggles a slide between two notes |
| DRUM | toggles a drum hit, preferring the style's variation positions |
| MEMORY | copies beat 1 onto the beat that has drifted furthest (restores repetition) |
| MOTIF | changes one step of the beat-1 motif and every repetition of it |
| ROTATE | shifts one beat by a sixteenth |
| PROB | turns a note into a ghost note or back |
| RECALL | restores one differing step or hit from the anchor |

**Locks.** A locked step (TRACK, L column), a locked drum lane (TRACK, B+↓), a locked bass line or a locked pattern (GEN) is never changed by GENERATE, REGEN or evolution.

**REGEN** replaces exactly half the bar — 8 steps chosen at random among unlocked ones — with fresh material in the current key, scale and style. **FILL** temporarily replaces the drums with a style-specific fill; the pattern itself is untouched.

## 10. Entering shared seed codes

A code looks like `0A51-430B-6B90`: **SSSS** seed · **K** key · **C** scale · **Y** style · **D** density · **P** syncopation · **A** accent · **L** slide · **O** octave.

*Worked example — hear OXIDE's core on either machine.* On ETCH II: FILE → SEED CODE. Set the digits to `0A51430B6B90` (←/→ choose a digit, A+↑/↓ change it) and press **A**. ETCH II switches to CLASSIC and generates the groove into the current pattern. On ETCH: GEN → CODE, enter the same digits, GEN ALL. Both machines now hold the same bass line and drums.

*Extended codes* have 16 digits: the 12 legacy digits plus 4 expansion-seed digits. In FILE → SEED CODE press B+→ for 16 digits. Importing one selects EXPANDED and also composes the extension tracks. The first 12 digits always work on ETCH.

## 11. Compatibility limitations

- A legacy code reproduces the **initial core groove** only: no edits, locks, sounds, extension tracks, song chain or evolution history. To keep those, save a project.
- **CLASSIC** plays only ETCH's material (bass + four drum lanes), through ETCH II's synthesizers. **EXPANDED** adds the extension tracks; they are never merged into ETCH's lanes and are simply absent on ETCH.
- Evolution follows CORROSION's rules exactly, but its random draws come from the moment-to-moment entropy pool, so two listens (and two machines) evolve differently.

## 12. Saving and loading

ETCH II saves to the cartridge's 32 KB battery SRAM: **four slots**, 8 KB each (a project is 7 230 bytes). FILE → SAVE → choose a slot → A (A again to replace an occupied slot). Every save is checked by reading it back (CRC-32). FILE → LOAD → slot → A, A. Missing slots show EMPTY; damaged ones show BAD and are never loaded — your current project stays as it was. Music keeps playing while you save. FILE → NAME renames the project.

## 13. Performance controls

Mute tracks on LIVE (A on a name). SELECT+A to evolve on the next bar, SELECT+B to undo, SELECT+START or B on LIVE for a fill, SELECT+↑/↓ to queue patterns, REGEN for half a bar of new material, ENGINE and TONE on LIVE to reshape the selected track while it plays, MODE and MUTATE to change how restless the piece is.

## 14. Technical specifications

| | |
|---|---|
| Platform | Nintendo Game Boy Advance (GBA, GBA SP, Game Boy Micro, flash cartridges, emulators) |
| ROM | 229 KB, native `.gba`, save type SRAM 32 KB |
| Audio | Direct Sound A (left) + B (right), 8-bit, **15 768 Hz**, locked to the video frame (264 samples per frame) |
| Voices | 5 melodic (BASS, LEAD, CHORD ×3 paraphonic, DRONE, TEXTURE) + 4 drum lanes; a CPU governor frees the least important voice before the CPU could overload |
| Timing | sample-clock sequencer, 1.5 ms resolution, no tempo drift (measured 1 584 vs 1 583.9 steps over 3 minutes) |
| CPU | album playback: 51–70 % average audio load, worst frame ≈ 95 % (measured in mGBA) |
| Data | 32 patterns × (64-byte ETCH pattern + 152-byte extension), 32-position song chain, 8 patches |
| Composer | CORROSION v2.4 (ETCH 2.5 source, unchanged) |

## 15. Troubleshooting

| Problem | What to do |
|---|---|
| No sound | Check the console volume; check no track is muted (LIVE, crossed name); FILE → DIAGNOSTICS shows the audio CPU and underruns. |
| A layer comes and goes in a dense arrangement | The CPU governor freed a voice to keep the audio continuous. DIAGNOSTICS → VOICES FREED counts them. Use fewer simultaneous layers, shorter releases, or the cheaper engines (FM, PD) for pads. |
| Extension tracks are silent | COMPAT is CLASSIC (header shows C). GEN → COMPAT → EXPANDED. |
| Evolution never happens | MODE is STABLE, or the pattern is locked (LOCK PAT on GEN). |
| SAVE FAILED | The cartridge has no working SRAM save. Flash carts need the save type set to SRAM (the ROM contains the standard `SRAM_V113` marker). |
| A slot shows BAD | Its data failed the CRC check and is not loaded; save over it. |
| A seed code is "INVALID" | Key must be 0–B, scale and style 0–5, octave 0–2. |

## 16. Credits

ETCH II — A Generative Synthesis Machine — by Synth Centric. Dan Schwebel, known as Synth Centric, is an Australian beat maker, music producer, writer and educator. CORROSION is from ETCH 2.5 (MIT licence). Startup code from devkitPro's devkitarm-crtls (MPL-2.0). All sounds are synthesised by the GBA at run time; there are no samples. See `ETCH_II_Controls.md` for every button and `ETCH_II_Overview.md` for how it works.
