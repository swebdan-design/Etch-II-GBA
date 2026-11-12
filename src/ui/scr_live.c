/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * LIVE: the instrument.  All eight tracks with their steps and playhead,
 * evolution state, the selected track's sound macro, and a scope.
 *
 *   track rows   ^v pick a track (it becomes the selected track everywhere)
 *                on the name: A mutes / unmutes
 *                on a step:   A toggles it; A+<> / A+^v move a note
 *   MODE         A+<> STABLE / DRIFT / CORRODE
 *   MUTATE       A+<> mutation intensity 0..15
 *   ENGINE       A+<> change the selected track's synthesizer
 *   TONE         A+<> the selected track's main timbre control
 *   REGEN        <> ALL / BASS / DRUMS, A replaces half the bar (CORROSION)
 *   B (tap)      FILL on / off (as ETCH 2.5)
 */
#include "ui.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"
#include "widgets.h"
#include "instruments.h"

#define R_TRK0  3
#define ROW_MODE 8
#define ROW_MUT  9
#define ROW_ENG  10
#define ROW_TONE 11
#define ROW_RGN  12
#define N_ROWS   13

static uint8_t row, col = 0xFF, rgn_what;     /* col 0xFF = name column */
static uint8_t last_step = 0xFF, last_pat = 0xFF, last_commits, last_scope;
static uint8_t last_note = 33;                /* A3 (ETCH note numbering) */
static uint8_t last_xnote = 69;

static const char *const mode_n[3] = { "STABLE ", "DRIFT  ", "CORRODE" };
static const char *const rgn_n[3] = { "ALL  ", "BASS ", "DRUMS" };

static void draw_track(uint8_t t)
{
    uint8_t y = (uint8_t)(R_TRK0 + t), sel = (row == t);
    uint8_t pal = (proj.mutes & (1u << t)) ? PAL_DIM : (TRK_IS_CORE(t) ? PAL_TEXT : PAL_LABEL);
    ui_textn(0, y, trk_names[t], 5, sel && col == 0xFF ? PAL_CURSOR : (app_track == t ? PAL_VALUE : pal));
    ui_put(5, y, (proj.mutes & (1u << t)) ? T_MUTE : ' ', PAL_ALERT);
    ui_textn(6, y, eng_short(t), 4, PAL_DIM);
    ui_put(11, y, (seq_activity & (1u << t)) && seq_playing ? T_BLOCK : T_REST, (seq_activity & (1u << t)) ? PAL_PLAY : PAL_DIM);
    w_track_row(y, t, seq_cur, sel && col != 0xFF ? (int8_t)col : -1);
}

static uint8_t tone_val(void) { return proj.patch[app_track].p[P_TONE]; }

static void draw_controls(void)
{
    char c4[4];
    uint8_t bars = seq_bars_to_evolution();
    ui_text(0, 12, "MODE  ", row == ROW_MODE ? PAL_CURSOR : PAL_LABEL);
    ui_textn(6, 12, mode_n[proj.gen.mode % 3], 8, PAL_VALUE);
    ui_text(14, 12, "NEXT", PAL_DIM);
    if (bars == 0xFF || !seq_playing) ui_textn(19, 12, "--", 2, PAL_DIM);
    else ui_num(19, 12, bars, 2, PAL_VALUE);
    ui_text(22, 12, "GEN", PAL_DIM);
    ui_num0(26, 12, proj.pat[seq_cur].gen, 3, PAL_VALUE);

    ui_text(0, 13, "MUTATE", row == ROW_MUT ? PAL_CURSOR : PAL_LABEL);
    ui_num(7, 13, proj.gen.mutation, 2, PAL_VALUE);
    ui_bar(10, 13, 4, proj.gen.mutation, 15, PAL_VALUE);
    ui_textn(15, 13, corr_style_names[proj.gen.style % STYLE_COUNT], 10, PAL_DIM);
    ui_textn(26, 13, proj.compat == COMPAT_EXPANDED ? "EXP " : "CLSC", 4, PAL_DIM);

    ui_text(0, 14, "ENGINE", row == ROW_ENG ? PAL_CURSOR : PAL_LABEL);
    ui_textn(7, 14, trk_names[app_track], 5, PAL_TEXT);
    ui_textn(13, 14, TRK_IS_DRUM(app_track) ? voicing_names[proj.patch[app_track].engine % 3] : eng_names[proj.patch[app_track].engine % ENG_COUNT], 8, PAL_VALUE);

    ui_text(0, 15, "TONE  ", row == ROW_TONE ? PAL_CURSOR : PAL_LABEL);
    ui_num(7, 15, tone_val(), 2, PAL_VALUE);
    ui_bar(10, 15, 4, tone_val(), 31, PAL_VALUE);
    ui_textn(15, 15, param_defs[param_def_of(&proj.patch[app_track], app_track)][P_TONE].label, 6, PAL_DIM);

    ui_text(0, 16, "REGEN ", row == ROW_RGN ? PAL_CURSOR : PAL_LABEL);
    ui_textn(7, 16, rgn_n[rgn_what], 5, PAL_VALUE);
    (void)c4;
}

static void draw_status(void)
{
    ui_textn(0, 1, proj.name, 10, PAL_VALUE);
    ui_text(11, 1, "KEY", PAL_DIM);
    {
        static const char kn[12][3] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
        ui_textn(15, 1, kn[proj.gen.key % 12], 2, PAL_TEXT);
    }
    ui_textn(18, 1, corr_scale_names[proj.gen.scale % SCALE_COUNT], 8, PAL_TEXT);
    ui_text(0, 2, "TRACK ENGN", PAL_DIM);
    ui_text(13, 2, "1   5   9   13  ", PAL_DIM);
}

void scr_live_enter(void)
{
    uint8_t t;
    ui_header("LIVE");
    draw_status();
    for (t = 0; t < TRK_COUNT; t++) draw_track(t);
    draw_controls();
    w_scope_map(14, 16);
    ui_text(0, 17, "LVL", PAL_DIM);
    ui_text(0, 18, "CPU", PAL_DIM);
    ui_help("A:EDIT  B:FILL  L/R:SCREEN");
    last_step = 0xFF;
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
}

void scr_live_frame(void)
{
    uint8_t nav = ui_nav(), t;
    int8_t d = ui_edit(row < TRK_COUNT ? 12 : 4);

    if (ui_b_tap()) { app_fill_toggle(); ui_msg(seq_fill_hold ? "FILL ON  (B: OFF)" : "FILL OFF"); }
    if (nav & 4) { if (row) row--; if (row < TRK_COUNT) app_track = row; scr_live_enter(); return; }
    if (nav & 8) { if (row < N_ROWS - 1) row++; if (row < TRK_COUNT) app_track = row; scr_live_enter(); return; }
    if (row < TRK_COUNT) {
        if (nav & 1) { col = col == 0xFF ? 0 : (uint8_t)(col < 15 ? col + 1 : 15); draw_track(row); }
        if (nav & 2) { col = (col == 0xFF || col == 0) ? 0xFF : (uint8_t)(col - 1); draw_track(row); }
        if (col == 0xFF) {
            if (ui_a_tap()) { proj.mutes ^= (uint8_t)(1u << row); draw_track(row); }
        } else {
            if (ui_a_tap()) edit_step(0, 1);
            if (d) edit_step(d, 0);
            if (ui_clear()) {
                if (row == TRK_BASS && !(proj.pat[seq_cur].flags[col] & STEP_LOCK)) { proj.pat[seq_cur].flags[col] = STEP_REST; pattern_sanitize(&proj.pat[seq_cur]); app_pattern_edited(seq_cur); }
                else if (!TRK_IS_CORE(row)) proj.xpat[seq_cur].trk[xt_of_trk[row]].flags[col] = STEP_REST;
            }
        }
    } else if (row == ROW_MODE) {
        if (d) { proj.gen.mode = clampadd(proj.gen.mode, d > 0 ? 1 : -1, 0, 2); draw_controls(); }
    } else if (row == ROW_MUT) {
        if (d) { proj.gen.mutation = clampadd(proj.gen.mutation, d > 0 ? (int8_t)(d > 1 ? 4 : 1) : (int8_t)(d < -1 ? -4 : -1), 0, 15); draw_controls(); }
    } else if (row == ROW_ENG) {
        if (d) {
            patch_t *pt = &proj.patch[app_track];
            if (TRK_IS_DRUM(app_track)) pt->engine = clampadd(pt->engine, d > 0 ? 1 : -1, 0, 2);
            else pt->engine = (uint8_t)((pt->engine + ENG_COUNT + (d > 0 ? 1 : -1)) % ENG_COUNT);
            scr_live_enter();
        }
    } else if (row == ROW_TONE) {
        if (d) { proj.patch[app_track].p[P_TONE] = clampadd(tone_val(), d, 0, 31); draw_controls(); }
    } else if (row == ROW_RGN) {
        if (nav & 1) { rgn_what = (uint8_t)((rgn_what + 1) % 3); draw_controls(); }
        if (nav & 2) { rgn_what = (uint8_t)((rgn_what + 2) % 3); draw_controls(); }
        if (ui_a_tap()) {
            static const uint8_t w[3] = { GEN_ALL, GEN_BASS, GEN_DRUMS };
            ui_msg(app_regen(w[rgn_what]) ? "REGEN: HALF THE BAR IS NEW" : "PATTERN LOCKED");
            scr_live_enter();
            return;
        }
    }

    /* live refresh */
    if (seq_step_event != last_step || seq_cur != last_pat || evo_commits != last_commits) {
        uint8_t full = seq_cur != last_pat || evo_commits != last_commits;
        last_step = seq_step_event;
        last_pat = seq_cur;
        last_commits = evo_commits;
        for (t = 0; t < TRK_COUNT; t++) draw_track(t);
        if (full) draw_controls();
        else {
            uint8_t bars = seq_bars_to_evolution();
            if (bars != 0xFF && seq_playing) ui_num(19, 12, bars, 2, PAL_VALUE);
        }
    }
    if ((uint8_t)mix_frames != last_scope) {
        last_scope = (uint8_t)mix_frames;
        if ((last_scope & 1) == 0) w_scope_draw();
        ui_bar(4, 17, 9, mix_peak, 127, mix_peak > 120 ? PAL_ALERT : PAL_PLAY);
        ui_bar(4, 18, 9, (uint8_t)(mix_lines_last > 228 ? 228 : mix_lines_last), 228, mix_lines_last > 200 ? PAL_ALERT : PAL_DIM);
    }
}
