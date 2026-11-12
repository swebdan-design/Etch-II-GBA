/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * MEMORY: a window onto CORROSION's ORIGINAL musical memory and evolution
 * state.  Nothing here is a new algorithm: it shows ETCH's per-pattern DRIFT
 * anchors, their divergence against ETCH's bounds, the candidate hand-off,
 * the undo buffer, sound drift and the entropy pool.  The two actions are
 * ETCH's own "your edit becomes the new memory" (forget the anchor).
 */
#include "ui.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"

static uint8_t act, last_commits, last_step;

static void draw(void)
{
    uint8_t i, y;
    uint8_t bmax = (uint8_t)(2 + proj.gen.mutation / 6), dmax = (uint8_t)(1 + (proj.gen.mutation + 4) / 8);
    ui_text(0, 1, "ANCHORS (DRIFT MEMORY)  DIVERG", PAL_DIM);
    for (i = 0; i < N_ANCHORS; i++) {
        y = (uint8_t)(2 + i);
        ui_put(0, y, 'A', PAL_LABEL);
        ui_num(1, y, (uint16_t)(i + 1), 1, PAL_LABEL);
        if (anchor_id[i] < E2_PATTERNS) {
            uint8_t dv = anchor_divergence(i), p = anchor_id[i];
            ui_text(3, y, "PAT", PAL_DIM);
            ui_num0(7, y, (uint16_t)(p + 1), 2, p == seq_cur ? PAL_PLAY : PAL_VALUE);
            ui_text(10, y, "GEN", PAL_DIM);
            ui_num0(14, y, proj.pat[p].gen, 3, PAL_VALUE);
            ui_num(18, y, dv, 2, PAL_VALUE);
            ui_bar(21, y, 8, dv, (uint8_t)(bmax + 4 * dmax), dv > bmax + 4 * dmax ? PAL_ALERT : PAL_DIM);
        } else {
            ui_textn(3, y, "--- EMPTY", 27, PAL_DIM);
        }
    }
    ui_text(0, 10, "BOUND BASS", PAL_DIM); ui_num(11, 10, bmax, 2, PAL_VALUE);
    ui_text(14, 10, "STEPS LANE", PAL_DIM); ui_num(25, 10, dmax, 2, PAL_VALUE);
    ui_text(0, 11, "MODE", PAL_LABEL);
    ui_textn(5, 11, corr_mode_names[proj.gen.mode % 3], 8, PAL_VALUE);
    ui_text(13, 11, "PHRASE", PAL_LABEL); ui_num(20, 11, seq_phrase, 3, PAL_VALUE);
    ui_text(24, 11, "NXT", PAL_LABEL);
    { uint8_t b = seq_bars_to_evolution(); if (b == 0xFF) ui_text(28, 11, "--", PAL_DIM); else ui_num(28, 11, b, 2, PAL_VALUE); }
    ui_text(0, 12, "CANDIDATE", PAL_LABEL);
    if (evo_state == EVO_READY) { ui_text(10, 12, "READY FOR P", PAL_VALUE); ui_num0(21, 12, (uint16_t)(evo_target + 1), 2, PAL_VALUE); ui_fill(23, 12, 7, ' ', 0); }
    else ui_textn(10, 12, "PREPARING / IDLE", 20, PAL_DIM);
    ui_text(0, 13, "COMMITS", PAL_LABEL); ui_num(8, 13, evo_commits, 3, PAL_VALUE);
    ui_text(12, 13, "STALE DROPPED", PAL_LABEL); ui_num(26, 13, evo_stale_drops, 3, PAL_VALUE);
    ui_text(0, 14, "UNDO", PAL_LABEL);
    if (evo_undo_pat < E2_PATTERNS) { ui_text(5, 14, "P", PAL_VALUE); ui_num0(6, 14, (uint16_t)(evo_undo_pat + 1), 2, PAL_VALUE); }
    else ui_text(5, 14, "---", PAL_DIM);
    ui_text(9, 14, "SOUND DRIFT", PAL_LABEL);
    {
        int8_t c = evo_cut_ofs;
        ui_put(21, 14, c < 0 ? '-' : '+', PAL_VALUE); ui_num(22, 14, (uint16_t)(c < 0 ? -c : c), 1, PAL_VALUE);
        for (i = 0; i < NUM_LANES; i++) {
            int8_t t = evo_tune_ofs[i];
            ui_put((uint8_t)(24 + i * 1), 14, t < 0 ? 'v' : t > 0 ? '^' : '.', PAL_VALUE);
        }
    }
    ui_text(0, 15, "ENTROPY", PAL_LABEL);
    ui_hex(8, 15, (uint16_t)(ui_entropy >> 16), 4, PAL_VALUE); ui_hex(12, 15, (uint16_t)ui_entropy, 4, PAL_VALUE);
    ui_text(17, 15, "LAST", PAL_LABEL); ui_hex(22, 15, evo_rng_peek, 4, PAL_VALUE);
    ui_text(0, 17, "FORGET THIS PATTERN", act == 0 ? PAL_CURSOR : PAL_TEXT);
    ui_text(20, 17, "FORGET ALL", act == 1 ? PAL_CURSOR : PAL_TEXT);
}

void scr_memory_enter(void)
{
    ui_header("MEMORY");
    draw();
    ui_help("CORROSION STATE  A:FORGET");
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
