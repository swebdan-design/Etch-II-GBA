# ETCH series spec

**ETCH (Game Boy) · ETCH II (Game Boy Advance) · ETCH DS · ETCH 3DS — by Synth Centric**

This is the one contract the whole series follows: the controls, the look, the words and the seed codes. ETCH DS holds the master copy, in `docs/SERIES_SPEC.md`. Every other repository carries an identical copy, and its `make` checks it against `SERIES_SPEC.sha256`, the same way CORROSION is shared. To change the spec, change it here, then copy it and its checksum to the other repositories in the same release.

A machine may **add** to the spec where its hardware has more (touch, shoulder buttons, colour). It may never give an existing button or word a different meaning.

Spec version: **1** (2026-11-16)

## 1. Pillars

1. **Press START, hear music.**
2. **The 303 is the centre;** every other voice frames the bass.
3. **CORROSION is the composer,** byte for byte on every machine. A 12-digit seed code makes the same groove everywhere.
4. **Buttons edit, touch performs.** Nothing *needs* touch.
5. **Corroded, not clean:** a minimal black and white face with one accent colour.
6. **One project, every machine.**

## 2. Controls

### 2.1 Everywhere

| Buttons | Action | GB | GBA | DS | 3DS |
|---|---|:-:|:-:|:-:|:-:|
| START | Play / stop | ✓ | ✓ | ✓ | ✓ |
| SELECT + ← / → | Previous / next screen (wraps) | ✓ | ✓ | ✓ | ✓ |
| SELECT + ↑ / ↓ | Queue the next / previous pattern (at the bar line; at once when stopped). Song mode turns off. | ✓ | ✓ | ✓ | ✓ |
| SELECT + A | Evolve now (at the next bar line; at once when stopped) | ✓ | ✓ | ✓ | ✓ |
| SELECT + B | Undo the last evolution, generation, groove load or clear | ✓ | ✓ | ✓ | ✓ |
| SELECT + START | Fill on / off (latched: a fresh fill every bar until switched off) | ✓ | ✓ | ✓ | ✓ |
| hold R | Fill while held | — | ✓ | ✓ | ✓ |
| B + L / R | Previous / next track (machines with more than one melodic track) | — | ✓ | — | ✓ |
| hold L while dragging | Temporary move: springs back on release (touch machines) | — | — | ✓ | ✓ |
| tap the tab bar | Go to that screen (touch machines) | — | — | ✓ | ✓ |

L and R never change screens.

### 2.2 The editing grammar (every screen)

| Buttons | Meaning |
|---|---|
| D-pad | Move the cursor |
| **A** (tap) | Place / toggle / do. On an empty step A writes the last note; on a filled one it changes it. |
| **A held + ← / →** | Value −1 / +1. **On a note: a semitone.** |
| **A held + ↑ / ↓** | Value up / down coarse. **On a note: an octave.** |
| **B + A** | Delete, or reset to default |
| **B** (tap) | Back / cancel. On LIVE: fill on / off. |
| B + D-pad | Jump (a beat in TRACK, a channel in SYNTH) |

On list screens, plain ← / → also changes the value, so a control can be swept without holding A.

Accent and slide are set in TRACK on every machine (columns A and S). Touch machines may also set them by touch on LIVE.

## 3. Screens

### 3.1 Order and names

All machines use this order, and screen changes wrap from the last to the first. A machine leaves out the screens it does not have and inserts its own extras in the slots shown.

| # | Screen | What it is | GB | GBA | DS / 3DS |
|---|---|---|:-:|:-:|:-:|
| 1 | **LIVE** | Home: the pattern, the macros or pads, performance | ✓ | ✓ | ✓ |
| 2 | **TRACK** | Tracker step editor | ✓ | ✓ | (in LIVE) |
| — | *XY* | XY pad (touch machines) | | | ✓ |
| 3 | **SYNTH** | The sound of each channel (page 2 on DS/3DS: DRUM) | ✓ | ✓ | ✓ |
| — | *MOD* | Modulation (GBA) | | ✓ | |
| — | *KEYS* | Keyboard (touch machines) | | | ✓ |
| — | *HARMONY* | OXIDE (3DS) | | | 3DS |
| 4 | **GEN** | CORROSION | ✓ | ✓ | ✓ |
| — | *MEMORY* | Forgetting (GBA) | | ✓ | |
| 5 | **SONG** | Tempo, swing and the chain | ✓ | ✓ | ✓ |
| 6 | **FILE** | New, load, save, seed code, ABOUT | ✓ | ✓ | ✓ (page 2 of SONG) |

### 3.2 Words

Use these words, in capitals, everywhere:

* Modes: **STABLE · DRIFT · CORRODE**
* LIVE pads and actions: **FILL · REGEN · EVOLVE · UNDO**
* GEN actions: **GEN ALL · GEN BASS · GEN DRUMS · GROOVE · EVOLVE · UNDO · LOCK BASS · LOCK PATT**
* Readouts: **MODE · PATTERN · GENERATION · NEXT EVO · TEMPO**
* Drum lanes: **KICK · SNARE · HAT · OPEN** (three-letter short forms where space is tight: **KCK SNR HAT OPN**). The bass is **BASS** (**BAS**).
* The composer is **CORROSION**. Its version is named by the ETCH release it comes from: "CORROSION FROM ETCH 2.5".

### 3.3 Title and ABOUT

* **Title:** the name, its subtitle ("A generative acid machine", or on ETCH II "A generative synthesis machine"), "by Synth Centric", the version, and PRESS START. START goes straight into the music.
* **ABOUT** (in FILE) has four pages in this order: the machine; generative music; Synth Centric; version, credits and licences. ← / → turns the pages and B returns.

## 4. The look

### 4.1 Colour roles

Code never uses a raw colour, only a role. Each machine maps the roles onto what it can show.

| Role | Meaning | DS / GBA / GBC (BGR555) | 3DS (RGB) | GB (DMG shade) |
|---|---|---|---|---|
| `BG` | Screen | `C(31,31,31)` | `#FFFFFF` | 0 white |
| `SURF` | Panels, pads | `C(29,29,29)` | `#EFEFEF` | 0 white |
| `SURF2` | Off cells, controls | `C(27,27,27)` | `#DEDEDE` | 1 light |
| `SURF3` | Tracks, beat ticks | `C(23,23,23)` | `#BDBDBD` | 1 light |
| `BORDER` | Hairlines | `C(24,24,24)` | `#C5C5C5` | 1 light |
| `TEXT` | Values | `C(2,2,2)` | `#101010` | 3 black |
| `TEXT2` | Captions | `C(13,13,13)` | `#6B6B6B` | 2 dark |
| `TEXT3` | Disabled, hints | `C(21,21,21)` | `#ADADAD` | 1 light |
| `NOTE` | Notes and hits | `C(11,11,11)` | `#5A5A5A` | 2 dark |
| `HOT` | Accented notes, the kick | `C(0,0,0)` | `#000000` | 3 black |
| `BEAT` | Every 4th step | `C(28,28,28)` | `#E6E6E6` | 0 white |
| **`ACCENT`** | **"Happening now"** (see 4.2) | **`C(30,10,4)`** | **`#F45423`** | 3 black |
| `ACC_TINT` | Accent wash: playhead column, fresh CORROSION change | `C(31,27,25)` | `#FDDCCF` | 1 light |
| `ACC_FADE` | Accent wash, fading | `C(31,29,28)` | `#FEEDE7` | 0 white |

The series accent is **#F45423**. On BGR555 hardware it is `C(30,10,4)`, which shows as #F75221.

### 4.2 The accent

The accent means "happening now", and is used for nothing else:

* the playhead, and the play indicator
* the cursor, and the selected row
* the underline on the current screen's tab
* a lit pad or latched function (FILL on, REC)
* steps CORROSION has just changed: `ACC_TINT`, fading through `ACC_FADE` to nothing
* warnings

Kick and accented notes stay `HOT` (black), so the accent always reads as "now".

**Contrast.** #F45423 on white is about 3.4 : 1, below the 4.5 : 1 needed for small text. **Never set text in the accent.** Use it for fills, bars, outlines and washes. Text on an accent fill is `TEXT` (black, about 6 : 1), never white.

### 4.3 Type

Two pixel fonts, both from ETCH DS (`tools/gen_fonts.py`), in capitals:

* **SMALL**, 3×5 proportional small caps, for captions and labels, in `TEXT2`
* **MAIN**, 5×7 proportional, for values and titles, in `TEXT`

Hierarchy comes from weight and shade, not size or colour. On the Game Boy, where text sits on an 8×8 grid, MAIN is used for both; captions are told apart by shade (`TEXT2`).

### 4.4 Shape and space

* Flat surfaces, 1 px hairlines in `BORDER`, corners rounded by 1–2 px.
* A 6 px outer margin and 4 px gaps where the screen allows (GB: one tile).
* Boxes are kept to a minimum: readouts sit on the page, separated by hairlines.
* Lit buttons fill with `ACCENT` (text stays `TEXT`).

### 4.5 The parts

Every screen is built from these parts, in this order from the top where they appear:

1. **Header:** `ETCH <machine>` in `TEXT`, then `|` and the song name in `TEXT2`; the seed code on the right in `TEXT`. Then a hairline.
2. **Screen bar:** the screen names in SMALL, the current one in `TEXT` with a 2 px `ACCENT` underline, the others in `TEXT3`. Where the names don't fit (GB), show `‹ NAME ›` with the neighbours implied by the arrows.
3. **The step grid:** a label column (SMALL, `TEXT2`; `TEXT` on the cursor's lane), then 16 cells.
   * Off cells `SURF2`; hits `NOTE`; kick and accents `HOT`; rounded corners.
   * Every 4th step on a `BEAT` column; the playhead on an `ACC_TINT` column.
   * Fresh CORROSION changes on `ACC_TINT`, fading.
   * The bass as pitch bars where the screen allows (GBA, DS, 3DS), with an A/S strip beneath; as cells on the GB.
   * The cursor is a 1 px `ACCENT` outline.
4. **Readout strip:** MODE · PATTERN · GENERATION · NEXT EVO · TEMPO, captions in SMALL `TEXT2` over values in MAIN `TEXT`, separated by hairlines.
5. **Controls:** pads (FILL · REGEN · EVOLVE · UNDO) or macro rows. Macro rows: caption in `TEXT2`, a 1 px `SURF3` track with a `NOTE` fill and a `TEXT` thumb (an `ACCENT` thumb on the selected row), value right-aligned in `TEXT`.
6. **Hint line** (machines without touch): one line in SMALL `TEXT2` saying what the buttons do here. Messages replace it for a moment, in `TEXT`.

## 5. Seed codes and projects

* **The 12-digit code** (`0F1C-9009-5870`) is CORROSION's on every machine, always shown in groups of four separated by `-`.
* **The 16-digit code** adds a fourth group, the **expansion seed** (ETCH II's X-SEED). The first 12 digits never change meaning, so a 16-digit code still works on ETCH, which reads only the first 12.
  * ETCH II uses it to compose its extension tracks (LEAD, CHORD, DRONE, TEXTURE).
  * ETCH 3DS seeds OXIDE (its chord generator) from the same four digits, so one 16-digit code gives the same extra material's *seed* on both. OXIDE keeps its own random stream and never touches CORROSION's.
  * No machine invents another code format.
* **Projects.** ETCH's `project_t` is the common core of every save. ETCH DS and 3DS share one save file (`/etch_ds/etch_ds.sav`) and import ETCH saves (`etch_gb.sav`) and ETCH II saves (`etch_gba.sav`) read-only.

## 6. Versions

| Machine | Version after this spec |
|---|---|
| ETCH (Game Boy) | 3.0 |
| ETCH II (Game Boy Advance) | 2.0 |
| ETCH DS | milestone numbering until its first release |
| ETCH 3DS | milestone numbering until its first release |
