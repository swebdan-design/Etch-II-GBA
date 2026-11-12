/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * User interface core and screens.
 *
 * Mode 0, BG0 = 30 x 20 tiles of 8 x 8 (text + glyphs), 16 palettes.
 * Editing grammar (ETCH's, on every screen):
 *   D-pad            move the cursor
 *   A (tap)          place / toggle / do
 *   A held + < >     value -1 / +1
 *   A held + ^ v     value +coarse / -coarse
 *   B + A            delete / reset to default
 *   B (tap)          back / cancel
 */
#ifndef UI_H
#define UI_H
#include "etch2.h"
#include "gfx.h"

#define SCR_W 30
#define SCR_H 20

enum { SCR_LIVE, SCR_TRACK, SCR_SYNTH, SCR_MOD, SCR_GEN, SCR_MEMORY, SCR_SONG, SCR_FILE, SCR_COUNT };
extern const char * const scr_names[SCR_COUNT];

/* drawing (direct to VRAM; the GBA allows it at any time) */
void ui_init(void);
void ui_cls(void);
void ui_put(uint8_t x, uint8_t y, uint16_t tile, uint8_t pal);
void ui_text(uint8_t x, uint8_t y, const char *s, uint8_t pal);
void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal);   /* pad/clip */
void ui_fill(uint8_t x, uint8_t y, uint8_t w, uint16_t tile, uint8_t pal);
void ui_num(uint8_t x, uint8_t y, uint16_t v, uint8_t digits, uint8_t pal);    /* space padded */
void ui_num0(uint8_t x, uint8_t y, uint16_t v, uint8_t digits, uint8_t pal);   /* zero padded */
void ui_hex(uint8_t x, uint8_t y, uint16_t v, uint8_t digits, uint8_t pal);
void ui_note(uint8_t x, uint8_t y, uint8_t midi, uint8_t pal);                  /* "C#4" */
void ui_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max, uint8_t pal);
void ui_recolor(uint8_t x, uint8_t y, uint8_t w, uint8_t pal);
void ui_header(const char *screen);
void ui_header_update(void);
void ui_help(const char *s);
void ui_msg(const char *s);                 /* transient message on the help row */
void ui_msg_tick(void);
uint16_t ui_screen_tile(uint8_t x, uint8_t y);   /* tests read the map */
extern char ui_textbuf[SCR_H][SCR_W + 1];   /* mirror of the text on screen (tests) */

/* input */
extern uint16_t key_held, key_pressed, key_released, key_repeat;
void ui_input(void);
uint8_t ui_nav(void);                       /* D-pad (no A/B held): bit per direction pressed/repeat */
int8_t  ui_edit(uint8_t coarse);            /* A held + D-pad: delta */
uint8_t ui_a_tap(void);                     /* A released without an edit */
uint8_t ui_clear(void);                     /* B + A */
uint8_t ui_b_tap(void);                     /* B released on its own */
uint8_t ui_b_dir(void);                     /* B + D-pad: direction bits */
void    ui_consume(void);

/* app services */
extern uint8_t app_screen, app_track;
extern uint8_t app_need_redraw;
void app_redraw(void);
void app_go(uint8_t screen);
uint8_t clampadd(uint8_t v, int8_t d, uint8_t lo, uint8_t hi);
void app_load_album(uint8_t i);
void app_new_project(void);
void app_set_compat(uint8_t c);

/* screens */
void scr_title_run(void);
void scr_live_enter(void);   void scr_live_frame(void);
void scr_track_enter(void);  void scr_track_frame(void);
void scr_synth_enter(void);  void scr_synth_frame(void);
void scr_mod_enter(void);    void scr_mod_frame(void);
void scr_gen_enter(void);    void scr_gen_frame(void);
void scr_memory_enter(void); void scr_memory_frame(void);
void scr_song_enter(void);   void scr_song_frame(void);
void scr_file_enter(void);   void scr_file_frame(void);

#endif
