/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * TRACK: the tracker view of the selected track (B+L / B+R or SELECT+L/R
 * change track).  One row per step.
 *
 *  BASS      NOTE  A(ccent) S(lide) L(ock) P(robability 1-8) CD(condition)
 *  drums     one column per ORIGINAL lane (HATS: closed + open), kit, lock
 *  LEAD...   NOTE  A  S  L  P   (+ SHAPE for CHORD), track LENGTH
 *
 *  NOTE: A cycles rest > note > tie; A+<> semitone, A+^v octave; B+A clears
 *  flags: A toggles;  P / CD / SHAPE / KIT / LEN: A+<> change
 */
#include "ui.h"
#include "seq.h"
#include "evolve.h"
#include "instruments.h"
#include "widgets.h"

static uint8_t cur_s, cur_c, last_step = 0xFF, last_pat = 0xFF, last_commits;
static uint8_t last_note = 33, last_xnote = 69;
static const char *const cond_n[COND_COUNT] = { "---", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4", "FIL", "!FL" };
static const char *const lane_n[NUM_LANES] = { "KICK", "SNAR", "CLHT", "OPHT" };
extern const char * const drum_kit_names2[NUM_LANES][4];

/* columns per track kind: x positions */
static const uint8_t bx[6] = { 3, 7, 9, 11, 13, 15 };       /* NOTE A S L P CD */
static const uint8_t xx[6] = { 3, 7, 9, 11, 13, 15 };       /* NOTE A S L P SHAPE */

static uint8_t ncols(void)
{
    if (app_track == TRK_BASS) return 6;
    if (app_track == TRK_HATS) return 2;
    if (TRK_IS_DRUM(app_track)) return 1;
    return app_track == TRK_CHORD ? 6 : 5;
}

static uint8_t lane_of(uint8_t c) { return app_track == TRK_KICK ? 0 : app_track == TRK_PERC ? 1 : (uint8_t)(2 + c); }

static void draw_row(uint8_t s)
{
    uint8_t y = (uint8_t)(2 + s), pat = seq_cur, playing = seq_playing;
    const pattern_t *p = &proj.pat[pat];
    uint8_t ph = playing && (TRK_IS_CORE(app_track) ? seq_step == s : seq_xpos[xt_of_trk[app_track]] == s);
    ui_num(0, y, (uint16_t)(s + 1), 2, ph ? PAL_PLAYHEAD : PAL_DIM);
    ui_fill(2, y, 17, ' ', PAL_TEXT);
    if (app_track == TRK_BASS) {
        uint8_t f = p->flags[s], t = f & STEP_TYPE_MASK, pal;
        pal = (cur_s == s && cur_c == 0) ? PAL_CURSOR : PAL_VALUE;
        if (t == STEP_NOTE) ui_note(bx[0], y, (uint8_t)(p->note[s] + 24), pal);
        else ui_text(bx[0], y, t == STEP_TIE ? "===" : "---", cur_s == s && cur_c == 0 ? PAL_CURSOR : PAL_DIM);
        ui_put(bx[1], y, (f & STEP_ACCENT) ? 'A' : '.', cur_s == s && cur_c == 1 ? PAL_CURSOR : PAL_VALUE);
        ui_put(bx[2], y, (f & STEP_SLIDE) ? 'S' : '.', cur_s == s && cur_c == 2 ? PAL_CURSOR : PAL_VALUE);
        ui_put(bx[3], y, (f & STEP_LOCK) ? 'L' : '.', cur_s == s && cur_c == 3 ? PAL_CURSOR : PAL_ALERT);
        ui_put(bx[4], y, (uint8_t)('1' + (p->prob[s] & 7)), cur_s == s && cur_c == 4 ? PAL_CURSOR : PAL_TEXT);
        ui_text(bx[5], y, cond_n[(p->prob[s] >> 4) % COND_COUNT], cur_s == s && cur_c == 5 ? PAL_CURSOR : PAL_TEXT);
    } else if (TRK_IS_DRUM(app_track)) {
        uint8_t c;
        for (c = 0; c < ncols(); c++) {
            uint8_t l = lane_of(c);
            ui_put((uint8_t)(4 + c * 6), y, (p->drum[l] >> s) & 1 ? T_HIT : T_REST,
                   cur_s == s && cur_c == c ? PAL_CURSOR : PAL_VALUE);
        }
    } else {
        const xpattern_t *xp = &proj.xpat[pat];
        uint8_t xt = xt_of_trk[app_track], f = xp->trk[xt].flags[s], t = f & STEP_TYPE_MASK;
        uint8_t len = xp->len[xt] ? xp->len[xt] : 16, dim = s >= len;
        if (t == STEP_NOTE) ui_note(xx[0], y, xp->trk[xt].note[s], cur_s == s && cur_c == 0 ? PAL_CURSOR : dim ? PAL_DIM : PAL_VALUE);
        else ui_text(xx[0], y, t == STEP_TIE ? "===" : "---", cur_s == s && cur_c == 0 ? PAL_CURSOR : PAL_DIM);
        ui_put(xx[1], y, (f & STEP_ACCENT) ? 'A' : '.', cur_s == s && cur_c == 1 ? PAL_CURSOR : PAL_VALUE);
        ui_put(xx[2], y, (f & STEP_SLIDE) ? 'S' : '.', cur_s == s && cur_c == 2 ? PAL_CURSOR : PAL_VALUE);
        ui_put(xx[3], y, (f & STEP_LOCK) ? 'L' : '.', cur_s == s && cur_c == 3 ? PAL_CURSOR : PAL_ALERT);
        ui_put(xx[4], y, (uint8_t)('8' - (f >> XF_PROB_SHIFT)), cur_s == s && cur_c == 4 ? PAL_CURSOR : PAL_TEXT);
        if (app_track == TRK_CHORD) ui_text(xx[5], y, chord_names[xp->shape[s] & 7], cur_s == s && cur_c == 5 ? PAL_CURSOR : PAL_TEXT);
    }
}

static void draw_side(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t x = 21;
    ui_textn(x, 2, trk_names[app_track], 9, PAL_VALUE);
    ui_textn(x, 3, eng_short(app_track), 9, PAL_DIM);
    ui_text(x, 5, "PAT", PAL_LABEL); ui_num0((uint8_t)(x + 4), 5, (uint16_t)(seq_cur + 1), 2, PAL_VALUE);
    ui_text(x, 6, "GEN", PAL_LABEL); ui_num0((uint8_t)(x + 4), 6, p->gen, 3, PAL_VALUE);
    if (TRK_IS_DRUM(app_track)) {
        uint8_t c;
        for (c = 0; c < ncols(); c++) {
            uint8_t l = lane_of(c);
            ui_textn(x, (uint8_t)(8 + c * 3), lane_n[l], 5, PAL_LABEL);
            ui_textn(x, (uint8_t)(9 + c * 3), drum_kit_names2[l][p->kit[l] & 3], 6, PAL_VALUE);
            ui_text(x, (uint8_t)(10 + c * 3), (p->lock & (PLOCK_LANE0 << l)) ? "LOCKED " : "       ", PAL_ALERT);
        }
        ui_text(x, 15, "KIT:A+<>", PAL_DIM);
        ui_text(x, 16, "LOCK:B+v", PAL_DIM);
    } else if (app_track == TRK_BASS) {
        ui_text(x, 8, (p->lock & PLOCK_BASS) ? "BASS LK" : "       ", PAL_ALERT);
        ui_text(x, 9, (p->lock & PLOCK_ALL) ? "PATT LK" : "       ", PAL_ALERT);
        ui_text(x, 11, "ORIGINAL", PAL_DIM);
        ui_text(x, 12, "CORROSION", PAL_DIM);
    } else {
        uint8_t xt = xt_of_trk[app_track];
        ui_text(x, 8, "LEN", PAL_LABEL);
        ui_num((uint8_t)(x + 4), 8, proj.xpat[seq_cur].len[xt] ? proj.xpat[seq_cur].len[xt] : 16, 2, PAL_VALUE);
        ui_text(x, 9, "B+<> LEN", PAL_DIM);
        ui_text(x, 11, "ETCH II", PAL_DIM);
        ui_text(x, 12, "EXTENSION", PAL_DIM);
        if (proj.compat != COMPAT_EXPANDED) ui_text(x, 14, "CLASSIC:", PAL_ALERT), ui_text(x, 15, "SILENT", PAL_ALERT);
    }
}

void scr_track_enter(void)
{
    uint8_t s;
    ui_header("TRACK");
    if (cur_c >= ncols()) cur_c = 0;
    if (app_track == TRK_BASS) ui_text(0, 1, " # NOTE A S L P CD", PAL_DIM);
    else if (TRK_IS_DRUM(app_track)) ui_text(0, 1, app_track == TRK_HATS ? " #  CLOSED OPEN" : " #  HIT", PAL_DIM);
    else ui_text(0, 1, app_track == TRK_CHORD ? " # NOTE A S L P SHAPE" : " # NOTE A S L P", PAL_DIM);
    for (s = 0; s < 16; s++) draw_row(s);
    draw_side();
    ui_help("A:EDIT B+A:CLR B+L/R:TRACK");
    last_step = 0xFF;
}

static void edited(void)
{
    if (TRK_IS_CORE(app_track)) { pattern_sanitize(&proj.pat[seq_cur]); app_pattern_edited(seq_cur); }
    else proj.xpat[seq_cur].present = 1;
}

static void edit(int8_t d, uint8_t tap, uint8_t clear)
{
    pattern_t *p = &proj.pat[seq_cur];
    uint8_t s = cur_s;
    if (app_track == TRK_BASS) {
        uint8_t *f = &p->flags[s];
        if (cur_c != 3 && (*f & STEP_LOCK)) { if (tap || d || clear) ui_msg("STEP LOCKED (L COLUMN)"); return; }
        if (p->lock & (PLOCK_BASS | PLOCK_ALL)) { if (tap || d || clear) ui_msg("BASS LOCKED (GEN)"); return; }
        if (clear) { *f = (uint8_t)(*f & STEP_LOCK); p->prob[s] = PROB_ALWAYS; edited(); return; }
        switch (cur_c) {
        case 0:
            if (tap) {
                uint8_t t = *f & STEP_TYPE_MASK;
                if (t == STEP_REST) { *f = (uint8_t)((*f & ~STEP_TYPE_MASK) | STEP_NOTE); p->note[s] = last_note; }
                else if (t == STEP_NOTE) *f = (uint8_t)((*f & ~STEP_TYPE_MASK) | STEP_TIE);
                else *f = (uint8_t)(*f & STEP_LOCK);
            } else if (d && (*f & STEP_TYPE_MASK) == STEP_NOTE) {
                int16_t n = (int16_t)p->note[s] + d;
                p->note[s] = (uint8_t)(n < 0 ? 0 : n > NOTE_MAX ? NOTE_MAX : n);
                last_note = p->note[s];
            } else return;
            break;
        case 1: if (!tap) return; *f ^= STEP_ACCENT; break;
        case 2: if (!tap) return; *f ^= STEP_SLIDE; break;
        case 3: if (!tap) return; *f ^= STEP_LOCK; break;
        case 4: if (!d) return; p->prob[s] = (uint8_t)((p->prob[s] & 0xF0) | clampadd(p->prob[s] & 7, d > 0 ? 1 : -1, 0, 7)); break;
        default: if (!d) return; p->prob[s] = (uint8_t)((p->prob[s] & 0x0F) | (clampadd(p->prob[s] >> 4, d > 0 ? 1 : -1, 0, COND_COUNT - 1) << 4)); break;
        }
        edited();
    } else if (TRK_IS_DRUM(app_track)) {
        uint8_t l = lane_of(cur_c);
        if (p->lock & ((PLOCK_LANE0 << l) | PLOCK_ALL)) { if (tap || d || clear) ui_msg("LANE LOCKED"); return; }
        if (tap) p->drum[l] ^= (uint16_t)(1u << s);
        else if (clear) p->drum[l] &= (uint16_t)~(1u << s);
        else if (d) p->kit[l] = (uint8_t)((p->kit[l] + (d > 0 ? 1 : 3)) & 3);
        else return;
        edited();
    } else {
        xpattern_t *xp = &proj.xpat[seq_cur];
        xtrack_t *t = &xp->trk[xt_of_trk[app_track]];
        uint8_t *f = &t->flags[s];
        if (cur_c != 3 && (*f & STEP_LOCK)) { if (tap || d || clear) ui_msg("STEP LOCKED"); return; }
        if (clear) { *f = (uint8_t)(*f & STEP_LOCK); edited(); return; }
        switch (cur_c) {
        case 0:
            if (tap) {
                uint8_t ty = *f & STEP_TYPE_MASK;
                if (ty == STEP_REST) { *f = (uint8_t)((*f & ~STEP_TYPE_MASK) | STEP_NOTE); t->note[s] = last_xnote; }
                else if (ty == STEP_NOTE) *f = (uint8_t)((*f & ~STEP_TYPE_MASK) | STEP_TIE);
                else *f = (uint8_t)(*f & STEP_LOCK);
            } else if (d && (*f & STEP_TYPE_MASK) == STEP_NOTE) {
                int16_t n = (int16_t)t->note[s] + d;
                t->note[s] = (uint8_t)(n < 12 ? 12 : n > 108 ? 108 : n);
                last_xnote = t->note[s];
            } else return;
            break;
        case 1: if (!tap) return; *f ^= STEP_ACCENT; break;
        case 2: if (!tap) return; *f ^= STEP_SLIDE; break;
        case 3: if (!tap) return; *f ^= STEP_LOCK; break;
        case 4: if (!d) return; *f = (uint8_t)((*f & ~XF_PROB_MASK) | (clampadd((uint8_t)(*f >> XF_PROB_SHIFT), d > 0 ? -1 : 1, 0, 7) << XF_PROB_SHIFT)); break;
        default: if (!d) return; xp->shape[s] = (uint8_t)((xp->shape[s] + (d > 0 ? 1 : 7)) & 7); break;
        }
        edited();
    }
}

void scr_track_frame(void)
{
    uint8_t nav = ui_nav(), bd = ui_b_dir(), s;
    int8_t d = ui_edit(12);
    uint8_t os = cur_s, oc = cur_c;
    if (nav & 4) cur_s = (uint8_t)((cur_s + 15) & 15);
    if (nav & 8) cur_s = (uint8_t)((cur_s + 1) & 15);
    if ((nav & 1) && cur_c + 1 < ncols()) cur_c++;
    if ((nav & 2) && cur_c) cur_c--;
    if (os != cur_s || oc != cur_c) { draw_row(os); draw_row(cur_s); }
    if (ui_a_tap()) { edit(0, 1, 0); draw_row(cur_s); draw_side(); }
    if (d) { edit(d, 0, 0); draw_row(cur_s); draw_side(); }
    if (ui_clear()) { edit(0, 0, 1); draw_row(cur_s); }
    if (bd && !TRK_IS_CORE(app_track) && (bd & 3)) {          /* B+<>: track length */
        uint8_t xt = xt_of_trk[app_track], *len = &proj.xpat[seq_cur].len[xt];
        if (*len == 0) *len = 16;
        *len = clampadd(*len, (bd & 1) ? 1 : -1, 1, 16);
        proj.xpat[seq_cur].present = 1;
        scr_track_enter();
        return;
    }
    if ((bd & 8) && TRK_IS_DRUM(app_track)) {                  /* B+v: lane lock */
        uint8_t l = lane_of(cur_c);
        proj.pat[seq_cur].lock ^= (uint8_t)(PLOCK_LANE0 << l);
        app_pattern_edited(seq_cur);
        draw_side();
    }
    if (seq_step_event != last_step || seq_cur != last_pat || evo_commits != last_commits) {
        uint8_t full = seq_cur != last_pat || evo_commits != last_commits;
        last_step = seq_step_event; last_pat = seq_cur; last_commits = evo_commits;
        if (full) { for (s = 0; s < 16; s++) draw_row(s); draw_side(); }
        else {
            static uint8_t prev = 0;
            uint8_t now = TRK_IS_CORE(app_track) ? seq_step : seq_xpos[xt_of_trk[app_track]];
            if (prev < 16) draw_row(prev);
            if (now < 16) draw_row(now);
            prev = now;
        }
    }
}
