/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Shared widgets: step cells, track summaries, oscilloscope.
 */
#include "ui.h"
#include "seq.h"
#include "audio.h"
#include "widgets.h"
#include "instruments.h"

/* one step cell for any logical track */
void w_step_cell(uint8_t x, uint8_t y, uint8_t trk, uint8_t pat, uint8_t s, uint8_t cursor)
{
    const pattern_t *p = &proj.pat[pat];
    const xpattern_t *xp = &proj.xpat[pat];
    uint16_t tile = T_REST;
    uint8_t pal = PAL_DIM, playing = seq_playing && pat == seq_cur;
    if (trk == TRK_BASS) {
        uint8_t f = p->flags[s], t = f & STEP_TYPE_MASK;
        if (t == STEP_NOTE) { tile = (f & STEP_ACCENT) ? T_ACC : (f & STEP_SLIDE) ? T_SLIDE : T_NOTE; pal = (p->prob[s] & 7) != 7 || (p->prob[s] >> 4) ? PAL_LABEL : PAL_VALUE; }
        else if (t == STEP_TIE) { tile = T_TIE; pal = PAL_VALUE; }
        if (f & STEP_LOCK) pal = PAL_TEXT;
    } else if (TRK_IS_DRUM(trk)) {
        uint8_t l = trk == TRK_KICK ? 0 : trk == TRK_PERC ? 1 : 2;
        const uint16_t *m = (seq_fill && playing) ? seq_fill_mask : p->drum;
        if (m[l] & (1u << s)) { tile = T_HIT; pal = PAL_VALUE; }
        if (trk == TRK_HATS && (m[3] & (1u << s))) { tile = (m[2] & (1u << s)) ? T_ACC : T_GHOST; pal = PAL_VALUE; }
        if (p->lock & ((PLOCK_LANE0 << l) | PLOCK_ALL)) pal = pal == PAL_DIM ? PAL_DIM : PAL_TEXT;
    } else {
        uint8_t xt = xt_of_trk[trk], f = xp->trk[xt].flags[s], t = f & STEP_TYPE_MASK;
        uint8_t len = xp->len[xt] ? xp->len[xt] : 16;
        if (t == STEP_NOTE) { tile = (f & STEP_ACCENT) ? T_ACC : (f & STEP_SLIDE) ? T_SLIDE : T_NOTE; pal = (f & XF_PROB_MASK) ? PAL_LABEL : PAL_VALUE; }
        else if (t == STEP_TIE) { tile = T_TIE; pal = PAL_VALUE; }
        if (s >= len) { tile = T_DOT2; pal = PAL_DIM; }
        else if (proj.compat != COMPAT_EXPANDED && pal != PAL_DIM) pal = PAL_TARNISH;
    }
    if (proj.mutes & (1u << trk)) pal = PAL_DIM;
    if (playing) {
        uint8_t pos = TRK_IS_CORE(trk) ? seq_step : seq_xpos[xt_of_trk[trk]];
        if (pos == s) pal = PAL_PLAYHEAD;
    }
    if (cursor) pal = PAL_CURSOR;
    ui_put(x, y, tile, pal);
}

void w_track_row(uint8_t y, uint8_t trk, uint8_t pat, int8_t cur_col)
{
    uint8_t s;
    for (s = 0; s < 16; s++) w_step_cell((uint8_t)(13 + s), y, trk, pat, s, cur_col == (int8_t)s);
}

/* oscilloscope from the mixer's decimated output: tiles 448..495 (16 x 3) */
#define SC_TILE0 448
static uint32_t sc_bm[16 * 3 * 8] EWRAM_BSS;
void w_scope_map(uint8_t x0, uint8_t y0)
{
    uint8_t x, y;
    for (y = 0; y < 3; y++)
        for (x = 0; x < 16; x++)
            ui_put((uint8_t)(x0 + x), (uint8_t)(y0 + y), (uint16_t)(SC_TILE0 + y * 16 + x), PAL_SCOPE);
}

void w_scope_draw(void)
{
    int x, prev = -1;
    memset(sc_bm, 0x22, sizeof sc_bm);                 /* colour 2 = background */
    for (x = 0; x < 128; x++) {
        int v = mix_scope[(x >> 1) & 63], y = 12 - v * 11 / 128, a, b, k;
        if (y < 0) y = 0;
        if (y > 23) y = 23;
        if (prev < 0) prev = y;
        a = prev < y ? prev : y; b = prev < y ? y : prev;
        for (k = a; k <= b; k++) {
            uint32_t t = (uint32_t)((k >> 3) * 16 + (x >> 3));
            uint32_t *w = &sc_bm[t * 8 + (k & 7)];
            *w = (*w & ~(15u << ((x & 7) * 4))) | (1u << ((x & 7) * 4));
        }
        prev = y;
    }
    {
        volatile uint32_t *dst = CHARBLOCK(0) + SC_TILE0 * 8;
        uint32_t i;
        for (i = 0; i < 16 * 3 * 8; i++) dst[i] = sc_bm[i];
    }
}

const char *eng_short(uint8_t trk)
{
    static const char *const en[ENG_COUNT] = { "SUB ", "FM  ", "WAVE", "PD  ", "RES " };
    static const char *const dv[3] = { "ANLG", "FM  ", "DIGI" };
    const patch_t *pt = &proj.patch[trk];
    return TRK_IS_DRUM(trk) ? dv[pt->engine % 3] : en[pt->engine % ENG_COUNT];
}
