/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Boot, interrupt dispatch, main loop and global controls.
 *
 * Division of labour:
 *   VBlank IRQ (ARM, IWRAM): DMA restart, sequencer, synthesis, mixing.
 *   Main loop (Thumb, ROM):  input, screens, CORROSION candidate
 *                            preparation, REGEN material, saving/loading.
 * The interrupt never waits for the main loop; the main loop hands musical
 * data over in short critical sections (as ETCH's timer ISR / main loop).
 */
#include "gba.h"
#include "audio.h"
#include "seq.h"
#include "ui.h"
#include "evolve.h"
#include "storage.h"
#include "album.h"

project2_t proj EWRAM_BSS;
uint8_t app_screen = SCR_LIVE, app_track;
uint8_t app_need_redraw = 1;
volatile uint32_t frame_count;
volatile uint16_t main_worst;              /* longest main-loop pass, frames */
char *fake_heap_end, *fake_heap_start;     /* referenced by devkitPro's crt0 */
static uint8_t last_commits;
volatile uint8_t probe_album;              /* test hook: 1..8 loads that album piece */

void irq_entry(void);
void irq_dispatch(uint32_t flags) IWRAM_CODE;

/* called from irq.s in SYS mode with IRQs masked */
void irq_dispatch(uint32_t flags)
{
    if (flags & IRQ_VBLANK) {
        audio_vblank();
        frame_count++;
    }
}

void app_redraw(void) { app_need_redraw = 1; }
void app_go(uint8_t s) { app_screen = (uint8_t)(s % SCR_COUNT); app_need_redraw = 1; }

static void enter_screen(void)
{
    switch (app_screen) {
    case SCR_LIVE:   scr_live_enter(); break;
    case SCR_TRACK:  scr_track_enter(); break;
    case SCR_SYNTH:  scr_synth_enter(); break;
    case SCR_MOD:    scr_mod_enter(); break;
    case SCR_GEN:    scr_gen_enter(); break;
    case SCR_MEMORY: scr_memory_enter(); break;
    case SCR_SONG:   scr_song_enter(); break;
    default:         scr_file_enter(); break;
    }
}

static void frame_screen(void)
{
    switch (app_screen) {
    case SCR_LIVE:   scr_live_frame(); break;
    case SCR_TRACK:  scr_track_frame(); break;
    case SCR_SYNTH:  scr_synth_frame(); break;
    case SCR_MOD:    scr_mod_frame(); break;
    case SCR_GEN:    scr_gen_frame(); break;
    case SCR_MEMORY: scr_memory_frame(); break;
    case SCR_SONG:   scr_song_frame(); break;
    default:         scr_file_frame(); break;
    }
}

void app_set_compat(uint8_t c)
{
    proj.compat = c ? COMPAT_EXPANDED : COMPAT_CLASSIC;
    if (proj.compat == COMPAT_CLASSIC) {
        uint8_t v;
        CRITICAL { for (v = V_LEAD; v < N_MELODIC; v++) voice_gate_off(v); }
    }
}

void app_load_album(uint8_t i)
{
    uint8_t was = seq_playing;
    seq_stop();
    CRITICAL { audio_all_off(); }
    album_load(i, &proj);
    app_project_loaded();
    if (was) { seq_seed_live(ui_entropy); seq_start(); }
    app_redraw();
}

void app_new_project(void)
{
    seq_stop();
    CRITICAL { audio_all_off(); }
    project_defaults(&proj);
    proj.gen.seed = (uint16_t)evo_random();
    app_project_loaded();
    app_redraw();
}

/* ---- global keys -------------------------------------------------------------- */
static void global_keys(void)
{
    if (key_held & KEY_SELECT) {
        if (key_pressed & KEY_RIGHT) app_go((uint8_t)(app_screen + 1));
        if (key_pressed & KEY_LEFT) app_go((uint8_t)(app_screen + SCR_COUNT - 1));
        if (key_pressed & (KEY_UP | KEY_DOWN)) {
            uint8_t base = seq_queued != 0xFF ? seq_queued : seq_cur;
            int8_t d = (key_pressed & KEY_UP) ? 1 : -1;
            seq_queue_pattern((uint8_t)((base + E2_PATTERNS + d) % E2_PATTERNS));
            if (proj.song.song) { proj.song.song = 0; ui_msg("LOOP MODE: PATTERN QUEUED"); }
            if (!seq_playing) app_redraw();
        }
        if (key_pressed & KEY_A) {
            if (proj.pat[seq_cur].lock & PLOCK_ALL) ui_msg("PATTERN LOCKED");
            else { app_evolve_now(); ui_msg(seq_playing ? "EVOLVE AT NEXT BAR" : "EVOLVED"); app_redraw(); }
        }
        if (key_pressed & KEY_B) { ui_msg(app_undo() ? "UNDO: PREVIOUS GENERATION" : "NOTHING TO UNDO"); app_redraw(); }
        if (key_pressed & KEY_START) { app_fill_toggle(); ui_msg(seq_fill_hold ? "FILL ON (SELECT+START: OFF)" : "FILL OFF"); }
        if (key_pressed & KEY_R) { app_track = (uint8_t)((app_track + 1) % TRK_COUNT); app_redraw(); }
        if (key_pressed & KEY_L) { app_track = (uint8_t)((app_track + TRK_COUNT - 1) % TRK_COUNT); app_redraw(); }
        ui_consume();
    } else if (key_held & KEY_B) {
        if (key_pressed & KEY_R) { app_track = (uint8_t)((app_track + 1) % TRK_COUNT); app_redraw(); }
        if (key_pressed & KEY_L) { app_track = (uint8_t)((app_track + TRK_COUNT - 1) % TRK_COUNT); app_redraw(); }
        if (key_pressed & (KEY_L | KEY_R)) ui_consume();
    } else {
        if (key_pressed & KEY_R) app_go((uint8_t)(app_screen + 1));
        if (key_pressed & KEY_L) app_go((uint8_t)(app_screen + SCR_COUNT - 1));
        if (key_pressed & KEY_START) {
            if (seq_playing) seq_stop();
            else { seq_seed_live(ui_entropy); seq_start(); }
        }
    }
}

int main(void)
{
    REG_IME = 0;
    REG_WAITCNT = 0x4317;                    /* SRAM 8, ROM 3/1, prefetch on */
    REG_DISPCNT = DCNT_BLANK;
    ui_init();
    evo_init();
    project_defaults(&proj);
    album_load(0, &proj);
    voice_init_all();
    drums_init();
    mixer_init();
    seq_init();
    app_project_loaded();
    storage_init();

    BIOS_IRQ_VECTOR = (uint32_t)irq_entry;
    REG_DISPSTAT = DSTAT_VBL_IRQ;
    REG_IE = IRQ_VBLANK;
    REG_IF = 0xFFFF;
    REG_IME = 1;
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;

    scr_title_run();                          /* returns when START is pressed */

    seq_seed_live(ui_entropy);                /* the moment START was pressed */
    app_screen = SCR_LIVE;
    app_need_redraw = 1;
    seq_start();                              /* press START, hear music */
    last_commits = evo_commits;

    for (;;) {
        uint32_t f0;
        vblank_wait_bios();
        f0 = frame_count;
        if (probe_album) { app_load_album((uint8_t)(probe_album - 1)); probe_album = 0; }
        ui_input();
        global_keys();
        if (app_need_redraw) {
            app_need_redraw = 0;
            ui_cls();
            enter_screen();
        } else {
            frame_screen();
        }
        ui_header_update();
        ui_msg_tick();
        app_fill_service();
        if (evo_commits != last_commits) {
            last_commits = evo_commits;
            evo_after_commit();
            if (app_screen == SCR_LIVE || app_screen == SCR_MEMORY || app_screen == SCR_TRACK) {
                static char m[] = "EVOLVED  GEN 000";
                uint8_t g = proj.pat[seq_cur].gen;
                m[13] = (char)('0' + g / 100); m[14] = (char)('0' + (g / 10) % 10); m[15] = (char)('0' + g % 10);
                ui_msg(m);
            }
        }
        evo_stable_reset();
        if (!evo_service()) regen_service();     /* one heavy job per pass at most */
        drum_cache_service();                    /* keep the percussion renders fresh */
        f0 = frame_count - f0;
        if (f0 > main_worst) main_worst = (uint16_t)f0;
    }
}
