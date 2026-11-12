/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * SYNTH: the selected track's synthesizer (B+L / B+R: other track).
 * MOD:   LFO, mix (level, pan, delay send, bit reduction) and master effects.
 * Each row: ^v select, A+<> -1/+1, A+^v coarse, B+A default.
 */
#include "ui.h"
#include "seq.h"
#include "audio.h"
#include "instruments.h"
#include "widgets.h"

typedef struct { const char *label; uint8_t *ptr; uint8_t max; uint8_t kind; uint8_t def; } row_t;
enum { K_NUM, K_PARAM, K_ENGINE, K_VOICING, K_DEST, K_SHAPE, K_PAN, K_DTIME };
#define MAX_ROWS 17
static row_t rows[MAX_ROWS];
static uint8_t nrows, cur;
static uint8_t scr_kind;          /* 0 SYNTH, 1 MOD */

static const char *const dtime_n[8] = { "1/16 ", "1/8  ", "3/16 ", "1/4  ", "3/8  ", "1/2  ", "3/32 ", "1/24 " };
static const char *const lane_short[NUM_LANES] = { "KK", "SN", "CH", "OH" };

static void add(const char *l, uint8_t *p, uint8_t max, uint8_t kind, uint8_t def)
{
    if (nrows >= MAX_ROWS) return;
    rows[nrows].label = l; rows[nrows].ptr = p; rows[nrows].max = max; rows[nrows].kind = kind; rows[nrows].def = def;
    nrows++;
}

static void build(void)
{
    patch_t *pt = &proj.patch[app_track];
    nrows = 0;
    if (scr_kind == 0) {
        uint8_t def = param_def_of(pt, app_track), i;
        if (TRK_IS_DRUM(app_track)) add("VOICING", &pt->engine, 2, K_VOICING, 0);
        else add("ENGINE ", &pt->engine, ENG_COUNT - 1, K_ENGINE, 0);
        for (i = 0; i < P_COUNT; i++)
            if (param_defs[def][i].max) add(param_defs[def][i].label, &pt->p[i], param_defs[def][i].max, K_PARAM, i);
        if (TRK_IS_DRUM(app_track)) {
            static char lbl[NUM_LANES][3][8];
            uint8_t l, l0 = app_track == TRK_KICK ? 0 : app_track == TRK_PERC ? 1 : 2, l1 = app_track == TRK_HATS ? 3 : l0;
            for (l = l0; l <= l1; l++) {
                memcpy(lbl[l][0], lane_short[l], 2); memcpy(lbl[l][0] + 2, " TUNE", 6);
                memcpy(lbl[l][1], lane_short[l], 2); memcpy(lbl[l][1] + 2, " DECY", 6);
                memcpy(lbl[l][2], lane_short[l], 2); memcpy(lbl[l][2] + 2, " LEVL", 6);
                add(lbl[l][0], &proj.drum_tune[l], 15, K_NUM, 8);
                add(lbl[l][1], &proj.drum_decay[l], 15, K_NUM, 8);
                add(lbl[l][2], &proj.drum_level[l], 15, K_NUM, 15);
            }
        }
    } else {
        add("LFO RATE", &pt->lfo_rate, 15, K_NUM, 5);
        add("LFO DPTH", &pt->lfo_depth, 15, K_NUM, 0);
        add("LFO DEST", &pt->lfo_dest, LFO_TO_COUNT - 1, K_DEST, 1);
        add("LFO SHAP", &pt->lfo_shape, LFOS_COUNT - 1, K_SHAPE, 0);
        add("LEVEL   ", &pt->level, 15, K_NUM, 12);
        add("PAN     ", &pt->pan, 15, K_PAN, 8);
        add("DLY SEND", &pt->send, 15, K_NUM, 0);
        add("BITCRUSH", &pt->crush, 7, K_NUM, 0);
        add("DLY TIME", &proj.fx.delay_time, 7, K_DTIME, 2);
        add("DLY FDBK", &proj.fx.delay_fb, 15, K_NUM, 6);
        add("DLY TONE", &proj.fx.delay_tone, 15, K_NUM, 7);
        add("PINGPONG", &proj.fx.width, 15, K_NUM, 10);
        add("MSTR DRV", &proj.fx.master_drive, 7, K_NUM, 0);
    }
    if (cur >= nrows) cur = (uint8_t)(nrows - 1);
}

static void draw_row(uint8_t r)
{
    const row_t *w = &rows[r];
    uint8_t y = (uint8_t)(3 + r), v = *w->ptr;
    char t4[5] = { 0 };
    ui_textn(0, y, w->label, 9, cur == r ? PAL_CURSOR : PAL_LABEL);
    switch (w->kind) {
    case K_ENGINE:  ui_textn(10, y, eng_names[v % ENG_COUNT], 9, PAL_VALUE); break;
    case K_VOICING: ui_textn(10, y, voicing_names[v % 3], 9, PAL_VALUE); break;
    case K_DEST:    ui_textn(10, y, lfo_dest_names[v % LFO_TO_COUNT], 9, PAL_VALUE); break;
    case K_SHAPE:   ui_textn(10, y, lfo_shape_names[v % LFOS_COUNT], 9, PAL_VALUE); break;
    case K_DTIME:   ui_textn(10, y, dtime_n[v & 7], 9, PAL_VALUE); break;
    case K_PAN:
        ui_textn(10, y, v == 8 ? "CENTRE" : v < 8 ? "LEFT  " : "RIGHT ", 9, PAL_VALUE);
        if (v != 8) ui_num(16, y, (uint16_t)(v < 8 ? 8 - v : v - 8), 1, PAL_VALUE);
        break;
    case K_PARAM:
        param_text(param_def_of(&proj.patch[app_track], app_track), w->def, v, t4);
        ui_textn(10, y, t4, 9, PAL_VALUE);
        break;
    default: ui_num(10, y, v, 2, PAL_VALUE); ui_fill(12, y, 7, ' ', PAL_VALUE); break;
    }
    if (w->max >= 3) ui_bar(20, y, 8, v, w->max, PAL_DIM);
    else ui_fill(20, y, 8, ' ', PAL_DIM);
}

static void enter(void)
{
    uint8_t r;
    build();
    ui_header(scr_kind ? "MOD" : "SYNTH");
    ui_text(0, 1, "TRACK", PAL_DIM);
    ui_textn(6, 1, trk_names[app_track], 6, PAL_VALUE);
    ui_text(13, 1, TRK_IS_CORE(app_track) ? "CORE" : "EXT ", PAL_DIM);
    ui_text(18, 1, "B+L/R:TRACK", PAL_DIM);
    for (r = 0; r < nrows; r++) draw_row(r);
    ui_help(scr_kind ? "A+</>:SET  MASTER FX ARE SHARED" : "A+</>:SET  A+^v:COARSE  B+A:DEF");
}

static void frame(void)
{
    uint8_t nav = ui_nav(), o = cur;
    int8_t d = ui_edit(4);
    if ((nav & 4) && cur) cur--;
    if ((nav & 8) && cur + 1 < nrows) cur++;
    if (o != cur) { draw_row(o); draw_row(cur); }
    if (d || ui_clear()) {
        row_t *w = &rows[cur];
        if (d) *w->ptr = w->kind == K_ENGINE ? (uint8_t)((*w->ptr + ENG_COUNT + (d > 0 ? 1 : -1)) % ENG_COUNT)
                                             : clampadd(*w->ptr, d, 0, w->max);
        else {
            patch_t def;
            patch_default(&def, app_track);
            *w->ptr = w->kind == K_PARAM ? def.p[w->def] : w->def;
        }
        if (scr_kind && w->ptr == &proj.fx.delay_time) fx_set_tempo(proj.bpm);
        if (w->kind == K_ENGINE || w->kind == K_VOICING) { enter(); return; }
        draw_row(cur);
    }
}

void scr_synth_enter(void) { scr_kind = 0; enter(); }
void scr_synth_frame(void) { scr_kind = 0; frame(); }
void scr_mod_enter(void) { scr_kind = 1; enter(); }
void scr_mod_frame(void) { scr_kind = 1; frame(); }
