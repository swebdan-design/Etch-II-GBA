/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * The canvas (see canvas.h): pixels drawn straight into BG0's tiles.
 */
#include <string.h>
#include "gba.h"
#include "canvas.h"
#include "ui.h"
#include "etch2.h"
#include "seq.h"
#include "expand.h"

extern const uint8_t font_small_w[64], font_small_rows[];
extern const uint8_t font_main_w[64], font_main_rows[];

#define TILES ((volatile uint32_t *)0x06000000)      /* CBB 0 (+1): tiles 0..599 */
#define MAP   SCREENBLOCK(30)
#define FILL(c) ((uint32_t)(c) * 0x11111111u)

/* SERIES_SPEC.md 4.1, BGR555 */
#define C(r, g, b) ((uint16_t)((r) | ((g) << 5) | ((b) << 10)))
static const uint16_t roles[16] = {
    C(31, 31, 31),   /* BG       */
    C(29, 29, 29),   /* SURF     */
    C(27, 27, 27),   /* SURF2    */
    C(23, 23, 23),   /* SURF3    */
    C(24, 24, 24),   /* BORDER   */
    C(2, 2, 2),      /* TEXT     */
    C(13, 13, 13),   /* TEXT2    */
    C(21, 21, 21),   /* TEXT3    */
    C(11, 11, 11),   /* NOTE     */
    C(0, 0, 0),      /* HOT      */
    C(28, 28, 28),   /* BEAT     */
    C(30, 10, 4),    /* ACCENT   #F45423 */
    C(31, 27, 25),   /* ACC_TINT */
    C(31, 29, 28),   /* ACC_FADE */
    C(8, 8, 8),      /* SCOPE    */
    C(31, 31, 31),
};

void cv_init(void)
{
    int i;
    for (i = 0; i < 16; i++) PAL_BG[i] = roles[i];
    for (i = 0; i < 32 * 32; i++) MAP[i] = 0;
    for (i = 0; i < 20 * 30; i++) MAP[(i / 30) * 32 + i % 30] = (uint16_t)i;
    REG_BG0CNT = BG_CBB(0) | BG_SBB(30) | BG_4BPP | BG_PRIO(1);
    REG_BG0HOFS = 0; REG_BG0VOFS = 0;
    cv_clear(CV_BG);
}

void cv_clear(uint8_t c)
{
    uint32_t f = FILL(c);
    int i;
    for (i = 0; i < 600 * 8; i++) TILES[i] = f;
}

static inline volatile uint32_t *word(int x, int y)
{
    return &TILES[((y >> 3) * 30 + (x >> 3)) * 8 + (y & 7)];
}

void cv_point(int x, int y, uint8_t c)
{
    volatile uint32_t *w;
    uint32_t sh;
    if ((unsigned)x >= CV_W || (unsigned)y >= CV_H) return;
    w = word(x, y);
    sh = (uint32_t)(x & 7) * 4;
    *w = (*w & ~(15u << sh)) | ((uint32_t)c << sh);
}

void cv_rect(int x, int y, int w, int h, uint8_t c) { cv_rect_fast(x, y, w, h, c); }

void cv_hline(int x, int y, int w, uint8_t c) { cv_rect(x, y, w, 1, c); }
void cv_vline(int x, int y, int h, uint8_t c) { cv_rect(x, y, 1, h, c); }

void cv_frame(int x, int y, int w, int h, uint8_t c)
{
    cv_rect(x, y, w, 1, c); cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c); cv_rect(x + w - 1, y, 1, h, c);
}

void cv_rrect(int x, int y, int w, int h, uint8_t c)
{
    cv_rect(x + 1, y, w - 2, h, c);
    cv_rect(x, y + 1, 1, h - 2, c);
    cv_rect(x + w - 1, y + 1, 1, h - 2, c);
}

void cv_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        int e2;
        cv_point(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

/* ---- text ------------------------------------------------------------------- */
static int glyph(int font, char ch, const uint8_t **rows)
{
    int i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    if (ch < 32 || ch > 95) ch = '?';
    i = ch - 32;
    if (font == F_SMALL) { *rows = &font_small_rows[i * 5]; return font_small_w[i]; }
    *rows = &font_main_rows[i * 7];
    return font_main_w[i];
}

int cv_text_w(int font, const char *s)
{
    int w = 0;
    const uint8_t *r;
    for (; *s; s++) w += glyph(font, *s, &r) + 1;
    return w ? w - 1 : 0;
}

/* mirror the text into ui_textbuf (the tests read it) */
static void mirror(int x, int y, const char *s)
{
    int cx = x >> 3, cy = y >> 3, n = (int)strlen(s);
    if (cy < 0 || cy >= SCR_H) return;
    if (cx + n > SCR_W) cx = SCR_W - n;          /* proportional text is narrower than the cells */
    if (cx < 0) { s -= cx; cx = 0; }
    while (*s && cx < SCR_W) { if (cx >= 0) ui_textbuf[cy][cx] = *s; cx++; s++; }
}

int cv_text(int x, int y, int font, uint8_t c, const char *s)
{
    mirror(x, y, s);
    return cv_text_fast(x, y, font, c, s);
}

void cv_text_c(int cx, int y, int font, uint8_t c, const char *s) { cv_text(cx - cv_text_w(font, s) / 2, y, font, c, s); }
void cv_text_r(int rx, int y, int font, uint8_t c, const char *s) { cv_text(rx - cv_text_w(font, s), y, font, c, s); }

void cv_numfmt(char *b, uint16_t v, uint8_t digits, uint8_t zero)
{
    char t[6];
    int n = 0, i;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 5);
    i = 0;
    while (digits > n) { b[i++] = zero ? '0' : ' '; digits--; }
    while (n) b[i++] = t[--n];
    b[i] = 0;
}

int cv_num(int x, int y, int font, uint8_t c, uint16_t v, uint8_t digits, uint8_t zero)
{
    char b[8];
    cv_numfmt(b, v, digits, zero);
    return cv_text(x, y, font, c, b);
}

void cv_field(int x, int y, int w, int font, uint8_t c, uint8_t bg, const char *s)
{
    cv_rect(x, y, w, FONT_H(font), bg);
    cv_text(x, y, font, c, s);
}

void cv_field_r(int x, int y, int w, int font, uint8_t c, uint8_t bg, const char *s)
{
    cv_rect(x, y, w, FONT_H(font), bg);
    cv_text_r(x + w, y, font, c, s);
}

void cv_blit_tile(int cx, int cy, const uint32_t *src, const uint8_t map[4])
{
    volatile uint32_t *d;
    int j, i;
    if ((unsigned)cx >= 30 || (unsigned)cy >= 20) return;
    d = &TILES[(cy * 30 + cx) * 8];
    for (j = 0; j < 8; j++) {
        uint32_t s = src[j], o = 0;
        for (i = 0; i < 8; i++) o |= (uint32_t)map[(s >> (i * 4)) & 3] << (i * 4);
        d[j] = o;
    }
}

/* ---- the spec's parts ----------------------------------------------------------- */
void cv_header(void)
{
    char code[16], b[20];
    int x, i, n;
    cv_rect(0, 0, CV_W, 12, CV_BG);
    x = cv_text(6, 2, F_MAIN, CV_TEXT, "ETCH II");
    cv_vline(x + 3, 2, 7, CV_BORDER);
    {
        char nm[E2_NAME_LEN + 1];
        memcpy(nm, proj.name, E2_NAME_LEN); nm[E2_NAME_LEN] = 0;
        for (i = E2_NAME_LEN - 1; i >= 0 && (nm[i] == ' ' || !nm[i]); i--) nm[i] = 0;
        cv_text(x + 7, 2, F_MAIN, CV_TEXT2, nm);
    }
    if (proj.compat == COMPAT_EXPANDED) { ext_code_encode(&proj.gen, proj.xseed, code); n = 16; }
    else { corr_code_encode(&proj.gen, code); n = 12; }
    for (i = 0, x = 0; i < n; i++) { if (i && !(i & 3)) b[x++] = '-'; b[x++] = code[i]; }
    b[x] = 0;
    cv_text_r(234, 2, F_MAIN, CV_TEXT, b);
    cv_hline(6, 11, 228, CV_BORDER);
}

void cv_tabs(uint8_t cur)
{
    int i, cw = 228 / SCR_COUNT;
    cv_rect(0, 12, CV_W, 13, CV_BG);
    for (i = 0; i < SCR_COUNT; i++) {
        int cx = 6 + cw * i + cw / 2, w = cv_text_w(F_SMALL, scr_names[i]);
        cv_text(cx - w / 2, 15, F_SMALL, i == cur ? CV_TEXT : CV_TEXT3, scr_names[i]);
        if (i == cur) cv_rect(cx - w / 2 - 2, 22, w + 4, 2, CV_ACCENT);
    }
    cv_hline(6, 25, 228, CV_BORDER);
}

void cv_hint(const char *s, uint8_t msg)
{
    cv_rect(0, CV_HINT_Y - 2, CV_W, CV_H - CV_HINT_Y + 2, CV_BG);
    cv_text(6, CV_HINT_Y, F_SMALL, msg ? CV_TEXT : CV_TEXT2, s);
}

/* ---- widgets (SERIES_SPEC.md 4.5) --------------------------------------------------- */
void cv_cap(int x, int y, uint8_t sel, const char *cap)
{
    cv_rect(x - 4, y, 2, 7, sel ? CV_ACCENT : CV_BG);
    cv_text(x, y + 1, F_SMALL, sel ? CV_TEXT : CV_TEXT2, cap);
}

void cv_slider(int x, int y, int w, uint16_t v, uint16_t max, uint8_t sel)
{
    int f = max ? (int)((uint32_t)v * (uint32_t)(w - 2) / max) : 0;
    cv_rect(x, y, w, 7, CV_BG);
    cv_hline(x, y + 3, w, CV_SURF3);
    cv_rect(x, y + 2, f, 3, CV_NOTE);
    cv_rect(x + f, y, 2, 7, sel ? CV_ACCENT : CV_TEXT);
}

void cv_button(int x, int y, int w, int h, const char *label, uint8_t sel, uint8_t lit)
{
    cv_rect(x - 1, y - 1, w + 2, h + 2, CV_BG);
    cv_rrect(x, y, w, h, lit ? CV_ACCENT : CV_SURF2);
    if (sel) {   /* the cursor: an accent outline just outside the button */
        cv_hline(x + 1, y - 1, w - 2, CV_ACCENT); cv_hline(x + 1, y + h, w - 2, CV_ACCENT);
        cv_vline(x - 1, y + 1, h - 2, CV_ACCENT); cv_vline(x + w, y + 1, h - 2, CV_ACCENT);
    }
    cv_text_c(x + w / 2, y + (h - 7) / 2, F_MAIN, CV_TEXT, label);
}

void cv_readout(int x, int y, int w, const char *cap, const char *val)
{
    cv_rect(x, y, w, 14, CV_BG);
    cv_text(x, y, F_SMALL, CV_TEXT2, cap);
    cv_text(x, y + 7, F_MAIN, CV_TEXT, val);
}

int cv_text_scaled(int x, int y, int font, int scale, uint8_t c, const char *s)
{
    int h = FONT_H(font);
    for (; *s; s++) {
        const uint8_t *r;
        int w = glyph(font, *s, &r), j, i;
        for (j = 0; j < h; j++) {
            uint8_t b = r[j];
            for (i = 0; b; i++, b = (uint8_t)(b << 1))
                if (b & 0x80) cv_rect(x + i * scale, y + j * scale, scale, scale, c);
        }
        x += (w + 1) * scale;
    }
    return x;
}
