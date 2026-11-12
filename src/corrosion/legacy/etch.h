/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Core data model shared by the audio engine, sequencer, CORROSION generator,
 * UI and storage.  This header is portable C so that the generator can be
 * compiled and unit-tested on a host machine (see tests/).
 */
#ifndef ETCH_H
#define ETCH_H

#include <stdint.h>
#include <string.h>

#ifdef __SDCC
#include <gbdk/platform.h>
#define ETCH_BANKED BANKED
#else
#define ETCH_BANKED
#define BANKREF(x)
#define BANKREF_EXTERN(x)
#endif

#define ETCH_VERSION_STR "2.5"

/* ---- dimensions -------------------------------------------------------- */
#define NUM_STEPS      16
#define NUM_PATTERNS   32
#define NUM_LANES      4
#define CHAIN_LEN      16
#define NUM_SLOTS      8
#define NUM_TRACKS     8          /* ROM preset tracks */

/* Bass note numbers: 0 = C1 (32.7 Hz) ... 47 = B4 */
#define NOTE_MIN       0
#define NOTE_MAX       47

/* ---- bass step flags --------------------------------------------------- */
#define STEP_TYPE_MASK 0x03
#define STEP_REST      0x00      /* silence (previous note released)        */
#define STEP_NOTE      0x01      /* new note                                */
#define STEP_TIE       0x02      /* sustain previous note, no retrigger     */
#define STEP_ACCENT    0x04
#define STEP_SLIDE     0x08      /* hold gate and glide into the next note  */
#define STEP_LOCK      0x10      /* protected from CORROSION                */

/* prob byte: low nibble = probability 0..7 ((p+1)/8), high nibble = cond */
#define PROB_ALWAYS    7
#define COND_ALL       0
#define COND_1OF2      1
#define COND_2OF2      2
#define COND_1OF4      3
#define COND_2OF4      4
#define COND_3OF4      5
#define COND_4OF4      6
#define COND_FILL      7
#define COND_NOFILL    8
#define COND_COUNT     9

/* pattern lock byte */
#define PLOCK_BASS     0x01
#define PLOCK_LANE0    0x02      /* << lane */
#define PLOCK_ALL      0x80

/* drum lanes */
#define LANE_KICK      0
#define LANE_SNARE     1
#define LANE_CH        2
#define LANE_OH        3

/* ---- pattern: exactly 64 bytes ----------------------------------------- */
typedef struct {
    uint8_t  note[NUM_STEPS];
    uint8_t  flags[NUM_STEPS];
    uint8_t  prob[NUM_STEPS];
    uint16_t drum[NUM_LANES];    /* bit i = step i                         */
    uint8_t  kit[NUM_LANES];     /* drum sound per lane                     */
    uint8_t  lock;               /* PLOCK_*                                 */
    uint8_t  style;              /* rhythmic grammar this pattern follows   */
    uint8_t  gen;                /* evolution generation counter            */
    uint8_t  spare;
} pattern_t;

/* ---- instrument (project-wide, shared by all patterns) ----------------- */
enum { WAVE_SAW, WAVE_SQUARE, WAVE_HOLLOW, WAVE_BITE, WAVE_COUNT };
enum { DRIVE_OFF, DRIVE_SOFT, DRIVE_HARD, DRIVE_COUNT };
enum { LFO_OFF, LFO_PITCH, LFO_CUT, LFO_RES, LFO_DEST_COUNT };
enum { LFO_TRI, LFO_SQR, LFO_SAW, LFO_SH, LFO_SHAPE_COUNT };

typedef struct {
    uint8_t wave;       /* 0..3   */
    uint8_t cutoff;     /* 0..31  */
    uint8_t res;        /* 0..7   */
    uint8_t envmod;     /* 0..15  */
    uint8_t decay;      /* 0..15  */
    uint8_t accent;     /* 0..15  */
    uint8_t drive;      /* 0..2   */
    uint8_t slide;      /* 0..15  slide time */
    uint8_t lfo_rate;   /* 0..15  */
    uint8_t lfo_depth;  /* 0..15  */
    uint8_t lfo_dest;   /* 0..3   */
    uint8_t lfo_shape;  /* 0..3   */
    uint8_t bpm;        /* 60..220 */
    uint8_t swing;      /* 0..8   */
    uint8_t tune;       /* 0..24, 12 = no transpose */
    uint8_t gate;       /* 0..3  note length */
} synth_t;

/* ---- CORROSION generator settings -------------------------------------- */
enum { STYLE_ACID, STYLE_ELECTRO, STYLE_MINIMAL, STYLE_BROKEN,
       STYLE_INDUSTRIAL, STYLE_SPARSE, STYLE_COUNT };
enum { SCALE_MINOR, SCALE_MAJOR, SCALE_DORIAN, SCALE_PHRYGIAN,
       SCALE_MINPENT, SCALE_CHROMATIC, SCALE_COUNT };
enum { EVO_STABLE, EVO_DRIFT, EVO_CORRODE, EVO_COUNT };

typedef struct {
    uint8_t  key;        /* 0..11  */
    uint8_t  scale;      /* 0..5   */
    uint8_t  style;      /* 0..5   */
    uint8_t  density;    /* 0..15  */
    uint8_t  sync;       /* 0..15  syncopation */
    uint8_t  accent;     /* 0..15  accent amount */
    uint8_t  slide;      /* 0..15  slide amount */
    uint8_t  octave;     /* 0..2   */
    uint8_t  mutation;   /* 0..15  intensity */
    uint8_t  mode;       /* EVO_*  */
    uint16_t seed;
} gen_t;

typedef struct {
    uint8_t pat[CHAIN_LEN];      /* pattern index or 0xFF = end           */
    uint8_t rep[CHAIN_LEN];      /* bars 1..8                              */
    uint8_t song;                /* 1 = follow the chain                   */
} chain_t;

#define PROJECT_NAME_LEN 10
typedef struct {
    char      name[PROJECT_NAME_LEN];
    synth_t   synth;
    gen_t     gen;
    chain_t   chain;
    uint8_t   mutes;             /* bit0 bass, bit1..4 lanes               */
    uint8_t   start_pattern;
    pattern_t pat[NUM_PATTERNS];
    /* drum voicing per lane (SYNTH screen); added in save format v2 */
    uint8_t   drum_tune[NUM_LANES];    /* 0..15, 8 = as designed */
    uint8_t   drum_decay[NUM_LANES];   /* 0..15, 8 = as designed */
    uint8_t   drum_level[NUM_LANES];   /* 0..15, 15 = full       */
} project_t;
#define PROJECT_V1_SIZE (sizeof(project_t) - 3 * NUM_LANES)

void project_drum_defaults(project_t *p) ETCH_BANKED;

/* ---- portable pattern helpers (src/generator/pattern.c) ---------------- */
void pattern_clear(pattern_t *p) ETCH_BANKED;
void pattern_sanitize(pattern_t *p) ETCH_BANKED;
uint8_t pattern_note_count(const pattern_t *p) ETCH_BANKED;

#endif
