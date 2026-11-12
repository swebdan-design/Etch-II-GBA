/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * EXPANSION: ETCH II's own generator for the extension tracks.
 *
 * This is NOT CORROSION.  It writes only xpattern_t (LEAD, CHORD, DRONE,
 * TEXTURE), reads the core pattern to complement it, and draws every random
 * number from its own xorshift32 stream seeded from (legacy seed, expansion
 * seed, pattern variant).  It never calls corr_seed / corr_rand / corr_randn,
 * so CORROSION's composition stream is untouched (tests/host checks this).
 */
#ifndef EXPAND_H
#define EXPAND_H
#include "etch2.h"

#define XG_LEAD    0x01
#define XG_CHORD   0x02
#define XG_DRONE   0x04
#define XG_TEXTURE 0x08
#define XG_ALL     0x0F

void expand_generate(xpattern_t *x, const pattern_t *core, const gen_t *g,
                     uint16_t xseed, uint8_t variant, uint8_t which);
uint32_t expand_stream_seed(const gen_t *g, uint16_t xseed, uint8_t variant);

/* extended share code: the 12 legacy digits + 4 expansion digits */
void ext_code_encode(const gen_t *g, uint16_t xseed, char *out16);
uint8_t ext_code_decode(gen_t *g, uint16_t *xseed, const char *in16);

#endif
