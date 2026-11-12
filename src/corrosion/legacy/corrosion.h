/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * CORROSION: seeded, rule-based pattern generation and mutation.
 * Portable C (host-testable).  See docs/GENERATIVE_SYSTEM.md.
 */
#ifndef CORROSION_H
#define CORROSION_H

#include "etch.h"

#define GEN_BASS   0x01
#define GEN_DRUMS  0x02
#define GEN_ALL    0x03

/* deterministic PRNG (xorshift16) used for pattern generation/mutation */
void     corr_seed(uint16_t seed) ETCH_BANKED;
uint8_t  corr_rand(void) ETCH_BANKED;
uint8_t  corr_randn(uint8_t n) ETCH_BANKED;        /* 0..n-1 */

/* musical helpers */
void     corr_range(const gen_t *g, uint8_t *lo, uint8_t *hi) ETCH_BANKED;
uint8_t  corr_in_scale(uint8_t note, uint8_t key, uint8_t scale) ETCH_BANKED;
uint8_t  corr_snap(uint8_t note, const gen_t *g) ETCH_BANKED;
uint8_t  corr_scale_step(uint8_t note, int8_t dir, const gen_t *g) ETCH_BANKED;

/* Generate bass and/or drums into p from g (seed + parameters).
 * Locked steps / locked lanes / a locked bass line are preserved; the random
 * stream is consumed identically regardless of locks, so unlocked content is
 * fully reproducible from the seed code. */
void     corr_generate(pattern_t *p, const gen_t *g, uint8_t what) ETCH_BANKED;

/* Produce one evolution step: dst = mutated copy of src.
 * anchor (may be NULL): the "musical memory" -- the pattern as it was when
 * evolution began or was last edited by hand; DRIFT is pulled back toward it.
 * mode: EVO_DRIFT (restrained) or EVO_CORRODE (aggressive); intensity 0..15.
 * Returns the number of steps/events that actually changed. */
uint8_t  corr_mutate(pattern_t *dst, const pattern_t *src, const pattern_t *anchor,
                     const gen_t *g, uint8_t mode, uint8_t intensity, uint16_t seed) ETCH_BANKED;

/* REGEN: replace exactly half of the pattern (8 of 16 steps, chosen at
 * random among unlocked steps) with freshly generated material in the GEN
 * key / scale / style.  what = GEN_BASS, GEN_DRUMS or GEN_ALL.  Locked steps,
 * locked lanes and a locked bass line are never touched; if fewer than 8
 * steps are free, all free steps are replaced.  Returns the mask of replaced
 * steps (bit n = step n).  A slide leading into a replaced step may be
 * cleared by sanitising if it no longer glides into a note. */
uint16_t corr_regen(pattern_t *p, const gen_t *g, uint16_t seed, uint8_t what) ETCH_BANKED;
/* the two halves of corr_regen, so the expensive part (composing a fresh
 * bar) can be done ahead of time and the tap itself is instant */
void     corr_regen_material(pattern_t *mat, const gen_t *g, uint16_t seed) ETCH_BANKED;
uint16_t corr_regen_apply(pattern_t *p, const pattern_t *mat, uint16_t pick_seed, uint8_t what) ETCH_BANKED;

/* Temporary fill drum masks derived from p's groove. */
void     corr_fill(const pattern_t *p, uint16_t rnd, uint16_t *out) ETCH_BANKED;

/* Seed code: 12 hex digits = seed(4) key scale style density sync accent slide octave */
void     corr_code_encode(const gen_t *g, char *out12) ETCH_BANKED;
uint8_t  corr_code_decode(gen_t *g, const char *in12) ETCH_BANKED;

/* Apply the base drum groove of a style (preset grooves on the DRUM screen). */
void     corr_drum_preset(pattern_t *p, uint8_t style) ETCH_BANKED;

extern const char * const corr_style_names[STYLE_COUNT];
extern const char * const corr_scale_names[SCALE_COUNT];
extern const char * const corr_mode_names[EVO_COUNT];

#endif
