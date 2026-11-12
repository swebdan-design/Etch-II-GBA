/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * CORROSION bridge: the main-loop side of evolution (ported from ETCH's
 * src/main.c), REGEN, FILL, UNDO and the entropy pool.
 */
#ifndef EVOLVE_H
#define EVOLVE_H
#include "etch2.h"

#define N_ANCHORS 8
extern pattern_t anchors[N_ANCHORS];
extern uint8_t anchor_id[N_ANCHORS];
extern uint32_t ui_entropy;               /* stirred by every button press */
extern uint16_t evo_rng_peek;             /* last evolution random value (MEMORY) */

void evo_init(void);
void evo_stir(uint32_t x);
uint16_t evo_random(void);
uint8_t evo_service(void);                 /* prepare the next candidate (main loop) */
uint8_t regen_service(void);
void evo_after_commit(void);               /* sound drift + message (main loop) */
void evo_stable_reset(void);
void anchors_forget(uint8_t pat);          /* 0xFF = all */
void app_pattern_edited(uint8_t pat);      /* an edit becomes the new memory */
uint16_t app_regen(uint8_t what);
void app_evolve_now(void);
uint8_t app_undo(void);
void app_fill(void);
void app_fill_toggle(void);
void app_fill_service(void);               /* latched fill: fresh variation per bar */
void app_generate(uint8_t what);           /* GEN ALL / BASS / DRUMS (+ expansion) */
void app_project_loaded(void);
uint8_t anchor_divergence(uint8_t slot);   /* steps + hits differing from the anchor */

#endif
