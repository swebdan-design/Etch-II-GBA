/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * SONG: the pattern chain (32 positions), tempo, swing, gate, transpose and
 * pattern tools.  Chain cells: A+<> changes the pattern (END = stop and
 * wrap) or the repeats (1-16); A (tap) on a position jumps there at the next
 * bar.  Copy / paste / clear act on the current pattern; paste and clear
 * also bump the pattern's edit epoch, so a pending evolution prepared for
 * the old content can never land on the new one.
 */
#include <string.h>
#include "ui.h"
#include "canvas.h"
#include "seq.h"
#include "evolve.h"
#include "audio.h"

enum { P_SONG, P_BPM, P_SWING, P_GATE, P_TUNE, P_START, P_COPY, P_PASTE, P_CLEAR, N_PANEL };
static const char *const pl[N_PANEL] = { "MODE", "BPM", "SWING", "GATE", "TUNE", "START", "COPY", "PASTE", "CLEAR" };
static uint8_t pos, fld, panel, prow;
static pattern_t clip EWRAM_BSS;
static xpattern_t xclip EWRAM_BSS;
static uint8_t have_clip, last_pos = 0xFF, last_pat = 0xFF;

/* layout: the chain in two columns of 16 (left), the song panel (right) */
#define CH_Y  38
#define CH_P  7
#define CH_W  54
#define PN_X  126
#define PN_Y  38
#define PN_P  9

static void draw_cell(uint8_t i)
{
    int x = 6 + (i >> 4) * (CH_W + 4), y = CH_Y + (i & 15) * CH_P;
    uint8_t p = proj.song.pat[i], playing = proj.song.song && seq_playing && seq_chain_pos == i;
    uint8_t c0 = !panel && pos == i && fld == 0, c1 = !panel && pos == i && fld == 1;
    char b[8];
    cv_rect(x - 2, y, CH_W + 2, CH_P, playing ? CV_ACC_TINT : ((i & 3) == 0 ? CV_BEAT : CV_BG));
    cv_numfmt(b, (uint16_t)(i + 1), 2, 1);
    cv_text(x, y + 1, F_SMALL, CV_TEXT3, b);
    if (p < E2_PATTERNS) { b[0] = 'P'; cv_numfmt(b + 1, (uint16_t)(p + 1), 2, 1); }
    else strcpy(b, "END");
    cv_text(x + 14, y + 1, F_SMALL, p < E2_PATTERNS ? CV_TEXT : CV_TEXT3, b);
    if (c0) cv_frame(x + 12, y, 16, CH_P, CV_ACCENT);
    b[0] = 'X'; cv_numfmt(b + 1, proj.song.rep[i], 0, 0);
    cv_text(x + 34, y + 1, F_SMALL, CV_TEXT2, b);
    if (c1) cv_frame(x + 32, y, 16, CH_P, CV_ACCENT);
}

static void draw_panel(void)
{
    static const uint8_t gfr[4] = { 6, 9, 12, 15 };
    char b[8];
    uint8_t r;
    for (r = 0; r <= P_START; r++) {
        int y = PN_Y + r * PN_P, vx = PN_X + 32;
        uint8_t sel = panel && prow == r;
        cv_rect(PN_X - 4, y - 1, 234 - PN_X + 4, PN_P, CV_BG);
        cv_cap(PN_X, y, sel, pl[r]);
        switch (r) {
        case P_SONG: cv_text(vx, y, F_MAIN, CV_TEXT, proj.song.song ? "SONG" : "LOOP"); break;
        case P_BPM: cv_num(vx, y, F_MAIN, CV_TEXT, proj.bpm, 0, 0); cv_slider(vx + 22, y, 50, (uint16_t)(proj.bpm - 60), 160, sel); break;
        case P_SWING: cv_num(vx, y, F_MAIN, CV_TEXT, proj.swing, 0, 0); cv_slider(vx + 22, y, 50, proj.swing, 8, sel); break;
        case P_GATE: cv_num(vx, y, F_MAIN, CV_TEXT, gfr[proj.gate & 3], 0, 0); cv_slider(vx + 22, y, 50, proj.gate & 3, 3, sel); break;
        case P_TUNE: {
            int8_t t = (int8_t)(proj.tune - 12);
            b[0] = t < 0 ? '-' : '+'; cv_numfmt(b + 1, (uint16_t)(t < 0 ? -t : t), 0, 0);
            cv_text(vx, y, F_MAIN, CV_TEXT, b);
            break;
        }
        default: b[0] = 'P'; cv_numfmt(b + 1, (uint16_t)(proj.start_pattern + 1), 2, 1); cv_text(vx, y, F_MAIN, CV_TEXT, b); break;
        }
    }
    cv_button(PN_X, 98, 34, 12, "COPY", panel && prow == P_COPY, 0);
    cv_button(PN_X + 37, 98, 34, 12, "PASTE", panel && prow == P_PASTE, have_clip);
    cv_button(PN_X + 74, 98, 34, 12, "CLEAR", panel && prow == P_CLEAR, 0);
    cv_numfmt(b, (uint16_t)(seq_cur + 1), 2, 1);
    cv_readout(PN_X, 118, 50, "PATTERN", b);
    cv_readout(PN_X + 56, 118, 50, "CLIPBOARD", have_clip ? "FULL" : "EMPTY");
}

void scr_song_enter(void)
{
    uint8_t i;
    ui_header("SONG");
    cv_text(6, 29, F_SMALL, CV_TEXT2, "CHAIN");
    cv_text(30, 29, F_SMALL, CV_TEXT3, "PATTERN X BARS");
    cv_vline(PN_X - 7, CH_Y, 16 * CH_P, CV_BORDER);
    for (i = 0; i < E2_SONG_LEN; i++) draw_cell(i);
    draw_panel();
    ui_help("A+</> SET  A JUMP  SELECT+^v QUEUE");
}

void scr_song_frame(void)
{
    uint8_t nav = ui_nav(), op = pos, of = fld, opanel = panel, orow = prow;
    int8_t d = ui_edit(4);
    if (!panel) {
        if (nav & 4) pos = (uint8_t)((pos + 31) & 31);
        if (nav & 8) pos = (uint8_t)((pos + 1) & 31);
        if (nav & 1) { if (fld == 0) fld = 1; else if (pos < 16) { pos = (uint8_t)(pos + 16); fld = 0; } else panel = 1; }
        if (nav & 2) { if (fld == 1) fld = 0; else if (pos >= 16) { pos = (uint8_t)(pos - 16); fld = 1; } }
        if (d) {
            if (fld == 0) {
                uint8_t p = proj.song.pat[pos];
                int16_t v = (p >= E2_PATTERNS ? E2_PATTERNS : p) + (d > 1 ? 4 : d < -1 ? -4 : d);
                if (v < 0) v = 0;
                if (v > E2_PATTERNS) v = E2_PATTERNS;
                proj.song.pat[pos] = (uint8_t)(v == E2_PATTERNS ? 0xFF : v);
            } else proj.song.rep[pos] = clampadd(proj.song.rep[pos], d > 0 ? 1 : -1, 1, 16);
            draw_cell(pos);
        }
        if (ui_a_tap()) {
            if (proj.song.pat[pos] < E2_PATTERNS) {
                proj.song.song = 1;
                seq_jump_chain(pos);
                ui_msg(seq_playing ? "JUMP AT NEXT BAR" : "CHAIN POSITION SET");
                draw_panel();
            }
        }
    } else {
        if ((nav & 4) && prow) prow--;
        if ((nav & 8) && prow + 1 < N_PANEL) prow++;
        if (nav & 2) { panel = 0; pos = (uint8_t)(16 + (pos & 15)); fld = 1; }
        if (d) {
            switch (prow) {
            case P_SONG: proj.song.song = d > 0; break;
            case P_BPM: proj.bpm = clampadd(proj.bpm, d > 1 ? 10 : d < -1 ? -10 : d, 60, 220); seq_set_tempo(); break;
            case P_SWING: proj.swing = clampadd(proj.swing, d > 0 ? 1 : -1, 0, 8); seq_set_tempo(); break;
            case P_GATE: proj.gate = clampadd(proj.gate, d > 0 ? 1 : -1, 0, 3); seq_set_tempo(); break;
            case P_TUNE: proj.tune = clampadd(proj.tune, d > 1 ? 12 : d < -1 ? -12 : d, 0, 24); break;
            case P_START: proj.start_pattern = clampadd(proj.start_pattern, d > 0 ? 1 : -1, 0, E2_PATTERNS - 1); break;
            }
            draw_panel();
        }
        if (ui_a_tap()) {
            if (prow == P_COPY) { clip = proj.pat[seq_cur]; xclip = proj.xpat[seq_cur]; have_clip = 1; ui_msg("PATTERN COPIED"); }
            if (prow == P_PASTE && have_clip) {
                seq_backup_for_undo(seq_cur);
                CRITICAL { proj.pat[seq_cur] = clip; proj.xpat[seq_cur] = xclip; }
                app_pattern_edited(seq_cur);
                ui_msg("PATTERN PASTED (UNDO: SEL+B)");
            }
            if (prow == P_CLEAR) {
                pattern_t p;
                xpattern_t x;
                pattern_clear(&p); xpattern_clear(&x);
                p.kit[1] = 2;
                seq_backup_for_undo(seq_cur);
                CRITICAL { proj.pat[seq_cur] = p; proj.xpat[seq_cur] = x; }
                app_pattern_edited(seq_cur);
                ui_msg("PATTERN CLEARED (UNDO: SEL+B)");
            }
            draw_panel();
        }
    }
    if (op != pos || of != fld || opanel != panel) { draw_cell(op); draw_cell(pos); draw_panel(); }
    if (orow != prow) draw_panel();
    if (seq_chain_pos != last_pos || seq_cur != last_pat) {
        if (last_pos < E2_SONG_LEN) draw_cell(last_pos);
        last_pos = seq_chain_pos; last_pat = seq_cur;
        draw_cell(last_pos);
        draw_panel();
    }
}
