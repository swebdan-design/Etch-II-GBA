/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Instrument definitions shared by the UI, presets and host tools.
 */
#ifndef INSTRUMENTS_H
#define INSTRUMENTS_H
#include "etch2.h"

#define ENG_DEFS (ENG_COUNT + 1)           /* + the drum-track layout */
typedef struct { char label[7]; uint8_t max; } paramdef_t;

extern const paramdef_t param_defs[ENG_DEFS][P_COUNT];
extern const char * const voicing_names[3];
extern const char * const lfo_dest_names[LFO_TO_COUNT];
extern const char * const lfo_shape_names[LFOS_COUNT];
extern const char * const chord_names[CHORD_SHAPES];

void param_text(uint8_t def, uint8_t slot, uint8_t v, char *out4);
uint8_t param_def_of(const patch_t *pt, uint8_t trk);

#endif
