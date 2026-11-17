/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * FILE: the album, load / save (verified), new project, project name, seed
 * codes (import / export), about, and diagnostics.  ^v pick, A open / do,
 * B back.
 */
#include <string.h>
#include "ui.h"
#include "canvas.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"
#include "storage.h"
#include "album.h"
#include "expand.h"

enum { M_ALBUM, M_LOAD, M_SAVE, M_NEW, M_NAME, M_CODE, M_ABOUT, M_DIAG, M_COUNT };
static const char *const menu_n[M_COUNT][2] = {
    { "ALBUM", "THE 8 LIVING PIECES" }, { "LOAD", "A SAVED PROJECT" }, { "SAVE", "THIS PROJECT" },
    { "NEW", "AN EMPTY PROJECT" }, { "NAME", "RENAME THE PROJECT" }, { "SEED CODE", "IMPORT / EXPORT" },
    { "ABOUT", "ETCH II, SYNTH CENTRIC, CREDITS" }, { "DIAGNOSTICS", "THE ENGINE, LIVE" }
};
static uint8_t mode = 0xFF, cur, sub, confirm, page;
static char code_buf[16];
static uint8_t code_len = 12;
extern volatile uint16_t main_worst;
extern volatile uint32_t frame_count;

static void name_str(char *nm)
{
    int i;
    memcpy(nm, proj.name, E2_NAME_LEN); nm[E2_NAME_LEN] = 0;
    for (i = E2_NAME_LEN - 1; i >= 0 && (nm[i] == ' ' || !nm[i]); i--) nm[i] = 0;
}

static void top(const char *cap, const char *val)
{
    int x;
    cv_rect(0, 27, CV_W, 10, CV_BG);
    x = cv_text(6, 29, F_SMALL, CV_TEXT2, cap);
    if (val) cv_text(x + 4, 28, F_MAIN, CV_TEXT, val);
}

static void draw_menu(void)
{
    uint8_t i;
    char nm[E2_NAME_LEN + 1];
    name_str(nm);
    top("PROJECT", nm);
    for (i = 0; i < M_COUNT; i++) {
        int y = 40 + i * 13;
        cv_rect(0, y - 2, CV_W, 12, cur == i ? CV_SURF : CV_BG);
        cv_rect(2, y - 1, 2, 9, cur == i ? CV_ACCENT : (cur == i ? CV_SURF : CV_BG));
        cv_text(8, y, F_MAIN, CV_TEXT, menu_n[i][0]);
        cv_text(80, y + 1, F_SMALL, CV_TEXT2, menu_n[i][1]);
    }
    ui_help("A OPEN  B BACK");
}

static void draw_album(void)
{
    uint8_t i;
    top("ALBUM   A LOADS AND PLAYS", 0);
    cv_text_r(234, 28, F_MAIN, CV_TEXT, album[sub].code);
    cv_text_r(230 - cv_text_w(F_MAIN, album[sub].code), 29, F_SMALL, CV_TEXT2, "CORE SEED");
    for (i = 0; i < ALBUM_TRACKS; i++) {
        int y = 39 + i * 14;
        cv_rect(0, y - 2, CV_W, 14, sub == i ? CV_SURF : CV_BG);
        if (sub == i) cv_rect(2, y - 1, 2, 12, CV_ACCENT);
        cv_num(8, y, F_SMALL, CV_TEXT3, (uint16_t)(i + 1), 0, 0);
        cv_text(16, y, F_MAIN, CV_TEXT, album[i].title);
        if (proj.album_id == i) cv_rect(70, y + 2, 3, 3, CV_ACCENT);
        cv_text(78, y, F_SMALL, CV_TEXT, album[i].line1);
        cv_text(78, y + 6, F_SMALL, CV_TEXT2, album[i].line2);
    }
}

static void draw_slots(uint8_t saving)
{
    uint8_t s;
    top(saving ? "SAVE TO A SLOT" : "LOAD FROM A SLOT", 0);
    for (s = 0; s < SAVE_SLOTS; s++) {
        char nm[E2_NAME_LEN + 1];
        uint8_t st = storage_slot_status(s, nm);
        int y = 42 + s * 16;
        nm[E2_NAME_LEN] = 0;
        cv_rect(0, y - 3, CV_W, 15, sub == s ? CV_SURF : CV_BG);
        if (sub == s) cv_rect(2, y - 2, 2, 11, CV_ACCENT);
        cv_text(8, y + 1, F_SMALL, CV_TEXT2, "SLOT");
        cv_num(28, y, F_MAIN, CV_TEXT, (uint16_t)(s + 1), 0, 0);
        if (st == SLOT_OK) cv_text(44, y, F_MAIN, CV_TEXT, nm);
        else if (st == SLOT_EMPTY) cv_text(44, y, F_MAIN, CV_TEXT3, "EMPTY");
        else { cv_rect(42, y - 2, 70, 11, CV_ACC_TINT); cv_text(44, y, F_MAIN, CV_TEXT, "CORRUPT"); }
    }
    cv_text(8, 110, F_SMALL, CV_TEXT3, "4 SLOTS OF 8 KB, CRC-32 VERIFIED ON SAVE AND LOAD");
    cv_rect(0, 120, CV_W, 14, CV_BG);
    if (confirm) {
        cv_rect(6, 120, 228, 13, CV_ACCENT);
        cv_text_c(120, 123, F_MAIN, CV_TEXT, saving ? "REPLACE THIS SLOT?  A YES   B NO" : "LOAD THIS SLOT?  A YES   B NO");
    }
}

static void draw_name(void)
{
    uint8_t i;
    top("PROJECT NAME", 0);
    cv_rect(0, 40, CV_W, 30, CV_BG);
    for (i = 0; i < E2_NAME_LEN; i++) {
        char c[2] = { proj.name[i], 0 };
        int x = 30 + i * 18;
        cv_rrect(x, 44, 15, 19, sub == i ? CV_ACC_TINT : CV_SURF);
        if (sub == i) cv_rect(x + 1, 64, 13, 2, CV_ACCENT);
        cv_text_c(x + 8, 50, F_MAIN, CV_TEXT, c);
    }
    cv_text(30, 76, F_SMALL, CV_TEXT2, "</> MOVE    A+^v CHANGE THE LETTER    B DONE");
}

static int fmt_code(const char *c, uint8_t n, int x, int y, int sel)
{
    uint8_t i;
    char one[2] = { 0, 0 };
    for (i = 0; i < n; i++) {
        if (i && (i & 3) == 0) x = cv_text(x, y, F_MAIN, CV_TEXT3, "-") + 1;
        one[0] = c[i];
        if (sel == i) cv_rect(x - 1, y + 8, cv_text_w(F_MAIN, one) + 2, 2, CV_ACCENT);
        x = cv_text(x, y, F_MAIN, CV_TEXT, one) + 1;
    }
    return x;
}

static void draw_code(void)
{
    char cur_code[16];
    top("SEED CODES", 0);
    cv_rect(0, 38, CV_W, 112, CV_BG);
    cv_text(8, 40, F_SMALL, CV_TEXT2, "THIS PATTERN'S GENERATOR");
    corr_code_encode(&proj.gen, cur_code);
    cv_text(8, 50, F_SMALL, CV_TEXT2, "ETCH");
    fmt_code(cur_code, 12, 44, 49, -1);
    ext_code_encode(&proj.gen, proj.xseed, cur_code);
    cv_text(8, 60, F_SMALL, CV_TEXT2, "EXTENDED");
    fmt_code(cur_code, 16, 44, 59, -1);
    cv_hline(6, 72, 228, CV_BORDER);
    cv_text(8, 78, F_SMALL, CV_TEXT2, "ENTER A CODE");
    cv_rrect(40, 86, 160, 16, CV_SURF);
    fmt_code(code_buf, code_len, 46, 90, sub);
    cv_text(8, 108, F_SMALL, CV_TEXT, code_len == 12 ? "12 DIGITS: AN ETCH CODE (CLASSIC)" : "16 DIGITS: AN ETCH II EXTENDED CODE");
    cv_text(8, 116, F_SMALL, CV_TEXT2, "B+</> 12 OR 16 DIGITS    A+^v DIGIT    A IMPORT + GEN ALL");
    cv_text(8, 128, F_SMALL, CV_TEXT3, "THE FIRST 12 DIGITS MAKE THE SAME GROOVE ON EVERY ETCH.");
    cv_text(8, 135, F_SMALL, CV_TEXT3, "SOUNDS AND EXTRA TRACKS: SAVE A PROJECT TO KEEP THEM.");
}

static const char *const about[4][14] = {
    { "ETCH II", "A Generative Synthesis Machine", "by Synth Centric", "", "Version " ETCH2_VERSION_STR,
      "", "A generative music machine", "for the Game Boy Advance.", "Its composer, CORROSION,", "writes the music; five",
      "software synthesizers play", "it, live, on the console.", "", "" },
    { "GENERATIVE MUSIC", "", "CORROSION composes a bass", "line and drums from a seed.", "In DRIFT or CORRODE it keeps",
      "rewriting them at phrase", "boundaries, so every listen", "differs. A 12-digit seed", "code always makes the same",
      "core groove: share the", "code to share the song.", "", "", "" },
    { "SYNTH CENTRIC", "", "Dan Schwebel, known as", "Synth Centric, is an", "Australian beat maker,",
      "music producer, writer and", "educator.", "", "ETCH II is a Synth", "Centric release.", "", "", "", "" },
    { "CREDITS AND LICENCES", "", "Code: MIT licence.", "Music, artwork, names:", "(c) Synth Centric.",
      "CORROSION: MIT licence.", "", "Startup code: devkitPro", "crtls (MPL-2.0).", "",
      "All sounds are synthesised", "by the GBA. No samples.", "", "" },
};

static void draw_about(void)
{
    uint8_t i;
    char b[8];
    top("ABOUT", 0);
    cv_text(8, 42, F_MAIN, CV_TEXT, about[page][0]);
    cv_hline(8, 52, 120, CV_BORDER);
    for (i = 1; i < 14; i++) if (about[page][i][0]) cv_text(8, 50 + i * 7, F_SMALL, CV_TEXT, about[page][i]);
    for (i = 0; i < 4; i++) cv_rect(212 + i * 6, 30, 4, 4, i == page ? CV_ACCENT : CV_SURF3);
    b[0] = 0; (void)b;
}

static void diag_row(int col, int r, const char *cap, uint32_t v, const char *unit, uint8_t warn)
{
    int x = 8 + col * 116, y = 40 + r * 13;
    char b[12];
    cv_rect(x, y, 112, 12, CV_BG);
    cv_text(x, y, F_SMALL, CV_TEXT2, cap);
    cv_numfmt(b, (uint16_t)(v > 65535 ? 65535 : v), 0, 0);
    if (warn) cv_rect(x - 2, y + 5, cv_text_w(F_MAIN, b) + 4, 8, CV_ACC_TINT);
    x = cv_text(x, y + 6, F_MAIN, CV_TEXT, b);
    if (unit) cv_text(x + 3, y + 7, F_SMALL, CV_TEXT3, unit);
}

static void draw_diag(void)
{
    uint32_t f = mix_frames ? mix_frames : 1;
    top("DIAGNOSTICS", 0);
    diag_row(0, 0, "MIXER RATE", MIX_RATE, "HZ STEREO", 0);
    diag_row(0, 1, "SAMPLES PER FRAME", MIX_SPF, 0, 0);
    diag_row(0, 2, "AUDIO CPU NOW", mix_lines_last * 100 / 228, "%", 0);
    diag_row(0, 3, "AUDIO CPU AVERAGE", mix_lines_sum / f * 100 / 228, "%", 0);
    diag_row(0, 4, "AUDIO CPU MAXIMUM", mix_lines_max * 100 / 228, "%", 0);
    diag_row(0, 5, "UNDERRUNS", mix_overruns, 0, mix_overruns != 0);
    diag_row(0, 6, "VOICES FREED", voices_stolen, 0, 0);
    diag_row(1, 0, "LIMITER HITS", mix_clips, 0, 0);
    diag_row(1, 1, "DRUM RENDERS", drum_cache_renders, 0, 0);
    diag_row(1, 2, "LIVE DRUM HITS", drum_rt_hits, 0, 0);
    diag_row(1, 3, "TIME MIXED", mix_frames / 60, "SECONDS", 0);
    diag_row(1, 4, "UI WORST PASS", main_worst, "FRAMES", 0);
    diag_row(1, 5, "EVOLUTIONS", evo_commits, 0, 0);
    diag_row(1, 6, "STALE REFUSED", evo_stale_drops, 0, 0);
}

static void redraw(void)
{
    ui_cls();
    ui_header("FILE");
    switch (mode) {
    case M_ALBUM: draw_album(); ui_help("A LOAD AND PLAY  B BACK"); break;
    case M_LOAD: draw_slots(0); ui_help("A LOAD  B BACK"); break;
    case M_SAVE: draw_slots(1); ui_help("A SAVE  B BACK"); break;
    case M_NAME: draw_name(); ui_help("B DONE"); break;
    case M_CODE: draw_code(); ui_help("A IMPORT  B BACK"); break;
    case M_ABOUT: draw_about(); ui_help("</> PAGE  B BACK"); break;
    case M_DIAG: draw_diag(); ui_help("B BACK  (LIVE FIGURES)"); break;
    case M_NEW:
        top("NEW PROJECT", 0);
        cv_text(8, 44, F_MAIN, CV_TEXT, "START AN EMPTY PROJECT?");
        cv_button(8, 60, 60, 13, "A YES", 0, 1);
        cv_button(74, 60, 60, 13, "B NO", 0, 0);
        ui_help("UNSAVED CHANGES ARE LOST");
        break;
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
        if (ui_a_tap()) { code_import(); cv_header(); draw_code(); }
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
