# ETCH II — Controls

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

## Everywhere

| Buttons | Action |
|---|---|
| **START** | Play / stop |
| **L** / **R** | Previous / next screen (wraps: LIVE → TRACK → SYNTH → MOD → GEN → MEMORY → SONG → FILE → LIVE) |
| **SELECT + ←/→** | Previous / next screen (ETCH's shortcut) |
| **SELECT + A** | Evolve now (at the next bar line while playing; immediately when stopped) |
| **SELECT + B** | Undo: restore the previous generation of the pattern |
| **SELECT + START** | Fill on / off (latched; a fresh fill variation every bar) |
| **SELECT + ↑/↓** | Queue the next / previous pattern (switches song mode to loop mode) |
| **B + L / B + R** (or SELECT + L / R) | Previous / next track (the selected track is shared by LIVE, TRACK, SYNTH and MOD) |

## The editing grammar (ETCH's, on every screen)

| Buttons | Action |
|---|---|
| D-pad | Move the cursor |
| **A** (tap) | Place / toggle / do |
| **A held + ←/→** | Value −1 / +1 |
| **A held + ↑/↓** | Value + / − coarse (octave, ×4, ×10 BPM…) |
| **B + A** | Delete / reset to default |
| **B** (tap) | Back / cancel |

## LIVE

| Row | Buttons |
|---|---|
| Track name | ↑/↓ select track · A mute / unmute |
| Step cells | ←/→ move along the 16 steps · A toggle the step · A+←/→ semitone · A+↑/↓ octave · B+A clear |
| MODE | A+←/→ STABLE / DRIFT / CORRODE |
| MUTATE | A+←/→ ±1, A+↑/↓ ±4 |
| ENGINE | A+←/→ change the selected track's synthesizer (drum tracks: voicing) |
| TONE | A+←/→ the selected track's main timbre (cutoff / FM index / wavetable position / distortion / brightness) |
| REGEN | ←/→ ALL / BASS / DRUMS · A replace half the bar with new CORROSION material |
| B (tap) | Fill on / off (as ETCH 2.5) |

## TRACK

| Column | Buttons |
|---|---|
| NOTE | A cycles rest → note → tie → rest · A+←/→ semitone · A+↑/↓ octave · B+A clear |
| A, S, L | A toggles Accent, Slide, step Lock |
| P | A+←/→ probability 1–8 (out of 8) |
| CD (bass) | A+←/→ condition: —, 1:2, 2:2, 1:4, 2:4, 3:4, 4:4, FIL, !FL |
| SHAPE (chord) | A+←/→ TRIAD, SUS4, SEVEN, OPEN, SUS2, SIXTH, QUART, CLUST |
| drum hit columns | A toggle · A+←/→ change the lane's kit · B+↓ lock / unlock the lane |
| extension tracks | B+←/→ track length 1–16 |

## SYNTH and MOD

↑/↓ choose a row · A+←/→ change · A+↑/↓ coarse · B+A default.

## GEN

| Row | Buttons |
|---|---|
| KEY … MODE | A+←/→ change, A+↑/↓ coarse |
| SEED | A+←/→ ±1, A+↑/↓ ±256, **A** new random seed + GEN ALL |
| CODE | ←/→ choose a digit, A+↑/↓ (or A+←/→) change it. Entering a code switches to CLASSIC |
| COMPAT | A+←/→ CLASSIC / EXPANDED |
| X-SEED | A+←/→ change, **A** new expansion seed + GEN EXT |
| action rows | ←/→ choose, **A** do: GEN ALL, GEN BASS, GEN DRUM, GROOVE, EVOLVE, UNDO, LOCK BAS, LOCK PAT, GEN EXT |

## MEMORY

←/→ choose FORGET THIS PATTERN / FORGET ALL · A do it.

## SONG

| Area | Buttons |
|---|---|
| chain cells | ↑/↓ position, ←/→ pattern / repeats / next column · A+←/→ change (END ends the chain) · A jump to that position |
| right panel | ↑/↓ row · A+←/→ change MODE, BPM (A+↑/↓ ±10), SWING, GATE, TUNE, START · A on COPY / PASTE / CLEAR |

## FILE

↑/↓ choose · A open · B back. In LOAD and SAVE a second A confirms. In NAME: ←/→ move, A+↑/↓ change the letter. In SEED CODE: ←/→ digit, A+↑/↓ change it, B+←/→ 12 or 16 digits, A import + GEN ALL. In ABOUT: ←/→ page.
