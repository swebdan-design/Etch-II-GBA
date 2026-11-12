/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * SONG: the pattern chain (32 positions), tempo, swing, gate, transpose and
 * pattern tools.  Chain cells: A+<> changes the pattern (END = stop and
 * wrap) or the repeats (1-16); A (tap) on a position jumps there at the next
 * bar.  Copy / paste / clear act on the current pattern; paste and clear
 * also bump the pattern's edit epoch, so a pending evolution prepared for
 * the old content can never land on the new one.
 */
#include "ui.h"
#include "seq.h"
#include "evolve.h"
#include "audio.h"

enum { P_SONG, P_BPM, P_SWING, P_GATE, P_TUNE, P_START, P_COPY, P_PASTE, P_CLEAR, N_PANEL };
static const char *const pl[N_PANEL] = { "MODE ", "BPM  ", "SWING", "GATE ", "TUNE ", "START", "COPY PAT", "PASTE PAT", "CLEAR PAT" };
static uint8_t pos, fld, panel, prow;
static pattern_t clip EWRAM_BSS;
static xpattern_t xclip EWRAM_BSS;
static uint8_t have_clip, last_pos = 0xFF, last_pat = 0xFF;

static void draw_cell(uint8_t i)
{
    uint8_t x = (uint8_t)((i >> 4) * 10), y = (uint8_t)(2 + (i & 15));
    uint8_t p = proj.song.pat[i], playing = proj.song.song && seq_playing && seq_chain_pos == i;
    ui_num0(x, y, (uint16_t)(i + 1), 2, playing ? PAL_PLAYHEAD : PAL_DIM);
    if (p < E2_PATTERNS) { ui_put((uint8_t)(x + 3), y, 'P', !panel && pos == i && fld == 0 ? PAL_CURSOR : PAL_VALUE); ui_num0((uint8_t)(x + 4), y, (uint16_t)(p + 1), 2, !panel && pos == i && fld == 0 ? PAL_CURSOR : PAL_VALUE); }
    else ui_text((uint8_t)(x + 3), y, "END", !panel && pos == i && fld == 0 ? PAL_CURSOR : PAL_DIM);
    ui_put((uint8_t)(x + 6), y, 'x', PAL_DIM);
    ui_num((uint8_t)(x + 7), y, proj.song.rep[i], 2, !panel && pos == i && fld == 1 ? PAL_CURSOR : PAL_TEXT);
}

static void draw_panel(void)
{
    uint8_t r;
    static const uint8_t gfr[4] = { 6, 9, 12, 15 };
    for (r = 0; r < N_PANEL; r++) {
        uint8_t y = (uint8_t)(2 + r + (r >= P_COPY ? 2 : 0));
        ui_textn(21, y, pl[r], 9, panel && prow == r ? PAL_CURSOR : PAL_LABEL);
        if (r >= P_COPY) continue;
        ui_fill(21, (uint8_t)(y + 1), 0, ' ', 0);
    }
    ui_textn(27, 2, proj.song.song ? "SNG" : "LP ", 3, PAL_VALUE);
    ui_num(27, 3, proj.bpm, 3, PAL_VALUE);
    ui_num(27, 4, proj.swing, 3, PAL_VALUE);
    ui_num(27, 5, gfr[proj.gate & 3], 3, PAL_VALUE);
    { int8_t t = (int8_t)(proj.tune - 12); ui_put(27, 6, t < 0 ? '-' : '+', PAL_VALUE); ui_num(28, 6, (uint16_t)(t < 0 ? -t : t), 2, PAL_VALUE); }
    ui_num0(28, 7, (uint16_t)(proj.start_pattern + 1), 2, PAL_VALUE);
    ui_text(21, 14, "CUR PAT", PAL_DIM); ui_num0(28, 14, (uint16_t)(seq_cur + 1), 2, PAL_VALUE);
    ui_text(21, 15, have_clip ? "CLIP OK" : "       ", PAL_DIM);
}

void scr_song_enter(void)
{
    uint8_t i;
    ui_header("SONG");
    ui_text(0, 1, "CHAIN  POS PAT x BARS", PAL_DIM);
    for (i = 0; i < E2_SONG_LEN; i++) draw_cell(i);
    draw_panel();
    ui_help("A+</>:SET A:JUMP  SEL+^v:QUEUE");
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
        ui_num0(28, 14, (uint16_t)(seq_cur + 1), 2, PAL_VALUE);
    }
}
