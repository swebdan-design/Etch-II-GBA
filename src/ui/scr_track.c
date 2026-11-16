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
#include "canvas.h"
#include "seq.h"
#include "evolve.h"
#include "instruments.h"
#include "widgets.h"

static uint8_t cur_s, cur_c, last_step = 0xFF, last_pat = 0xFF, last_commits;
static uint8_t last_note = 33, last_xnote = 69;
static const char *const cond_n[COND_COUNT] = { "---", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4", "FIL", "!FL" };
static const char *const lane_n[NUM_LANES] = { "KICK", "SNARE", "HAT", "OPEN" };
extern const char * const drum_kit_names2[NUM_LANES][4];

/* layout: one row per step (SMALL font, 7 px), a side panel on the right */
#define TR_Y  37
#define TR_P  7
static const uint8_t cx_[6] = { 22, 46, 58, 70, 82, 96 };   /* NOTE A S L P CD/SHAPE (bass, ext) */
#define SIDE_X 136

static uint8_t ncols(void)
{
    if (app_track == TRK_BASS) return 6;
    if (app_track == TRK_HATS) return 2;
    if (TRK_IS_DRUM(app_track)) return 1;
    return app_track == TRK_CHORD ? 6 : 5;
}

static uint8_t lane_of(uint8_t c) { return app_track == TRK_KICK ? 0 : app_track == TRK_PERC ? 1 : (uint8_t)(2 + c); }

static void note_txt(char *b, uint8_t midi)
{
    static const char nn[12][3] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
    b[0] = nn[midi % 12][0]; b[1] = nn[midi % 12][1]; b[2] = (char)('0' + (midi / 12 - 1) % 10); b[3] = 0;
}

static void cellt(uint8_t s, uint8_t c, const char *t, uint8_t ink)
{
    int x = cx_[c], y = TR_Y + s * TR_P;
    cv_text(x, y + 1, F_SMALL, ink, t);
    if (cur_s == s && cur_c == c) cv_frame(x - 2, y, cv_text_w(F_SMALL, t) + 4, TR_P, CV_ACCENT);
}

static void draw_row(uint8_t s)
{
    int y = TR_Y + s * TR_P;
    uint8_t pat = seq_cur, playing = seq_playing;
    const pattern_t *p = &proj.pat[pat];
    uint8_t ph = playing && (TRK_IS_CORE(app_track) ? seq_step == s : seq_xpos[xt_of_trk[app_track]] == s);
    char b[6];
    cv_rect(0, y, SIDE_X - 6, TR_P, ph ? CV_ACC_TINT : (s & 3) == 0 ? CV_BEAT : CV_BG);
    cv_numfmt(b, (uint16_t)(s + 1), 2, 1);
    cv_text(6, y + 1, F_SMALL, CV_TEXT3, b);
    if (app_track == TRK_BASS) {
        uint8_t f = p->flags[s], t = f & STEP_TYPE_MASK;
        if (t == STEP_NOTE) note_txt(b, (uint8_t)(p->note[s] + 24));
        cellt(s, 0, t == STEP_NOTE ? b : t == STEP_TIE ? "===" : "---", t == STEP_NOTE ? ((f & STEP_ACCENT) ? CV_HOT : CV_TEXT) : CV_TEXT3);
        cellt(s, 1, (f & STEP_ACCENT) ? "A" : ".", (f & STEP_ACCENT) ? CV_TEXT : CV_TEXT3);
        cellt(s, 2, (f & STEP_SLIDE) ? "S" : ".", (f & STEP_SLIDE) ? CV_TEXT : CV_TEXT3);
        cellt(s, 3, (f & STEP_LOCK) ? "L" : ".", (f & STEP_LOCK) ? CV_TEXT : CV_TEXT3);
        b[0] = (char)('1' + (p->prob[s] & 7)); b[1] = 0;
        cellt(s, 4, b, (p->prob[s] & 7) == 7 ? CV_TEXT3 : CV_TEXT);
        cellt(s, 5, cond_n[(p->prob[s] >> 4) % COND_COUNT], (p->prob[s] >> 4) ? CV_TEXT : CV_TEXT3);
    } else if (TRK_IS_DRUM(app_track)) {
        uint8_t c;
        for (c = 0; c < ncols(); c++) {
            uint8_t l = lane_of(c), on = (p->drum[l] >> s) & 1;
            int x = 30 + c * 40;
            cv_rrect(x, y + 1, 11, 5, on ? (l == 0 ? CV_HOT : CV_NOTE) : CV_SURF2);
            if (cur_s == s && cur_c == c) cv_frame(x - 2, y, 15, TR_P, CV_ACCENT);
        }
    } else {
        const xpattern_t *xp = &proj.xpat[pat];
        uint8_t xt = xt_of_trk[app_track], f = xp->trk[xt].flags[s], t = f & STEP_TYPE_MASK;
        uint8_t len = xp->len[xt] ? xp->len[xt] : 16, dim = s >= len;
        if (t == STEP_NOTE) note_txt(b, xp->trk[xt].note[s]);
        cellt(s, 0, t == STEP_NOTE ? b : t == STEP_TIE ? "===" : "---", t == STEP_NOTE && !dim ? CV_TEXT : CV_TEXT3);
        cellt(s, 1, (f & STEP_ACCENT) ? "A" : ".", (f & STEP_ACCENT) ? CV_TEXT : CV_TEXT3);
        cellt(s, 2, (f & STEP_SLIDE) ? "S" : ".", (f & STEP_SLIDE) ? CV_TEXT : CV_TEXT3);
        cellt(s, 3, (f & STEP_LOCK) ? "L" : ".", (f & STEP_LOCK) ? CV_TEXT : CV_TEXT3);
        b[0] = (char)('8' - (f >> XF_PROB_SHIFT)); b[1] = 0;
        cellt(s, 4, b, (f >> XF_PROB_SHIFT) ? CV_TEXT : CV_TEXT3);
        if (app_track == TRK_CHORD) cellt(s, 5, chord_names[xp->shape[s] & 7], CV_TEXT);
    }
}

static void draw_side(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    int x = SIDE_X, y = 30;
    char b[8];
    cv_rect(x, 27, 240 - x, 124, CV_BG);
    cv_readout(x, y, 96, "TRACK", trk_names[app_track]);
    cv_text_r(234, y, F_SMALL, CV_TEXT3, eng_short(app_track));
    cv_numfmt(b, (uint16_t)(seq_cur + 1), 2, 1); cv_readout(x, y + 18, 44, "PATTERN", b);
    cv_numfmt(b, p->gen, 3, 1); cv_readout(x + 50, y + 18, 46, "GENERATION", b);
    y += 40;
    if (TRK_IS_DRUM(app_track)) {
        uint8_t c;
        for (c = 0; c < ncols(); c++) {
            uint8_t l = lane_of(c);
            cv_readout(x, y, 60, lane_n[l], drum_kit_names2[l][p->kit[l] & 3]);
            if (p->lock & (PLOCK_LANE0 << l)) { cv_rect(x + 64, y + 6, 30, 9, CV_ACCENT); cv_text(x + 66, y + 8, F_SMALL, CV_TEXT, "LOCKED"); }
            y += 18;
        }
        cv_text(x, y + 2, F_SMALL, CV_TEXT3, "A+</> KIT   B+v LOCK");
    } else if (app_track == TRK_BASS) {
        if (p->lock & PLOCK_BASS) { cv_rect(x, y, 46, 9, CV_ACCENT); cv_text(x + 2, y + 2, F_SMALL, CV_TEXT, "BASS LOCKED"); }
        if (p->lock & PLOCK_ALL) { cv_rect(x, y + 11, 46, 9, CV_ACCENT); cv_text(x + 2, y + 13, F_SMALL, CV_TEXT, "PATT LOCKED"); }
        cv_text(x, y + 26, F_SMALL, CV_TEXT3, "THE ETCH BASS LINE");
        cv_text(x, y + 33, F_SMALL, CV_TEXT3, "COMPOSED BY CORROSION");
    } else {
        uint8_t xt = xt_of_trk[app_track];
        cv_numfmt(b, proj.xpat[seq_cur].len[xt] ? proj.xpat[seq_cur].len[xt] : 16, 0, 0);
        cv_readout(x, y, 46, "LENGTH", b);
        cv_text(x + 50, y + 8, F_SMALL, CV_TEXT3, "B+</>");
        cv_text(x, y + 22, F_SMALL, CV_TEXT3, "AN ETCH II TRACK");
        if (proj.compat != COMPAT_EXPANDED) { cv_rect(x, y + 32, 96, 9, CV_ACC_TINT); cv_text(x + 2, y + 34, F_SMALL, CV_TEXT, "CLASSIC: SILENT (SEE GEN)"); }
    }
}

void scr_track_enter(void)
{
    uint8_t s;
    ui_header("TRACK");
    if (cur_c >= ncols()) cur_c = 0;
    cv_rect(0, 27, SIDE_X - 6, 9, CV_BG);
    cv_text(6, 29, F_SMALL, CV_TEXT2, "#");
    if (app_track == TRK_BASS || !TRK_IS_DRUM(app_track)) {
        static const char *const h[6] = { "NOTE", "A", "S", "L", "P", "CD" };
        uint8_t c;
        for (c = 0; c < ncols(); c++) cv_text(cx_[c], 29, F_SMALL, CV_TEXT2, c == 5 && app_track == TRK_CHORD ? "SHAPE" : h[c]);
    } else if (app_track == TRK_HATS) { cv_text(28, 29, F_SMALL, CV_TEXT2, "CLOSED"); cv_text(70, 29, F_SMALL, CV_TEXT2, "OPEN"); }
    else cv_text(30, 29, F_SMALL, CV_TEXT2, "HIT");
    cv_vline(SIDE_X - 5, 28, 122, CV_BORDER);
    for (s = 0; s < 16; s++) draw_row(s);
    draw_side();
    ui_help("A EDIT  B+A CLEAR  B+L/R TRACK");
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
