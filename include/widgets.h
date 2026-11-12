/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Shared UI widgets.
 */
#ifndef WIDGETS_H
#define WIDGETS_H
#include "ui.h"
void w_step_cell(uint8_t x, uint8_t y, uint8_t trk, uint8_t pat, uint8_t s, uint8_t cursor);
void w_track_row(uint8_t y, uint8_t trk, uint8_t pat, int8_t cur_col);
void w_scope_map(uint8_t x0, uint8_t y0);
void w_scope_draw(void);
const char *eng_short(uint8_t trk);
#endif
