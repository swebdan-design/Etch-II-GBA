/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * MEMORY: a window onto CORROSION's ORIGINAL musical memory and evolution
 * state.  Nothing here is a new algorithm: it shows ETCH's per-pattern DRIFT
 * anchors, their divergence against ETCH's bounds, the candidate hand-off,
 * the undo buffer, sound drift and the entropy pool.  The two actions are
 * ETCH's own "your edit becomes the new memory" (forget the anchor).
 */
#include <string.h>
#include "ui.h"
#include "canvas.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"

static uint8_t act, last_commits, last_step;

static void hex4(char *b, uint16_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    b[0] = hx[v >> 12]; b[1] = hx[(v >> 8) & 15]; b[2] = hx[(v >> 4) & 15]; b[3] = hx[v & 15]; b[4] = 0;
}

#define AN_Y 38
#define AN_P 8
#define RO_Y 106

static void draw(void)
{
    uint8_t i;
    char b[24];
    uint8_t bmax = (uint8_t)(2 + proj.gen.mutation / 6), dmax = (uint8_t)(1 + (proj.gen.mutation + 4) / 8);
    /* top line: the bounds and the entropy pool */
    cv_rect(0, 27, CV_W, 9, CV_BG);
    {
        int x = cv_text(6, 29, F_SMALL, CV_TEXT2, "BOUND");
        x = cv_num(x + 4, 28, F_MAIN, CV_TEXT, bmax, 0, 0);
        x = cv_text(x + 3, 29, F_SMALL, CV_TEXT2, "BASS");
        x = cv_num(x + 4, 28, F_MAIN, CV_TEXT, dmax, 0, 0);
        cv_text(x + 3, 29, F_SMALL, CV_TEXT2, "PER DRUM LANE");
        hex4(b, (uint16_t)(ui_entropy >> 16)); hex4(b + 4, (uint16_t)ui_entropy);
        cv_text_r(234, 28, F_MAIN, CV_TEXT, b);
        cv_text_r(232 - cv_text_w(F_MAIN, b), 29, F_SMALL, CV_TEXT2, "ENTROPY");
    }
    /* the anchors */
    for (i = 0; i < N_ANCHORS; i++) {
        int y = AN_Y + i * AN_P;
        cv_rect(0, y - 1, CV_W, AN_P, CV_BG);
        b[0] = 'A'; cv_numfmt(b + 1, (uint16_t)(i + 1), 0, 0);
        cv_text(6, y + 1, F_SMALL, CV_TEXT2, b);
        if (anchor_id[i] < E2_PATTERNS) {
            uint8_t dv = anchor_divergence(i), p = anchor_id[i], lim = (uint8_t)(bmax + 4 * dmax);
            cv_text(20, y + 1, F_SMALL, CV_TEXT2, "PATTERN");
            cv_num(50, y, F_MAIN, p == seq_cur ? CV_HOT : CV_TEXT, (uint16_t)(p + 1), 2, 1);
            if (p == seq_cur) cv_rect(64, y + 2, 3, 3, CV_ACCENT);
            cv_text(74, y + 1, F_SMALL, CV_TEXT2, "GEN");
            cv_num(90, y, F_MAIN, CV_TEXT, proj.pat[p].gen, 3, 1);
            cv_text(116, y + 1, F_SMALL, CV_TEXT2, "DIVERGENCE");
            cv_num(160, y, F_MAIN, CV_TEXT, dv, 2, 0);
            cv_slider(176, y, 58, dv > lim ? lim : dv, lim, dv > lim);
        } else cv_text(20, y + 1, F_SMALL, CV_TEXT3, "EMPTY");
    }
    cv_hline(6, RO_Y - 3, 228, CV_BORDER);
    /* readouts */
    cv_readout(6, RO_Y, 54, "MODE", corr_mode_names[proj.gen.mode % 3]);
    cv_numfmt(b, seq_phrase, 0, 0); cv_readout(64, RO_Y, 54, "PHRASE", b);
    { uint8_t n = seq_bars_to_evolution(); if (n == 0xFF) strcpy(b, "--"); else cv_numfmt(b, n, 0, 0); }
    cv_readout(122, RO_Y, 54, "NEXT EVO", b);
    if (evo_state == EVO_READY) { strcpy(b, "READY P"); cv_numfmt(b + 7, (uint16_t)(evo_target + 1), 2, 1); }
    else strcpy(b, "IDLE");
    cv_readout(180, RO_Y, 54, "CANDIDATE", b);
    cv_numfmt(b, evo_commits, 0, 0); cv_readout(6, RO_Y + 15, 54, "COMMITS", b);
    cv_numfmt(b, evo_stale_drops, 0, 0); cv_readout(64, RO_Y + 15, 54, "STALE REFUSED", b);
    if (evo_undo_pat < E2_PATTERNS) { b[0] = 'P'; cv_numfmt(b + 1, (uint16_t)(evo_undo_pat + 1), 2, 1); } else strcpy(b, "--");
    cv_readout(122, RO_Y + 15, 54, "UNDO", b);
    {
        int8_t c = evo_cut_ofs;
        uint8_t k = 0;
        b[k++] = c < 0 ? '-' : '+'; b[k++] = (char)('0' + (c < 0 ? -c : c) % 10); b[k++] = ' ';
        for (i = 0; i < NUM_LANES; i++) { int8_t t = evo_tune_ofs[i]; b[k++] = t < 0 ? 'v' : t > 0 ? '^' : '.'; }
        b[k] = 0;
        cv_readout(180, RO_Y + 15, 54, "SOUND DRIFT", b);
    }
    cv_button(6, 138, 112, 11, "FORGET THIS PATTERN", act == 0, 0);
    cv_button(122, 138, 112, 11, "FORGET ALL", act == 1, 0);
}

void scr_memory_enter(void)
{
    ui_header("MEMORY");
    draw();
    ui_help("CORROSION'S MEMORY  </> CHOOSE  A FORGET");
    last_commits = evo_commits;
}

void scr_memory_frame(void)
{
    uint8_t nav = ui_nav();
    if (nav & 1) { act = 1; draw(); }
    if (nav & 2) { act = 0; draw(); }
    if (ui_a_tap()) {
        anchors_forget(act ? 0xFF : seq_cur);
        ui_msg(act ? "ALL ANCHORS FORGOTTEN" : "ANCHOR FORGOTTEN: NEW MEMORY");
        draw();
    }
    if (evo_commits != last_commits || seq_step_event != last_step) {
        last_commits = evo_commits; last_step = seq_step_event;
        if ((seq_step_event & 3) == 0) draw();
    }
}
