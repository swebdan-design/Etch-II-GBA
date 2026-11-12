/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * GEN: the ORIGINAL CORROSION controls, as on ETCH.
 *
 *   KEY .. MODE   A+<> change, A+^v coarse
 *   SEED          A+<> +-1, A+^v +-256, A (tap) = new seed + GEN ALL
 *   CODE          <> pick a digit, A+^v change it.  The 12 digits are ETCH's
 *                 seed code (SSSS-KCYD-PALO); entering one switches to CLASSIC.
 *   COMPAT        CLASSIC (core only, exactly ETCH) / EXPANDED (+ ETCH II tracks)
 *   X-SEED        expansion seed (digits 13-16 of the extended code)
 *   actions       <> pick, A executes
 */
#include "ui.h"
#include "seq.h"
#include "evolve.h"
#include "expand.h"

enum { R_KEY, R_SCALE, R_STYLE, R_OCT, R_DENS, R_SYNC, R_ACC, R_SLIDE, R_MUT, R_MODE,
       R_SEED, R_CODE, R_COMPAT, R_XSEED, R_ACT1, R_ACT2, R_ACT3, N_ROWS };
static const char *const lbl[R_XSEED + 1] = {
    "KEY    ", "SCALE  ", "STYLE  ", "OCTAVE ", "DENSITY", "SYNCOP ", "ACCENT ", "SLIDE  ",
    "MUTATE ", "MODE   ", "SEED   ", "CODE   ", "COMPAT ", "X-SEED "
};
static const char *const key_n[12] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
static const char *const act_n[3][3] = {
    { "GEN ALL ", "GEN BASS", "GEN DRUM" },
    { "GROOVE  ", "EVOLVE  ", "UNDO    " },
    { "LOCK BAS", "LOCK PAT", "GEN EXT " },
};
static const uint8_t gmax[R_MODE + 1] = { 11, SCALE_COUNT - 1, STYLE_COUNT - 1, 2, 15, 15, 15, 15, 15, EVO_COUNT - 1 };
static uint8_t cur, col, digit, last_pat = 0xFF, last_commits;

static uint8_t *gptr(uint8_t r)
{
    switch (r) {
    case R_KEY: return &proj.gen.key;
    case R_SCALE: return &proj.gen.scale;
    case R_STYLE: return &proj.gen.style;
    case R_OCT: return &proj.gen.octave;
    case R_DENS: return &proj.gen.density;
    case R_SYNC: return &proj.gen.sync;
    case R_ACC: return &proj.gen.accent;
    case R_SLIDE: return &proj.gen.slide;
    case R_MUT: return &proj.gen.mutation;
    default: return &proj.gen.mode;
    }
}

static void draw_row(uint8_t r)
{
    uint8_t y = (uint8_t)(2 + r);
    const pattern_t *p = &proj.pat[seq_cur];
    if (r <= R_XSEED) {
        ui_text(0, y, lbl[r], cur == r ? PAL_CURSOR : PAL_LABEL);
        ui_put(7, y, ' ', PAL_TEXT);
    }
    switch (r) {
    case R_KEY: ui_textn(8, y, key_n[proj.gen.key % 12], 12, PAL_VALUE); break;
    case R_SCALE: ui_textn(8, y, corr_scale_names[proj.gen.scale % SCALE_COUNT], 12, PAL_VALUE); break;
    case R_STYLE: ui_textn(8, y, corr_style_names[proj.gen.style % STYLE_COUNT], 12, PAL_VALUE); break;
    case R_OCT: ui_num(8, y, (uint16_t)(proj.gen.octave + 1), 1, PAL_VALUE); ui_fill(9, y, 11, ' ', PAL_VALUE); break;
    case R_MODE: ui_textn(8, y, corr_mode_names[proj.gen.mode % EVO_COUNT], 12, PAL_VALUE); break;
    case R_SEED: ui_hex(8, y, proj.gen.seed, 4, PAL_VALUE); ui_text(13, y, "A:NEW+GEN", PAL_DIM); break;
    case R_CODE: {
        char c[12];
        uint8_t i, x = 8;
        corr_code_encode(&proj.gen, c);
        for (i = 0; i < 12; i++) {
            if (i == 4 || i == 8) ui_put(x++, y, '-', PAL_DIM);
            ui_put(x++, y, (uint8_t)c[i], (cur == R_CODE && digit == i) ? PAL_CURSOR : PAL_VALUE);
        }
        break;
    }
    case R_COMPAT:
        ui_textn(8, y, proj.compat == COMPAT_EXPANDED ? "EXPANDED" : "CLASSIC ", 9, PAL_VALUE);
        ui_textn(17, y, proj.compat == COMPAT_EXPANDED ? "+ETCH II TRKS" : "=ETCH CORE   ", 13, PAL_DIM);
        break;
    case R_XSEED: ui_hex(8, y, proj.xseed, 4, PAL_VALUE); ui_text(13, y, "A:NEW+GEN EXT", PAL_DIM); break;
    case R_ACT1: case R_ACT2: case R_ACT3: {
        uint8_t i, a = (uint8_t)(r - R_ACT1);
        for (i = 0; i < 3; i++) {
            uint8_t on = (a == 2 && i == 0 && (p->lock & PLOCK_BASS)) || (a == 2 && i == 1 && (p->lock & PLOCK_ALL));
            ui_textn((uint8_t)(i * 10), y, act_n[a][i], 9, (cur == r && col == i) ? PAL_CURSOR : on ? PAL_ALERT : PAL_TEXT);
        }
        break;
    }
    default:
        ui_num(8, y, *gptr(r), 2, PAL_VALUE);
        ui_bar(11, y, 8, *gptr(r), 15, PAL_DIM);
        break;
    }
}

void scr_gen_enter(void)
{
    uint8_t r;
    ui_header("GEN");
    ui_text(0, 1, "CORROSION v2.4", PAL_DIM);
    ui_text(16, 1, "PAT", PAL_DIM);
    ui_num0(20, 1, (uint16_t)(seq_cur + 1), 2, PAL_VALUE);
    ui_text(23, 1, "GEN", PAL_DIM);
    ui_num0(27, 1, proj.pat[seq_cur].gen, 3, PAL_VALUE);
    for (r = 0; r < N_ROWS; r++) draw_row(r);
    ui_help("A+</>:SET  SEED CODE = ETCH");
    last_pat = seq_cur;
    last_commits = evo_commits;
}

static void generate(uint8_t what)
{
    if (proj.pat[seq_cur].lock & PLOCK_ALL) { ui_msg("PATTERN LOCKED"); return; }
    app_generate(what);
    ui_msg(what == GEN_ALL ? "GENERATED (UNDO: SELECT+B)" : what == GEN_BASS ? "BASS GENERATED" : "DRUMS GENERATED");
}

static void action(uint8_t a, uint8_t i)
{
    pattern_t *p = &proj.pat[seq_cur];
    switch (a * 3 + i) {
    case 0: generate(GEN_ALL); break;
    case 1: generate(GEN_BASS); break;
    case 2: generate(GEN_DRUMS); break;
    case 3: seq_backup_for_undo(seq_cur); corr_drum_preset(p, proj.gen.style); app_pattern_edited(seq_cur); ui_msg("STYLE GROOVE LOADED"); break;
    case 4: app_evolve_now(); ui_msg(seq_playing ? "EVOLVE AT NEXT BAR" : "EVOLVED"); break;
    case 5: ui_msg(app_undo() ? "UNDO: PREVIOUS GENERATION" : "NOTHING TO UNDO"); break;
    case 6: p->lock ^= PLOCK_BASS; app_pattern_edited(seq_cur); break;
    case 7: p->lock ^= PLOCK_ALL; app_pattern_edited(seq_cur); break;
    default: {
        xpattern_t x;
        memcpy(&x, &proj.xpat[seq_cur], sizeof x);
        expand_generate(&x, p, &proj.gen, proj.xseed, seq_cur, XG_ALL);
        CRITICAL { memcpy(&proj.xpat[seq_cur], &x, sizeof x); }
        if (proj.compat != COMPAT_EXPANDED) app_set_compat(COMPAT_EXPANDED);
        ui_msg("EXPANSION TRACKS GENERATED");
        break;
    }
    }
    scr_gen_enter();
}

void scr_gen_frame(void)
{
    uint8_t nav = ui_nav(), o = cur;
    int8_t d = ui_edit(4);
    if ((nav & 4) && cur) cur--;
    if ((nav & 8) && cur + 1 < N_ROWS) cur++;
    if (cur == R_CODE) {
        if ((nav & 1) && digit < 11) { digit++; draw_row(R_CODE); }
        if ((nav & 2) && digit) { digit--; draw_row(R_CODE); }
    }
    if (cur >= R_ACT1) {
        if ((nav & 1) && col < 2) { col++; draw_row(cur); }
        if ((nav & 2) && col) { col--; draw_row(cur); }
    }
    if (o != cur) { draw_row(o); draw_row(cur); }

    if (cur <= R_MODE && d) {
        uint8_t *v = gptr(cur);
        *v = clampadd(*v, d > 1 ? 4 : d < -1 ? -4 : d, 0, gmax[cur]);
        draw_row(cur); draw_row(R_CODE);
    } else if (cur == R_SEED) {
        if (d) { proj.gen.seed = (uint16_t)(proj.gen.seed + (d > 1 ? 256 : d < -1 ? -256 : d)); draw_row(R_SEED); draw_row(R_CODE); }
        if (ui_a_tap()) { proj.gen.seed = evo_random(); generate(GEN_ALL); scr_gen_enter(); }
    } else if (cur == R_CODE && d) {
        /* digit entry: the ORIGINAL decoder validates every field */
        char c[12];
        gen_t g = proj.gen;
        int8_t v;
        corr_code_encode(&proj.gen, c);
        v = (int8_t)(c[digit] >= 'A' ? c[digit] - 'A' + 10 : c[digit] - '0');
        v = (int8_t)((v + (d > 0 ? 1 : 15)) & 15);
        c[digit] = (char)(v < 10 ? '0' + v : 'A' + v - 10);
        if (corr_code_decode(&g, c)) {
            proj.gen = g;
            if (proj.compat != COMPAT_CLASSIC) { app_set_compat(COMPAT_CLASSIC); ui_msg("SEED CODE: CLASSIC MODE"); }
        }
        scr_gen_enter();
    } else if (cur == R_COMPAT && d) {
        app_set_compat(d > 0 ? COMPAT_EXPANDED : COMPAT_CLASSIC);
        scr_gen_enter();
    } else if (cur == R_XSEED) {
        if (d) { proj.xseed = (uint16_t)(proj.xseed + (d > 1 ? 256 : d < -1 ? -256 : d)); draw_row(R_XSEED); }
        if (ui_a_tap()) { proj.xseed = evo_random(); action(2, 2); }
    } else if (cur >= R_ACT1 && ui_a_tap()) {
        action((uint8_t)(cur - R_ACT1), col);
    }
    if (seq_cur != last_pat || evo_commits != last_commits) scr_gen_enter();
}
