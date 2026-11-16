/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * The canvas: BG0 maps each of the 30 x 20 screen cells to its own 4bpp
 * tile (tiles 0..599), so the screen is a 240 x 160 bitmap we draw into
 * directly in VRAM.  The look follows the series spec (SERIES_SPEC.md 4):
 * colour roles, ETCH DS's two pixel fonts, hairlines, one accent.
 *
 * Draw only what changes: the mixer shares this CPU.
 */
#ifndef CANVAS_H
#define CANVAS_H
#include <stdint.h>

/* colour roles = palette 0 indices (SERIES_SPEC.md 4.1) */
enum {
    CV_BG, CV_SURF, CV_SURF2, CV_SURF3, CV_BORDER,
    CV_TEXT, CV_TEXT2, CV_TEXT3, CV_NOTE, CV_HOT, CV_BEAT,
    CV_ACCENT, CV_ACC_TINT, CV_ACC_FADE, CV_SCOPE, CV_ROLES
};
enum { F_SMALL, F_MAIN };           /* 3x5 caps, 5x7 caps */
#define FONT_H(f) ((f) == F_SMALL ? 5 : 7)

#define CV_W 240
#define CV_H 160

void cv_init(void);
void cv_clear(uint8_t c);
void cv_point(int x, int y, uint8_t c);
void cv_rect(int x, int y, int w, int h, uint8_t c);
void cv_hline(int x, int y, int w, uint8_t c);
void cv_vline(int x, int y, int h, uint8_t c);
void cv_frame(int x, int y, int w, int h, uint8_t c);
void cv_rrect(int x, int y, int w, int h, uint8_t c);     /* corners cut by 1 px */
void cv_line(int x0, int y0, int x1, int y1, uint8_t c);

int  cv_text_w(int font, const char *s);
int  cv_text_scaled(int x, int y, int font, int scale, uint8_t c, const char *s);   /* wordmarks */
int  cv_text(int x, int y, int font, uint8_t c, const char *s);   /* returns x after */
void cv_text_c(int cx, int y, int font, uint8_t c, const char *s);
void cv_text_r(int rx, int y, int font, uint8_t c, const char *s);
/* a number, space- or zero-padded to `digits` (0: as short as possible) */
int  cv_num(int x, int y, int font, uint8_t c, uint16_t v, uint8_t digits, uint8_t zero);
void cv_numfmt(char *b, uint16_t v, uint8_t digits, uint8_t zero);
/* text clipped/cleared to a box w wide (background bg): for values that change */
void cv_field(int x, int y, int w, int font, uint8_t c, uint8_t bg, const char *s);
void cv_field_r(int x, int y, int w, int font, uint8_t c, uint8_t bg, const char *s);

/* the hot paths, in IWRAM (canvas_fast.iwram.c) */
void cv_rect_fast(int x, int y, int w, int h, uint8_t c) __attribute__((long_call));
int  cv_text_fast(int x, int y, int font, uint8_t c, const char *s) __attribute__((long_call));

/* the spec's parts (SERIES_SPEC.md 4.5) */
void cv_header(void);                        /* ETCH II | song ... seed code */
void cv_tabs(uint8_t cur);                   /* the screen bar */
void cv_hint(const char *s, uint8_t msg);    /* bottom line: hint (TEXT2) or message (TEXT) */
#define CV_TOP 26                            /* first free y under header + tabs */
#define CV_HINT_Y 153                        /* y of the hint line */

/* widgets: a row caption (accent marker when selected), a slider, a button
 * (lit = accent fill; sel = accent outline), a caption-over-value readout */
void cv_cap(int x, int y, uint8_t sel, const char *cap);
void cv_slider(int x, int y, int w, uint16_t v, uint16_t max, uint8_t sel);
void cv_button(int x, int y, int w, int h, const char *label, uint8_t sel, uint8_t lit);
void cv_readout(int x, int y, int w, const char *cap, const char *val);

/* copy one 8x8 tile from a 4bpp source, remapping its colours 0..3 */
void cv_blit_tile(int cx, int cy, const uint32_t *src, const uint8_t map[4]);
#endif
