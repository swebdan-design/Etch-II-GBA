/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * FILE: the album, load / save (verified), new project, project name, seed
 * codes (import / export), about, and diagnostics.  ^v pick, A open / do,
 * B back.
 */
#include "ui.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"
#include "storage.h"
#include "album.h"
#include "expand.h"

enum { M_ALBUM, M_LOAD, M_SAVE, M_NEW, M_NAME, M_CODE, M_ABOUT, M_DIAG, M_COUNT };
static const char *const menu_n[M_COUNT] = {
    "ALBUM      the 8 living pieces", "LOAD       a saved project", "SAVE       this project",
    "NEW        empty project", "NAME       rename project", "SEED CODE  import / export",
    "ABOUT      ETCH II", "DIAGNOSTICS"
};
static uint8_t mode = 0xFF, cur, sub, confirm, page;
static char code_buf[16];
static uint8_t code_len = 12;
extern volatile uint16_t main_worst;
extern volatile uint32_t frame_count;

static void draw_menu(void)
{
    uint8_t i;
    ui_text(0, 2, "PROJECT", PAL_DIM);
    ui_textn(8, 2, proj.name, 10, PAL_VALUE);
    for (i = 0; i < M_COUNT; i++) ui_textn(1, (uint8_t)(4 + i * 2), menu_n[i], 29, cur == i ? PAL_CURSOR : PAL_TEXT);
    ui_help("A:OPEN  B:BACK");
}

static void draw_album(void)
{
    uint8_t i;
    ui_text(0, 1, "ALBUM  A:LOAD AND PLAY", PAL_DIM);
    for (i = 0; i < ALBUM_TRACKS; i++) {
        uint8_t y = (uint8_t)(2 + i * 2);
        ui_num(0, y, (uint16_t)(i + 1), 1, PAL_DIM);
        ui_textn(2, y, album[i].title, 9, sub == i ? PAL_CURSOR : (proj.album_id == i ? PAL_PLAY : PAL_VALUE));
        ui_textn(12, y, album[i].line1, 18, PAL_TEXT);
        ui_textn(2, (uint8_t)(y + 1), album[i].line2, 28, PAL_DIM);
    }
    ui_text(0, 18, "CORE SEED", PAL_LABEL);
    ui_textn(10, 18, album[sub].code, 14, PAL_VALUE);
}

static void draw_slots(uint8_t saving)
{
    uint8_t s;
    ui_text(0, 1, saving ? "SAVE TO SLOT" : "LOAD FROM SLOT", PAL_DIM);
    for (s = 0; s < SAVE_SLOTS; s++) {
        char nm[E2_NAME_LEN];
        uint8_t st = storage_slot_status(s, nm), y = (uint8_t)(3 + s * 2);
        ui_text(0, y, "SLOT", sub == s ? PAL_CURSOR : PAL_LABEL);
        ui_num(5, y, (uint16_t)(s + 1), 1, sub == s ? PAL_CURSOR : PAL_LABEL);
        if (st == SLOT_OK) ui_textn(8, y, nm, 10, PAL_VALUE);
        else ui_textn(8, y, st == SLOT_EMPTY ? "EMPTY" : "BAD (CORRUPT)", 13, st == SLOT_EMPTY ? PAL_DIM : PAL_ALERT);
    }
    ui_text(0, 12, "32KB SRAM, 4 SLOTS x 8KB", PAL_DIM);
    ui_text(0, 13, "CRC-32 VERIFIED ON SAVE+LOAD", PAL_DIM);
    if (confirm) ui_text(0, 15, saving ? "REPLACE? A:YES  B:NO" : "LOAD? A:YES  B:NO   ", PAL_ALERT);
    else ui_fill(0, 15, 30, ' ', PAL_TEXT);
}

static void draw_name(void)
{
    uint8_t i;
    ui_text(0, 2, "PROJECT NAME", PAL_DIM);
    for (i = 0; i < E2_NAME_LEN; i++) ui_put((uint8_t)(4 + i), 4, (uint8_t)proj.name[i], sub == i ? PAL_CURSOR : PAL_VALUE);
    ui_text(0, 6, "</>:MOVE  A+^v:LETTER", PAL_DIM);
    ui_text(0, 7, "B:DONE", PAL_DIM);
}

static void fmt_code(const char *c, uint8_t n, uint8_t x, uint8_t y)
{
    uint8_t i;
    for (i = 0; i < n; i++) {
        if (i && (i & 3) == 0) ui_put(x++, y, '-', PAL_DIM);
        ui_put(x++, y, (uint8_t)c[i], PAL_VALUE);
    }
}

static void draw_code(void)
{
    uint8_t i, x = 2;
    char cur_code[16];
    ui_text(0, 1, "SEED CODES", PAL_DIM);
    ui_text(0, 2, "THIS PATTERN'S GENERATOR:", PAL_LABEL);
    corr_code_encode(&proj.gen, cur_code);
    ui_text(0, 3, "ETCH ", PAL_DIM);
    fmt_code(cur_code, 12, 5, 3);
    ext_code_encode(&proj.gen, proj.xseed, cur_code);
    ui_text(0, 4, "EXT  ", PAL_DIM);
    fmt_code(cur_code, 16, 5, 4);
    ui_text(0, 6, "ENTER A CODE:", PAL_LABEL);
    for (i = 0; i < code_len; i++) {
        if (i && (i & 3) == 0) ui_put(x++, 7, '-', PAL_DIM);
        ui_put(x++, 7, (uint8_t)code_buf[i], sub == i ? PAL_CURSOR : PAL_VALUE);
    }
    ui_fill(x, 7, (uint8_t)(30 - x), ' ', PAL_TEXT);
    ui_text(0, 9, code_len == 12 ? "12 DIGITS: ETCH (LEGACY)   " : "16 DIGITS: ETCH II EXTENDED", PAL_TEXT);
    ui_text(0, 10, "B+</>: 12 / 16 DIGITS", PAL_DIM);
    ui_text(0, 12, "A+^v:DIGIT  START IS PLAY", PAL_DIM);
    ui_text(0, 13, "A (TAP): IMPORT + GEN ALL", PAL_DIM);
    ui_text(0, 15, "A LEGACY CODE GIVES THE SAME", PAL_DIM);
    ui_text(0, 16, "CORE GROOVE AS ON ETCH.", PAL_DIM);
    ui_text(0, 17, "SOUNDS + EXTRA TRACKS: SAVE", PAL_DIM);
    ui_text(0, 18, "A PROJECT TO KEEP THEM.", PAL_DIM);
}

static const char *const about[4][14] = {
    { "ETCH II", "A Generative Synthesis Machine", "by Synth Centric", "", "Version " ETCH2_VERSION_STR,
      "", "The successor to ETCH, a", "generative acid machine for", "the Game Boy. The same", "composer, CORROSION v2.4,",
      "now plays through five", "software synthesizers on", "the Game Boy Advance.", "" },
    { "GENERATIVE MUSIC", "", "CORROSION composes a bass", "line and drums from a seed.", "In DRIFT or CORRODE it keeps",
      "rewriting them at phrase", "boundaries, so every listen", "differs. A 12-digit ETCH", "seed code gives the same",
      "core groove on both", "machines; only the sound", "changes.", "", "" },
    { "SYNTH CENTRIC", "", "Dan Schwebel, known as", "Synth Centric, is an", "Australian beat maker,",
      "music producer, writer and", "educator.", "", "ETCH and ETCH II are", "Synth Centric releases.", "", "", "", "" },
    { "CREDITS AND LICENCES", "", "Code: MIT licence.", "Music, artwork, names:", "(c) Synth Centric.",
      "CORROSION: from ETCH 2.5,", "MIT, unchanged.", "Startup code: devkitPro", "crtls (MPL-2.0).", "",
      "All sounds are synthesised", "by the GBA. No samples.", "", "B: BACK   </>: PAGE" },
};

static void draw_about(void)
{
    uint8_t i;
    for (i = 0; i < 14; i++) ui_textn(0, (uint8_t)(2 + i), about[page][i], 30, i == 0 ? PAL_VALUE : PAL_TEXT);
    ui_text(24, 18, "PAGE", PAL_DIM);
    ui_num(29, 18, (uint16_t)(page + 1), 1, PAL_DIM);
}

static void draw_diag(void)
{
    uint32_t f = mix_frames ? mix_frames : 1;
    ui_text(0, 2, "MIXER RATE", PAL_LABEL); ui_num(14, 2, (uint16_t)MIX_RATE, 5, PAL_VALUE); ui_text(20, 2, "HZ STEREO", PAL_DIM);
    ui_text(0, 3, "SAMPLES/FRAME", PAL_LABEL); ui_num(14, 3, MIX_SPF, 5, PAL_VALUE);
    ui_text(0, 5, "AUDIO CPU NOW", PAL_LABEL); ui_num(14, 5, (uint16_t)(mix_lines_last * 100 / 228), 3, PAL_VALUE); ui_put(17, 5, '%', PAL_VALUE);
    ui_text(0, 6, "AUDIO CPU AVG", PAL_LABEL); ui_num(14, 6, (uint16_t)(mix_lines_sum / f * 100 / 228), 3, PAL_VALUE); ui_put(17, 6, '%', PAL_VALUE);
    ui_text(0, 7, "AUDIO CPU MAX", PAL_LABEL); ui_num(14, 7, (uint16_t)(mix_lines_max * 100 / 228), 3, PAL_VALUE); ui_put(17, 7, '%', PAL_VALUE);
    ui_text(0, 8, "UNDERRUNS", PAL_LABEL); ui_num(14, 8, mix_overruns, 5, mix_overruns ? PAL_ALERT : PAL_VALUE);
    ui_text(0, 9, "VOICES FREED", PAL_LABEL); ui_num(14, 9, voices_stolen, 5, PAL_VALUE);
    ui_text(0, 10, "LIMITER HITS", PAL_LABEL); ui_num(14, 10, mix_clips, 5, PAL_VALUE);
    ui_text(0, 11, "DRUM RENDERS", PAL_LABEL); ui_num(14, 11, drum_cache_renders, 5, PAL_VALUE);
    ui_text(0, 12, "LIVE DRUM HITS", PAL_LABEL); ui_num(14, 12, drum_rt_hits, 5, PAL_VALUE);
    ui_text(0, 13, "FRAMES MIXED", PAL_LABEL); ui_num(14, 13, (uint16_t)(mix_frames / 60), 5, PAL_VALUE); ui_text(20, 13, "SECONDS", PAL_DIM);
    ui_text(0, 14, "UI WORST PASS", PAL_LABEL); ui_num(14, 14, main_worst, 5, PAL_VALUE); ui_text(20, 14, "FRAMES", PAL_DIM);
    ui_text(0, 15, "EVOLUTIONS", PAL_LABEL); ui_num(14, 15, evo_commits, 5, PAL_VALUE);
    ui_text(0, 16, "STALE REFUSED", PAL_LABEL); ui_num(14, 16, evo_stale_drops, 5, PAL_VALUE);
}

static void redraw(void)
{
    ui_cls();
    ui_header("FILE");
    switch (mode) {
    case M_ALBUM: draw_album(); ui_help("A:LOAD+PLAY  B:BACK"); break;
    case M_LOAD: draw_slots(0); ui_help("A:LOAD  B:BACK"); break;
    case M_SAVE: draw_slots(1); ui_help("A:SAVE  B:BACK"); break;
    case M_NAME: draw_name(); ui_help("B:DONE"); break;
    case M_CODE: draw_code(); ui_help("A:IMPORT  B:BACK"); break;
    case M_ABOUT: draw_about(); ui_help("</>:PAGE  B:BACK"); break;
    case M_DIAG: draw_diag(); ui_help("B:BACK  (LIVE FIGURES)"); break;
    case M_NEW: ui_text(0, 4, "START AN EMPTY PROJECT?", PAL_TEXT); ui_text(0, 6, "A:YES  B:NO", PAL_ALERT); ui_help("UNSAVED CHANGES ARE LOST"); break;
    default: draw_menu(); break;
    }
}

void scr_file_enter(void) { redraw(); }

static void open_mode(uint8_t m)
{
    mode = m; sub = 0; confirm = 0; page = 0;
    if (m == M_ALBUM) sub = proj.album_id < ALBUM_TRACKS ? proj.album_id : 0;
    if (m == M_CODE) { corr_code_encode(&proj.gen, code_buf); ext_code_encode(&proj.gen, proj.xseed, code_buf); }
    redraw();
}

static void code_import(void)
{
    gen_t g = proj.gen;
    uint16_t xs = proj.xseed;
    uint8_t ok = code_len == 12 ? corr_code_decode(&g, code_buf) : ext_code_decode(&g, &xs, code_buf);
    if (!ok) { ui_msg("INVALID CODE"); return; }
    g.mode = proj.gen.mode;
    g.mutation = proj.gen.mutation;
    proj.gen = g;
    proj.xseed = xs;
    app_set_compat(code_len == 12 ? COMPAT_CLASSIC : COMPAT_EXPANDED);
    if (proj.pat[seq_cur].lock & PLOCK_ALL) { ui_msg("PATTERN LOCKED"); return; }
    app_generate(GEN_ALL);
    ui_msg(code_len == 12 ? "IMPORTED: CLASSIC, GEN ALL" : "IMPORTED: EXPANDED, GEN ALL");
}

void scr_file_frame(void)
{
    uint8_t nav = ui_nav();
    int8_t d = ui_edit(1);
    if (mode == 0xFF) {
        if ((nav & 4) && cur) { cur--; draw_menu(); }
        if ((nav & 8) && cur + 1 < M_COUNT) { cur++; draw_menu(); }
        if (ui_a_tap()) open_mode(cur);
        return;
    }
    if (ui_b_tap()) {
        if (confirm) { confirm = 0; redraw(); }
        else { mode = 0xFF; redraw(); }
        return;
    }
    switch (mode) {
    case M_ALBUM:
        if ((nav & 4) && sub) { sub--; redraw(); }
        if ((nav & 8) && sub + 1 < ALBUM_TRACKS) { sub++; redraw(); }
        if (ui_a_tap()) {
            app_load_album(sub);
            if (!seq_playing) { seq_seed_live(ui_entropy); seq_start(); }
            app_go(SCR_LIVE);
        }
        break;
    case M_LOAD: case M_SAVE:
        if (!confirm) {
            if ((nav & 4) && sub) { sub--; redraw(); }
            if ((nav & 8) && sub + 1 < SAVE_SLOTS) { sub++; redraw(); }
        }
        if (ui_a_tap()) {
            uint8_t st = storage_slot_status(sub, 0);
            if (mode == M_SAVE) {
                if (st == SLOT_OK && !confirm) { confirm = 1; redraw(); break; }
                confirm = 0;
                ui_msg(storage_save(sub) ? "SAVED AND VERIFIED" : "SAVE FAILED: CHECK THE CART");
                redraw();
            } else {
                if (st != SLOT_OK) { ui_msg(st == SLOT_EMPTY ? "SLOT IS EMPTY" : "SLOT IS CORRUPT: NOT LOADED"); break; }
                if (!confirm) { confirm = 1; redraw(); break; }
                confirm = 0;
                {
                    uint8_t was = seq_playing;
                    seq_stop();
                    CRITICAL { audio_all_off(); }
                    if (storage_load(sub)) {
                        app_project_loaded();
                        ui_msg("LOADED");
                        if (was) { seq_seed_live(ui_entropy); seq_start(); }
                        app_go(SCR_LIVE);
                    } else ui_msg("LOAD FAILED: DATA CORRUPT");
                }
            }
        }
        break;
    case M_NEW:
        if (ui_a_tap()) { app_new_project(); app_go(SCR_LIVE); ui_msg("NEW PROJECT"); }
        break;
    case M_NAME:
        if ((nav & 1) && sub + 1 < E2_NAME_LEN) { sub++; draw_name(); }
        if ((nav & 2) && sub) { sub--; draw_name(); }
        if (d) {
            static const char cs[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.";
            uint8_t i = 0, n = (uint8_t)(sizeof cs - 1);
            while (i < n && cs[i] != proj.name[sub]) i++;
            if (i >= n) i = 0;
            proj.name[sub] = cs[(i + n + (d > 0 ? 1 : -1)) % n];
            proj.album_id = 0xFF;
            draw_name();
        }
        break;
    case M_CODE:
        {
            uint8_t bd = ui_b_dir();
            if (!(bd & 3)) goto no_len;
            code_len = (bd & 1) ? 16 : 12;
            if (sub >= code_len) sub = (uint8_t)(code_len - 1);
            redraw();
            break;
        }
no_len:
        if ((nav & 1) && sub + 1 < code_len) { sub++; draw_code(); }
        if ((nav & 2) && sub) { sub--; draw_code(); }
        if (d) {
            char c = code_buf[sub];
            int8_t v = (int8_t)(c >= 'A' ? c - 'A' + 10 : c - '0');
            v = (int8_t)((v + (d > 0 ? 1 : 15)) & 15);
            code_buf[sub] = (char)(v < 10 ? '0' + v : 'A' + v - 10);
            draw_code();
        }
        if (ui_a_tap()) { code_import(); draw_code(); }
        break;
    case M_ABOUT:
        if ((nav & 1) && page < 3) { page++; redraw(); }
        if ((nav & 2) && page) { page--; redraw(); }
        break;
    case M_DIAG:
        if ((frame_count & 15) == 0) draw_diag();
        break;
    }
}
