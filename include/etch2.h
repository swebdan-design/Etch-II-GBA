/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Project data model.
 *
 * Two layers, kept apart on purpose:
 *   COMPOSITION  the ORIGINAL ETCH data model (pattern_t, gen_t from
 *                src/corrosion/legacy/etch.h, unchanged) plus ETCH II
 *                extension tracks (xpattern_t) that CORROSION never touches;
 *   RENDERING    per-track synthesizer patches (patch_t) that only describe
 *                how events sound.  Nothing here feeds back into composition.
 */
#ifndef ETCH2_H
#define ETCH2_H

#include "etch.h"          /* ORIGINAL ETCH v2.4/2.5 data model (verbatim) */
#include "corrosion.h"     /* ORIGINAL CORROSION API (verbatim)            */

#define ETCH2_VERSION_STR "1.0"
#define E2_PATTERNS   NUM_PATTERNS          /* 32, as ETCH */
#define E2_SONG_LEN   32

/* ---- logical tracks ------------------------------------------------------ */
enum {
    TRK_BASS,       /* ORIGINAL: CORROSION bass line (pattern_t note/flags/prob)  */
    TRK_LEAD,       /* ETCH II extension                                          */
    TRK_CHORD,      /* ETCH II extension (3-voice)                                */
    TRK_DRONE,      /* ETCH II extension                                          */
    TRK_KICK,       /* ORIGINAL: drum lane 0                                      */
    TRK_PERC,       /* ORIGINAL: drum lane 1 (snare / clap / rim)                 */
    TRK_HATS,       /* ORIGINAL: drum lanes 2 (closed) and 3 (open), kept apart   */
    TRK_TEXTURE,    /* ETCH II extension                                          */
    TRK_COUNT
};
#define TRK_IS_CORE(t)  ((t) == TRK_BASS || (t) == TRK_KICK || (t) == TRK_PERC || (t) == TRK_HATS)
#define TRK_IS_DRUM(t)  ((t) == TRK_KICK || (t) == TRK_PERC || (t) == TRK_HATS)
extern const char * const trk_names[TRK_COUNT];

/* extension track index (into xpattern_t.trk) */
enum { XT_LEAD, XT_CHORD, XT_DRONE, XT_TEXTURE, XT_COUNT };
extern const uint8_t xt_of_trk[TRK_COUNT];   /* 0xFF for core tracks */

/* ---- extension pattern data (never read or written by CORROSION) -------- */
/* flags: bits 0-1 type (STEP_REST/NOTE/TIE), STEP_ACCENT, STEP_SLIDE,
 * STEP_LOCK as in ETCH; bits 5-7 = probability class (0 = always,
 * n = plays with probability (8-n)/8). */
#define XF_PROB_SHIFT 5
#define XF_PROB_MASK  0xE0
typedef struct {
    uint8_t note[NUM_STEPS];     /* MIDI note number 12..108 */
    uint8_t flags[NUM_STEPS];
} xtrack_t;

#define CHORD_SHAPES 8
typedef struct {
    xtrack_t trk[XT_COUNT];      /* 128 bytes */
    uint8_t  shape[NUM_STEPS];   /* CHORD shape per step (0..7) */
    uint8_t  len[XT_COUNT];      /* independent track length 1..16 */
    uint8_t  present;            /* 1 = this pattern has extension content */
    uint8_t  spare[3];
} xpattern_t;                    /* 152 bytes */

/* ---- synthesizer patches (rendering only) -------------------------------- */
enum { ENG_SUB, ENG_FM, ENG_WT, ENG_PD, ENG_RES, ENG_COUNT };
extern const char * const eng_names[ENG_COUNT];

/* parameter slots (meaning depends on the engine; see src/instruments) */
enum {
    P_A, P_B, P_TONE, P_C, P_ENVMOD, P_MDECAY, P_ATTACK, P_DECAY, P_SUSTAIN,
    P_RELEASE, P_ACCENT, P_GLIDE, P_DRIVE, P_OCTAVE, P_FINE, P_COUNT
};
enum { LFO_TO_PITCH, LFO_TO_TONE, LFO_TO_AMP, LFO_TO_PAN, LFO_TO_COUNT };
enum { LFOS_TRI, LFOS_SQR, LFOS_SAW, LFOS_SH, LFOS_COUNT };

typedef struct {
    uint8_t engine;              /* ENG_* (drum tracks: voicing 0..2) */
    uint8_t p[P_COUNT];
    uint8_t level;               /* 0..15 */
    uint8_t pan;                 /* 0..15, 8 = centre */
    uint8_t send;                /* delay send 0..15 */
    uint8_t crush;               /* bit reduction 0..7 */
    uint8_t lfo_rate, lfo_depth, lfo_dest, lfo_shape;
} patch_t;                       /* 24 bytes */

typedef struct {
    uint8_t delay_time;          /* 0..7: 1/16 .. 1/2 note, tempo synced */
    uint8_t delay_fb;            /* 0..15 */
    uint8_t delay_tone;          /* 0..15 low-pass in the feedback path */
    uint8_t master_drive;        /* 0..7 */
    uint8_t width;               /* 0..15 stereo width of the delay */
    uint8_t spare[3];
} fx_t;

typedef struct {
    uint8_t pat[E2_SONG_LEN];    /* pattern index or 0xFF = end */
    uint8_t rep[E2_SONG_LEN];    /* bars 1..16 */
    uint8_t song;                /* 1 = follow the chain */
    uint8_t spare[3];
} song_t;

enum { COMPAT_CLASSIC, COMPAT_EXPANDED };

#define E2_NAME_LEN 10
typedef struct {
    char      name[E2_NAME_LEN];
    uint8_t   compat;            /* COMPAT_CLASSIC / COMPAT_EXPANDED */
    uint8_t   bpm;               /* 60..220 (ETCH semantics) */
    uint8_t   swing;             /* 0..8   (ETCH semantics) */
    uint8_t   gate;              /* 0..3   (ETCH semantics) */
    uint8_t   tune;              /* 0..24, 12 = no transpose (ETCH semantics) */
    uint8_t   mutes;             /* bit per logical track */
    uint8_t   start_pattern;
    uint8_t   album_id;          /* 0xFF = user project, else album piece */
    gen_t     gen;               /* ORIGINAL generator settings (12 bytes) */
    song_t    song;
    fx_t      fx;
    patch_t   patch[TRK_COUNT];
    uint8_t   drum_tune[NUM_LANES];     /* 0..15, 8 = as designed (ETCH v2 meaning) */
    uint8_t   drum_decay[NUM_LANES];
    uint8_t   drum_level[NUM_LANES];
    uint16_t  xseed;                    /* EXPANSION seed (extended share code digits 13-16) */
    uint8_t   spare[6];
    pattern_t pat[E2_PATTERNS];         /* ORIGINAL 64-byte patterns */
    xpattern_t xpat[E2_PATTERNS];       /* ETCH II extensions */
} project2_t;

extern project2_t proj;
/* runtime-only edit epoch per pattern: bumped by every edit, load, copy or
 * paste; an evolution candidate computed for an older epoch is discarded */
extern volatile uint8_t pat_epoch[E2_PATTERNS];

void project_defaults(project2_t *p);
void patch_default(patch_t *p, uint8_t trk);
void xpattern_clear(xpattern_t *x);

#endif
