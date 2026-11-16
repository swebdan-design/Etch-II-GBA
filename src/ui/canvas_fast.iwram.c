/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * The canvas's two hot paths, in ARM code in IWRAM: filling rectangles and
 * drawing text.  Both work on whole 32-bit tile rows (8 pixels at a time)
 * with a nibble mask, instead of pixel by pixel.
 */
#include <stdint.h>
#include "canvas.h"

#define TILES ((volatile uint32_t *)0x06000000)
#define FILL(c) ((uint32_t)(c) * 0x11111111u)

extern const uint8_t font_small_w[64], font_small_rows[];
extern const uint8_t font_main_w[64], font_main_rows[];

/* one glyph-row byte (bit 7 = leftmost pixel) as a nibble mask, low pixel first */
static uint32_t expand(uint8_t b)
{
    uint32_t m = 0;
    int i;
    for (i = 0; i < 8; i++) if (b & (0x80 >> i)) m |= 15u << (i * 4);
    return m;
}

/* write fill colour f into the pixels of mask m, starting at pixel x of row y
 * (m covers up to 8 pixels; it may straddle two tile words) */
static inline void span(int x, int y, uint32_t m, uint32_t f)
{
    int sh = (x & 7) * 4;
    volatile uint32_t *p = &TILES[((y >> 3) * 30 + (x >> 3)) * 8 + (y & 7)];
    uint32_t lo = m << sh;
    *p = (*p & ~lo) | (f & lo);
    if (sh && (x >> 3) < 29) {
        uint32_t hi = m >> (32 - sh);
        if (hi) { p += 8; *p = (*p & ~hi) | (f & hi); }
    }
}

void cv_rect_fast(int x, int y, int w, int h, uint8_t c)
{
    int yy, x1, tx;
    uint32_t f = FILL(c);
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > CV_W) w = CV_W - x;
    if (y + h > CV_H) h = CV_H - y;
    if (w <= 0 || h <= 0) return;
    x1 = x + w - 1;
    for (tx = x >> 3; tx <= x1 >> 3; tx++) {
        int a = tx * 8 > x ? 0 : x & 7, b = tx * 8 + 7 < x1 ? 7 : x1 & 7;
        uint32_t m = (b == 7 ? 0xFFFFFFFFu : ((1u << ((b + 1) * 4)) - 1)) & ~((1u << (a * 4)) - 1);
        volatile uint32_t *p = &TILES[((y >> 3) * 30 + tx) * 8 + (y & 7)];
        for (yy = y; yy < y + h; yy++) {
            *p = m == 0xFFFFFFFFu ? f : ((*p & ~m) | (f & m));
            p = ((yy + 1) & 7) ? p + 1 : p + 30 * 8 - 7;
        }
    }
}

int cv_text_fast(int x, int y, int font, uint8_t c, const char *s)
{
    int h = font == F_SMALL ? 5 : 7;
    uint32_t f = FILL(c);
    if (y < 0 || y + h > CV_H) return x;
    for (; *s; s++) {
        char ch = *s;
        const uint8_t *r;
        int w, j;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        if (ch < 32 || ch > 95) ch = '?';
        if (font == F_SMALL) { r = &font_small_rows[(ch - 32) * 5]; w = font_small_w[ch - 32]; }
        else { r = &font_main_rows[(ch - 32) * 7]; w = font_main_w[ch - 32]; }
        if (x >= 0 && x + w <= CV_W && ch != ' ')
            for (j = 0; j < h; j++) if (r[j]) span(x, y + j, expand(r[j]), f);
        x += w + 1;
    }
    return x;
}
