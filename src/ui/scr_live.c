/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * LIVE: the instrument, laid out as ETCH DS's LIVE (SERIES_SPEC.md 4.5).
 *
 *   header + screen bar
 *   BASS as pitch bars, its A/S strip, then LEAD CHORD DRONE KICK PERC HATS
 *   TEXTR as rounded cells; beat columns, the playhead and fresh CORROSION
 *   changes as washes; the cursor as an accent outline
 *   controls (left): MODE MUTATE ENGINE TONE REGEN
 *   readouts (right): PATTERN GENERATION NEXT EVO TEMPO, scope, CPU
 *   hint line
 *
 *   track rows   ^v pick a track (it becomes the selected track everywhere)
 *                on the name: A mutes / unmutes
 *                on a step:   A toggles it; A+<> semitone, A+^v octave
 *   MODE         A+<> STABLE / DRIFT / CORRODE
 *   MUTATE       A+<> mutation intensity 0..15
 *   ENGINE       A+<> change the selected track's synthesizer
 *   TONE         A+<> the selected track's main timbre control
 *   REGEN        <> ALL / BASS / DRUMS, A replaces half the bar (CORROSION)
 *   B (tap)      FILL on / off
 */
#include <string.h>
#include "ui.h"
#include "canvas.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"
#include "widgets.h"
#include "instruments.h"

#define ROW_MODE 8
#define ROW_MUT  9
#define ROW_ENG  10
#define ROW_TONE 11
#define ROW_RGN  12
#define N_ROWS   13

/* layout (pixels) */
#define GX      40          /* first step column            */
#define SW      12          /* step pitch                   */
#define BASS_Y0 28          /* pitch bar area               */
#define BASS_Y1 45
#define AS_Y    47          /* A/S strip                    */
#define LANE_Y0 52          /* first cell lane              */
#define LANE_P  7           /* lane pitch                   */
#define GRID_Y1 101         /* last grid pixel              */
#define CTL_Y   106         /* controls / readouts          */
#define CTL_P   8
#define RX      122         /* readout column               */
#define SC_X    122         /* scope                        */
#define SC_Y    137
#define SC_W    112
#define SC_H    10

static uint8_t row, col = 0xFF, rgn_what;     /* col 0xFF = name column */
static uint8_t last_step = 0xFF, last_pat = 0xFF, last_commits, last_scope, last_play_col = 0xFF;
static uint8_t last_note = 33;                /* A3 (ETCH note numbering) */
static uint8_t last_xnote = 69;
static uint8_t fresh[16];                     /* CORROSION's fresh changes, fading 3..0 */
static pattern_t before;                      /* the pattern before the last commit */
static uint8_t lo_note, hi_note;              /* the bass's display range */
static uint8_t ro_sig[6];
static uint8_t last_x[XT_COUNT];

static const char *const mode_n[3] = { "STABLE", "DRIFT", "CORRODE" };
static const char *const rgn_n[3] = { "ALL", "BASS", "DRUMS" };
static const uint8_t lane_trk[7] = { TRK_LEAD, TRK_CHORD, TRK_DRONE, TRK_KICK, TRK_PERC, TRK_HATS, TRK_TEXTURE };

static uint8_t tone_val(void) { return proj.patch[app_track].p[P_TONE]; }
static uint8_t muted(uint8_t t) { return (proj.mutes >> t) & 1; }

static int lane_y(uint8_t t)
{
    uint8_t i;
    for (i = 0; i < 7; i++) if (lane_trk[i] == t) return LANE_Y0 + i * LANE_P;
    return BASS_Y0;
}

/* ---- the grid ------------------------------------------------------------------ */
static void bass_range(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t s, lo = 0xFF, hi = 0;
    for (s = 0; s < 16; s++)
        if ((p->flags[s] & STEP_TYPE_MASK) == STEP_NOTE) { if (p->note[s] < lo) lo = p->note[s]; if (p->note[s] > hi) hi = p->note[s]; }
    if (lo == 0xFF) { lo = 24; hi = 36; }
    while (hi - lo < 12) { if (lo) lo--; if (hi - lo < 12) hi++; }
    lo_note = lo; hi_note = hi;
}

static int note_y(uint8_t n)
{
    int span = hi_note - lo_note;
    return BASS_Y1 - 1 - (int)(n - lo_note) * (BASS_Y1 - BASS_Y0 - 2) / span;
}

/* the note that sounds at step s (following ties back) */
static int8_t sounding(const pattern_t *p, int8_t s)
{
    while (s >= 0) {
        uint8_t t = p->flags[s] & STEP_TYPE_MASK;
        if (t == STEP_NOTE) return (int8_t)s;
        if (t != STEP_TIE) return -1;
        s--;
    }
    return -1;
}

static uint8_t col_bg(uint8_t s, uint8_t playing_here)
{
    if (playing_here) return CV_ACC_TINT;
    if (fresh[s] >= 2) return CV_ACC_TINT;
    if (fresh[s] == 1) return CV_ACC_FADE;
    return (s & 3) == 0 ? CV_BEAT : CV_BG;
}

static void draw_bass(uint8_t s, uint8_t bg)
{
    const pattern_t *p = &proj.pat[seq_cur];
    int x = GX + s * SW, src = sounding(p, (int8_t)s);
    uint8_t f = p->flags[s], t = f & STEP_TYPE_MASK, m = muted(TRK_BASS);
    cv_rect(x, BASS_Y0 - 1, SW, AS_Y + 4 - BASS_Y0 + 1, bg);   /* bars, gap and A/S strip */
    if (src >= 0) {
        uint8_t sf = p->flags[src];
        int y = note_y(p->note[src]);
        uint8_t ink = m ? CV_SURF3 : (sf & STEP_ACCENT) ? CV_HOT : ((p->prob[src] & 7) != 7 || (p->prob[src] >> 4)) ? CV_TEXT3 : CV_NOTE;
        cv_vline(x + 5, y + 2, BASS_Y1 - y - 1, CV_SURF3);
        if (t == STEP_TIE) cv_rect(x, y, SW, 2, ink);
        else cv_rect(x + 1, y, 9, 2, ink);
        if ((sf & STEP_SLIDE) && s < 15) {
            int8_t nx = sounding(p, (int8_t)(s + 1));
            if (nx == (int8_t)(s + 1)) {
                int y2 = note_y(p->note[nx]);
                cv_line(x + 9, y, x + 11, y + (y2 - y) / 4, ink);
            }
        }
    }
    /* A/S strip */
    if (t == STEP_NOTE && (f & STEP_ACCENT)) cv_rect(x + 2, AS_Y, 7, 3, m ? CV_SURF3 : CV_HOT);
    else if (t == STEP_NOTE && (f & STEP_SLIDE)) cv_line(x + 2, AS_Y + 2, x + 8, AS_Y, CV_NOTE);
    else cv_hline(x + 2, AS_Y + 1, 7, CV_SURF2);
}

static void draw_cell(uint8_t t, uint8_t s, uint8_t bg)
{
    const pattern_t *p = &proj.pat[seq_cur];
    const xpattern_t *xp = &proj.xpat[seq_cur];
    int x = GX + s * SW, y = lane_y(t);
    uint8_t ink = CV_SURF2, ring = 0, dot = 0, m = muted(t);
    cv_rect(x, y - 1, SW, LANE_P, bg);
    if (TRK_IS_DRUM(t)) {
        uint8_t l = t == TRK_KICK ? 0 : t == TRK_PERC ? 1 : 2;
        const uint16_t *mk = (seq_fill && seq_playing) ? seq_fill_mask : p->drum;
        if (mk[l] & (1u << s)) ink = t == TRK_KICK ? CV_HOT : CV_NOTE;
        if (t == TRK_HATS && (mk[3] & (1u << s))) { if (ink == CV_SURF2) { ink = CV_NOTE; ring = 1; } else ink = CV_HOT; }
    } else {
        uint8_t xt = xt_of_trk[t], f = xp->trk[xt].flags[s], ty = f & STEP_TYPE_MASK;
        uint8_t len = xp->len[xt] ? xp->len[xt] : 16;
        if (s >= len) dot = 1;
        else if (ty == STEP_NOTE) ink = (f & STEP_ACCENT) ? CV_HOT : (f & XF_PROB_MASK) ? CV_TEXT3 : CV_NOTE;
        else if (ty == STEP_TIE) { cv_rect(x, y + 1, SW, 3, proj.compat == COMPAT_EXPANDED && !m ? CV_NOTE : CV_SURF3); return; }
        if (proj.compat != COMPAT_EXPANDED && ink != CV_SURF2) ink = CV_SURF3;
    }
    if (dot) { cv_rect(x + 5, y + 2, 1, 1, CV_SURF3); return; }
    if (m && ink != CV_SURF2) ink = CV_SURF3;
    cv_rrect(x + 1, y, 9, 5, ink);
    if (ring) cv_rect(x + 3, y + 1, 5, 3, bg == CV_BG ? CV_BG : bg);
}

static uint8_t pos_of(uint8_t t)
{
    return TRK_IS_CORE(t) ? seq_step : seq_xpos[xt_of_trk[t]];
}

static void draw_col(uint8_t s)
{
    uint8_t i, pl = seq_playing;
    draw_bass(s, col_bg(s, pl && seq_step == s));
    for (i = 0; i < 7; i++) {
        uint8_t t = lane_trk[i];
        draw_cell(t, s, col_bg(s, pl && pos_of(t) == s));
    }
    if (row < TRK_COUNT && col == s) {
        if (row == TRK_BASS) cv_frame(GX + s * SW, BASS_Y0 - 1, SW, AS_Y + 4 - BASS_Y0, CV_ACCENT);
        else cv_frame(GX + s * SW, lane_y(row) - 1, SW, LANE_P, CV_ACCENT);
    }
}

static void draw_labels(void)
{
    uint8_t i;
    cv_rect(0, BASS_Y0 - 1, GX, GRID_Y1 - BASS_Y0 + 2, CV_BG);
    for (i = 0; i < TRK_COUNT; i++) {
        uint8_t t = i == 0 ? TRK_BASS : lane_trk[i - 1];
        int y = t == TRK_BASS ? 34 : lane_y(t) - 1 + 1;
        uint8_t sel = row == t, c = muted(t) ? CV_TEXT3 : (sel || app_track == t) ? CV_TEXT : CV_TEXT2;
        if (sel && col == 0xFF) cv_rect(2, y - 1, 2, 7, CV_ACCENT);
        cv_text(6, y, F_SMALL, c, trk_names[t]);
        if (muted(t)) cv_hline(5, y + 2, cv_text_w(F_SMALL, trk_names[t]) + 2, CV_TEXT2);
    }
    cv_text(6, AS_Y - 1, F_SMALL, CV_TEXT3, "A/S");
}

static void draw_grid(void)
{
    uint8_t s;
    bass_range();
    for (s = 0; s < 16; s++) draw_col(s);
}

/* ---- controls and readouts ------------------------------------------------------- */
static void ctl_row(uint8_t r, const char *cap, const char *val)
{
    int y = CTL_Y + (r - ROW_MODE) * CTL_P;
    cv_rect(0, y - 1, RX - 4, CTL_P, CV_BG);
    if (row == r) cv_rect(2, y - 1, 2, 7, CV_ACCENT);
    cv_text(6, y + 1, F_SMALL, row == r ? CV_TEXT : CV_TEXT2, cap);
    if (val) cv_text(40, y, F_MAIN, CV_TEXT, val);
}

static void ctl_slider(uint8_t r, uint8_t v, uint8_t max, const char *tail)
{
    int y = CTL_Y + (r - ROW_MODE) * CTL_P, W = 40, f = v * W / max;
    char b[4];
    cv_hline(40, y + 3, W, CV_SURF3);
    cv_rect(40, y + 2, f, 3, CV_NOTE);
    cv_rect(40 + f, y, 2, 7, row == r ? CV_ACCENT : CV_TEXT);
    cv_numfmt(b, v, 0, 0);
    cv_text(86, y, F_MAIN, CV_TEXT, b);
    if (tail) cv_text_r(RX - 6, y + 1, F_SMALL, CV_TEXT2, tail);
}

static void draw_controls(void)
{
    char eng[20];
    const char *en = TRK_IS_DRUM(app_track) ? voicing_names[proj.patch[app_track].engine % 3]
                                            : eng_names[proj.patch[app_track].engine % ENG_COUNT];
    ctl_row(ROW_MODE, "MODE", mode_n[proj.gen.mode % 3]);
    ctl_row(ROW_MUT, "MUTATE", 0);
    ctl_slider(ROW_MUT, proj.gen.mutation, 15, 0);
    strcpy(eng, en);
    ctl_row(ROW_ENG, "ENGINE", eng);
    cv_text_r(RX - 6, CTL_Y + 2 * CTL_P + 1, F_SMALL, CV_TEXT2, trk_names[app_track]);
    ctl_row(ROW_TONE, "TONE", 0);
    ctl_slider(ROW_TONE, tone_val(), 31, param_defs[param_def_of(&proj.patch[app_track], app_track)][P_TONE].label);
    ctl_row(ROW_RGN, "REGEN", rgn_n[rgn_what]);
}

static void readout(int x, int y, const char *cap, const char *val)
{
    cv_rect(x, y, 56, 14, CV_BG);
    cv_text(x, y, F_SMALL, CV_TEXT2, cap);
    cv_text(x, y + 7, F_MAIN, CV_TEXT, val);
}

static void draw_readouts(uint8_t force)
{
    char b[12];
    uint8_t bars = seq_bars_to_evolution();
    uint8_t sig[6] = { seq_cur, seq_queued, (uint8_t)proj.pat[seq_cur].gen, (uint8_t)(seq_playing ? bars : 0xFE),
                       (uint8_t)proj.bpm, seq_fill };
    if (!force && !memcmp(sig, ro_sig, 6)) return;
    memcpy(ro_sig, sig, 6);
    cv_numfmt(b, (uint16_t)(seq_cur + 1), 2, 1);
    if (seq_queued != 0xFF) { b[2] = '>'; cv_numfmt(b + 3, (uint16_t)(seq_queued + 1), 2, 1); }
    readout(RX, CTL_Y, "PATTERN", b);
    if (seq_fill) {                               /* FILL: a lit tag beside the pattern */
        int x = RX + cv_text_w(F_MAIN, b) + 4;
        cv_rect(x, CTL_Y + 6, 19, 9, CV_ACCENT);
        cv_text(x + 2, CTL_Y + 8, F_SMALL, CV_TEXT, "FILL");
    }
    cv_numfmt(b, proj.pat[seq_cur].gen, 3, 1);
    readout(RX + 58, CTL_Y, "GENERATION", b);
    if (bars == 0xFF || !seq_playing) strcpy(b, "--");
    else { cv_numfmt(b, bars, 0, 0); strcat(b, bars == 1 ? " BAR" : " BARS"); }
    readout(RX, CTL_Y + 15, "NEXT EVO", b);
    cv_numfmt(b, proj.bpm, 0, 0);
    readout(RX + 58, CTL_Y + 15, "TEMPO", b);
}

static void draw_scope(void)
{
    int x, prev = -1;
    cv_rect(SC_X, SC_Y, SC_W, SC_H, CV_SURF);
    for (x = 0; x < SC_W - 2; x++) {
        int v = mix_scope[(x * 64 / (SC_W - 2)) & 63], y = SC_Y + SC_H / 2 - v * (SC_H / 2 - 1) / 128;
        if (prev >= 0) cv_line(SC_X + x, prev, SC_X + 1 + x, y, CV_SCOPE);
        prev = y;
    }
    /* CPU: the mixer's share of the frame, as a hairline meter */
    {
        uint16_t l = mix_lines_last > 228 ? 228 : mix_lines_last;
        int w = l * SC_W / 228;
        cv_rect(SC_X, SC_Y + SC_H + 1, SC_W, 2, CV_SURF2);
        cv_rect(SC_X, SC_Y + SC_H + 1, w, 2, l > 200 ? CV_ACCENT : CV_NOTE);
    }
}

static void draw_all(void)
{
    ui_header("LIVE");
    draw_labels();
    draw_grid();
    cv_hline(6, GRID_Y1 + 2, 228, CV_BORDER);
    cv_vline(RX - 4, CTL_Y, 44, CV_BORDER);
    cv_hline(RX, CTL_Y + 30, SC_W, CV_BORDER);
    draw_controls();
    draw_readouts(1);
    ui_help("A EDIT  B FILL  SELECT+</> SCREEN  R HOLD FILL");
}

void scr_live_enter(void)
{
    draw_all();
    last_step = seq_step_event;
    last_pat = seq_cur;
    last_commits = evo_commits;
    last_play_col = seq_step;
    memcpy(&before, &proj.pat[seq_cur], sizeof before);
}

/* compare the committed pattern with the copy taken before: those steps glow */
static void mark_fresh(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t s, l;
    for (s = 0; s < 16; s++) {
        uint8_t ch = p->flags[s] != before.flags[s] || ((p->flags[s] & STEP_TYPE_MASK) == STEP_NOTE && p->note[s] != before.note[s]);
        for (l = 0; l < NUM_LANES; l++) if ((p->drum[l] ^ before.drum[l]) & (1u << s)) ch = 1;
        if (ch) fresh[s] = 3;
    }
    memcpy(&before, p, sizeof before);
}

static void edit_step(int8_t d, uint8_t tap)
{
    uint8_t t = row, s = col, pat = seq_cur;
    pattern_t *p = &proj.pat[pat];
    if (t == TRK_BASS) {
        uint8_t f = p->flags[s];
        if (f & STEP_LOCK) { ui_msg("STEP LOCKED"); return; }
        if (p->lock & (PLOCK_BASS | PLOCK_ALL)) { ui_msg("BASS LOCKED"); return; }
        if (tap) {
            if ((f & STEP_TYPE_MASK) == STEP_NOTE) p->flags[s] = STEP_REST;
            else { p->flags[s] = STEP_NOTE; p->note[s] = last_note; }
        } else if ((f & STEP_TYPE_MASK) == STEP_NOTE) {
            int16_t n = (int16_t)p->note[s] + d;
            if (n < 0) n = 0;
            if (n > NOTE_MAX) n = NOTE_MAX;
            p->note[s] = (uint8_t)n;
            last_note = (uint8_t)n;
        }
        pattern_sanitize(p);
        app_pattern_edited(pat);
    } else if (TRK_IS_DRUM(t)) {
        uint8_t l = t == TRK_KICK ? 0 : t == TRK_PERC ? 1 : 2;
        if (!tap) return;
        if (p->lock & ((PLOCK_LANE0 << l) | PLOCK_ALL)) { ui_msg("LANE LOCKED"); return; }
        p->drum[l] ^= (uint16_t)(1u << s);
        app_pattern_edited(pat);
    } else {
        xtrack_t *x = &proj.xpat[pat].trk[xt_of_trk[t]];
        uint8_t f = x->flags[s];
        if (f & STEP_LOCK) { ui_msg("STEP LOCKED"); return; }
        if (tap) {
            if ((f & STEP_TYPE_MASK) == STEP_NOTE) x->flags[s] = STEP_REST;
            else { x->flags[s] = STEP_NOTE; x->note[s] = last_xnote; }
        } else if ((f & STEP_TYPE_MASK) == STEP_NOTE) {
            int16_t n = (int16_t)x->note[s] + d;
            if (n < 12) n = 12;
            if (n > 108) n = 108;
            x->note[s] = (uint8_t)n;
            last_xnote = (uint8_t)n;
        }
        proj.xpat[pat].present = 1;
        if (proj.compat != COMPAT_EXPANDED) ui_msg("EXPANDED MODE PLAYS THIS (GEN)");
    }
    memcpy(&before, p, sizeof before);            /* your own edits don't glow */
    if (t == TRK_BASS) { uint8_t lo = lo_note, hi = hi_note; bass_range(); if (lo != lo_note || hi != hi_note) { draw_grid(); return; } }
    draw_col(s);
    if (s) draw_col((uint8_t)(s - 1));            /* slides and ties reach back */
    if (s < 15) draw_col((uint8_t)(s + 1));
}

static void move_row(uint8_t nr)
{
    uint8_t or = row, oc = col;
    row = nr;
    if (row < TRK_COUNT) app_track = row;
    draw_labels();
    if (oc != 0xFF && or < TRK_COUNT) draw_col(oc);
    if (col != 0xFF && row < TRK_COUNT) draw_col(col);
    draw_controls();
}

void scr_live_frame(void)
{
    uint8_t nav = ui_nav();
    int8_t d = ui_edit(row < TRK_COUNT ? 12 : 4);

    if (ui_b_tap()) { app_fill_toggle(); ui_msg(seq_fill_hold ? "FILL ON  (B: OFF)" : "FILL OFF"); }
    if (nav & 4) { if (row) move_row((uint8_t)(row - 1)); return; }
    if (nav & 8) { if (row < N_ROWS - 1) move_row((uint8_t)(row + 1)); return; }
    if (row < TRK_COUNT) {
        uint8_t oc = col;
        if (nav & 1) col = col == 0xFF ? 0 : (uint8_t)(col < 15 ? col + 1 : 15);
        if (nav & 2) col = (col == 0xFF || col == 0) ? 0xFF : (uint8_t)(col - 1);
        if (oc != col) {
            if (oc != 0xFF) draw_col(oc);
            if (col != 0xFF) draw_col(col);
            if (oc == 0xFF || col == 0xFF) draw_labels();
        }
        if (col == 0xFF) {
            if (ui_a_tap()) { proj.mutes ^= (uint8_t)(1u << row); draw_labels(); draw_grid(); }
        } else {
            if (ui_a_tap()) edit_step(0, 1);
            if (d) edit_step(d, 0);
            if (ui_clear()) {
                if (row == TRK_BASS && !(proj.pat[seq_cur].flags[col] & STEP_LOCK)) { proj.pat[seq_cur].flags[col] = STEP_REST; pattern_sanitize(&proj.pat[seq_cur]); app_pattern_edited(seq_cur); }
                else if (!TRK_IS_CORE(row)) proj.xpat[seq_cur].trk[xt_of_trk[row]].flags[col] = STEP_REST;
                memcpy(&before, &proj.pat[seq_cur], sizeof before);
                draw_col(col);
                if (col) draw_col((uint8_t)(col - 1));
                if (col < 15) draw_col((uint8_t)(col + 1));
            }
        }
    } else if (row == ROW_MODE) {
        if (d) { proj.gen.mode = clampadd(proj.gen.mode, d > 0 ? 1 : -1, 0, 2); draw_controls(); draw_readouts(1); }
    } else if (row == ROW_MUT) {
        if (d) { proj.gen.mutation = clampadd(proj.gen.mutation, d > 0 ? (int8_t)(d > 1 ? 4 : 1) : (int8_t)(d < -1 ? -4 : -1), 0, 15); draw_controls(); }
    } else if (row == ROW_ENG) {
        if (d) {
            patch_t *pt = &proj.patch[app_track];
            if (TRK_IS_DRUM(app_track)) pt->engine = clampadd(pt->engine, d > 0 ? 1 : -1, 0, 2);
            else pt->engine = (uint8_t)((pt->engine + ENG_COUNT + (d > 0 ? 1 : -1)) % ENG_COUNT);
            draw_controls();
        }
    } else if (row == ROW_TONE) {
        if (d) { proj.patch[app_track].p[P_TONE] = clampadd(tone_val(), d, 0, 31); draw_controls(); }
    } else if (row == ROW_RGN) {
        if (nav & 1) { rgn_what = (uint8_t)((rgn_what + 1) % 3); draw_controls(); }
        if (nav & 2) { rgn_what = (uint8_t)((rgn_what + 2) % 3); draw_controls(); }
        if (ui_a_tap()) {
            static const uint8_t w[3] = { GEN_ALL, GEN_BASS, GEN_DRUMS };
            ui_msg(app_regen(w[rgn_what]) ? "REGEN: HALF THE BAR IS NEW" : "PATTERN LOCKED");
            mark_fresh();
            draw_grid();
            return;
        }
    }

    /* live refresh: only the columns the playhead leaves and enters */
    if (seq_cur != last_pat) {
        last_pat = seq_cur;
        memset(fresh, 0, sizeof fresh);
        memcpy(&before, &proj.pat[seq_cur], sizeof before);
        last_commits = evo_commits;
        draw_grid();
        draw_readouts(1);
    } else if (evo_commits != last_commits) {
        last_commits = evo_commits;
        mark_fresh();
        draw_grid();
    } else if (seq_step_event != last_step) {
        uint8_t s, wrapped = seq_step < last_play_col;
        last_step = seq_step_event;
        if (wrapped) {                            /* a new bar: the glow fades */
            for (s = 0; s < 16; s++) if (fresh[s]) { fresh[s]--; if (s != last_play_col) draw_col(s); }
        }
        if (last_play_col < 16) draw_col(last_play_col);
        last_play_col = seq_step;
        for (s = 0; s < XT_COUNT; s++) {          /* extension tracks keep their own time */
            uint8_t o = last_x[s];
            last_x[s] = seq_xpos[s];
            if (o < 16 && o != seq_step) draw_col(o);
            if (last_x[s] < 16 && last_x[s] != o) draw_col(last_x[s]);
        }
        draw_col(seq_step);
    }
    draw_readouts(0);
    if ((uint8_t)mix_frames != last_scope) {
        last_scope = (uint8_t)mix_frames;
        if ((last_scope & 3) == 0) draw_scope();
    }
}
