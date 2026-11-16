/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * SYNTH: the selected track's synthesizer (B+L / B+R: other track).
 * MOD:   LFO, mix (level, pan, delay send, bit reduction) and master effects.
 * Each row: ^v select, A+<> -1/+1, A+^v coarse, B+A default.
 */
#include <string.h>
#include "ui.h"
#include "canvas.h"
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

/* layout: two columns of rows (SERIES_SPEC.md 4.5, controls) */
#define LIST_Y  42
#define ROW_P   9
#define COL_W   114

static uint8_t per_col(void) { return (uint8_t)((nrows + 1) / 2); }

static void draw_row(uint8_t r)
{
    const row_t *w = &rows[r];
    uint8_t v = *w->ptr, pc = per_col(), sel = cur == r;
    int x = 6 + (r / pc) * (COL_W + 4), y = LIST_Y + (r % pc) * ROW_P;
    char t[12];
    cv_rect(x - 4, y - 1, COL_W + 4, ROW_P, CV_BG);
    if (sel) cv_rect(x - 4, y, 2, 7, CV_ACCENT);
    cv_text(x, y + 1, F_SMALL, sel ? CV_TEXT : CV_TEXT2, w->label);
    switch (w->kind) {
    case K_ENGINE:  strcpy(t, eng_names[v % ENG_COUNT]); break;
    case K_VOICING: strcpy(t, voicing_names[v % 3]); break;
    case K_DEST:    strcpy(t, lfo_dest_names[v % LFO_TO_COUNT]); break;
    case K_SHAPE:   strcpy(t, lfo_shape_names[v % LFOS_COUNT]); break;
    case K_DTIME:   strcpy(t, dtime_n[v & 7]); break;
    case K_PAN:
        if (v == 8) strcpy(t, "C");
        else { t[0] = v < 8 ? 'L' : 'R'; cv_numfmt(t + 1, (uint16_t)(v < 8 ? 8 - v : v - 8), 0, 0); }
        break;
    case K_PARAM: param_text(param_def_of(&proj.patch[app_track], app_track), w->def, v, t); t[4] = 0; break;
    default: cv_numfmt(t, v, 0, 0); break;
    }
    {   /* trim the padding the 1.0 tables carry */
        int n = (int)strlen(t);
        while (n && t[n - 1] == ' ') t[--n] = 0;
    }
    cv_text(x + 38, y, F_MAIN, CV_TEXT, t);
    if (w->max >= 3 && w->kind == K_NUM) {
        int sx = x + 70, W = 38, f = v * W / w->max;
        cv_hline(sx, y + 3, W, CV_SURF3);
        cv_rect(sx, y + 2, f, 3, CV_NOTE);
        cv_rect(sx + f, y, 2, 7, sel ? CV_ACCENT : CV_TEXT);
    } else if (w->max >= 3 && w->kind == K_PARAM) {
        int sx = x + 70, W = 38, f = v * W / w->max;
        cv_hline(sx, y + 3, W, CV_SURF3);
        cv_rect(sx, y + 2, f, 3, CV_NOTE);
        cv_rect(sx + f, y, 2, 7, sel ? CV_ACCENT : CV_TEXT);
    }
}

static void enter(void)
{
    uint8_t r;
    build();
    ui_header(scr_kind ? "MOD" : "SYNTH");
    /* the track readout */
    cv_text(6, 28, F_SMALL, CV_TEXT2, "TRACK");
    {
        int x = cv_text(6, 34, F_MAIN, CV_TEXT, trk_names[app_track]);
        cv_text(x + 6, 35, F_SMALL, CV_TEXT3, TRK_IS_CORE(app_track) ? "ETCH TRACK" : "ETCH II TRACK");
    }
    cv_text_r(234, 35, F_SMALL, CV_TEXT2, "B+L/R TRACK");
    cv_vline(6 + COL_W + 1, LIST_Y, per_col() * ROW_P - 2, CV_BORDER);
    for (r = 0; r < nrows; r++) draw_row(r);
    ui_help(scr_kind ? "A+</> SET  </> COLUMN  MASTER FX ARE SHARED" : "A+</> SET  A+^v COARSE  B+A DEFAULT  </> COLUMN");
}

static void frame(void)
{
    uint8_t nav = ui_nav(), o = cur, pc = per_col();
    int8_t d = ui_edit(4);
    if ((nav & 4) && cur) cur--;
    if ((nav & 8) && cur + 1 < nrows) cur++;
    if ((nav & 1) && cur < pc && cur + pc < nrows) cur = (uint8_t)(cur + pc);
    if ((nav & 2) && cur >= pc) cur = (uint8_t)(cur - pc);
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
