/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * UI core: tile text drawing, header/help rows, and ETCH's input grammar.
 */
#include "ui.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"

#include "canvas.h"

const char * const scr_names[SCR_COUNT] = { "LIVE", "TRACK", "SYNTH", "MOD", "GEN", "MEMORY", "SONG", "FILE" };
char ui_textbuf[SCR_H][SCR_W + 1] EWRAM_BSS;

uint16_t key_held, key_pressed, key_released, key_repeat;
static uint16_t prev_keys;
static uint8_t rep_timer;
static uint8_t a_edited, b_used;
static uint8_t msg_timer;
static char help_text[64];

/*
 * The cell layer: ETCH II 1.0's 30 x 20 character API, drawn onto the canvas
 * in the series colours and ETCH DS's MAIN font.  Screens not yet laid out
 * in pixels use it; it keeps every screen working while they move over.
 */
static uint16_t cell_tile[SCR_H][SCR_W] EWRAM_BSS;
static uint8_t cell_pal[SCR_H][SCR_W] EWRAM_BSS;

/* ETCH II 1.0 palettes -> series roles: { colour 0, 1 (ink), 2 (paper), 3 (detail) } */
static const uint8_t pal_map[16][4] = {
    { CV_BG, CV_TEXT,   CV_BG,       CV_ACCENT },   /* TEXT        */
    { CV_BG, CV_TEXT2,  CV_BG,       CV_ACCENT },   /* LABEL       */
    { CV_BG, CV_TEXT,   CV_BG,       CV_ACCENT },   /* VALUE       */
    { CV_BG, CV_TEXT,   CV_ACCENT,   CV_TEXT },     /* CURSOR: lit */
    { CV_BG, CV_TEXT3,  CV_BG,       CV_TEXT3 },    /* DIM         */
    { CV_BG, CV_ACCENT, CV_BG,       CV_ACCENT },   /* PLAY        */
    { CV_BG, CV_TEXT,   CV_BG,       CV_ACCENT },   /* HEAD        */
    { CV_BG, CV_TEXT,   CV_ACC_TINT, CV_ACCENT },   /* ALERT       */
    { CV_BG, CV_TEXT,   CV_BG,       CV_ACCENT },   /* LOGO        */
    { CV_BG, CV_TEXT,   CV_ACC_TINT, CV_ACCENT },   /* PLAYHEAD    */
    { CV_BG, CV_TEXT,   CV_SURF,     CV_ACCENT },   /* PANEL       */
    { CV_BG, CV_TEXT2,  CV_SURF,     CV_ACCENT },   /* PANEL_LABEL */
    { CV_BG, CV_SURF3,  CV_BG,       CV_SURF3 },    /* RUST1       */
    { CV_BG, CV_TEXT3,  CV_BG,       CV_TEXT3 },    /* TARNISH     */
    { CV_BG, CV_SCOPE,  CV_BG,       CV_TEXT3 },    /* SCOPE       */
    { CV_BG, CV_BG,     CV_TEXT,     CV_TEXT },     /* INV         */
};

static void cell_draw(uint8_t x, uint8_t y)
{
    uint16_t t = cell_tile[y][x];
    const uint8_t *m = pal_map[cell_pal[y][x] & 15];
    if (t < 128) {
        char s[2] = { (char)t, 0 };
        cv_rect(x * 8, y * 8, 8, 8, m[2]);
        if (t > ' ') cv_text(x * 8 + 1, y * 8, F_MAIN, m[1], s);
        ui_textbuf[y][x] = (char)t;
    } else if (t < 256) {
        cv_blit_tile(x, y, &gfx_tiles[t * 8], m);
        ui_textbuf[y][x] = '#';
    }
}

void ui_init(void)
{
    cv_init();
    ui_cls();
}

void ui_cls(void)
{
    uint32_t i;
    cv_clear(CV_BG);
    for (i = 0; i < SCR_H; i++) {
        memset(ui_textbuf[i], ' ', SCR_W); ui_textbuf[i][SCR_W] = 0;
        memset(cell_tile[i], 0, sizeof cell_tile[i]);
    }
}

void ui_put(uint8_t x, uint8_t y, uint16_t tile, uint8_t pal)
{
    if (x >= SCR_W || y >= SCR_H) return;
    if (cell_tile[y][x] == tile && cell_pal[y][x] == pal) return;
    cell_tile[y][x] = tile;
    cell_pal[y][x] = pal;
    cell_draw(x, y);
}

uint16_t ui_screen_tile(uint8_t x, uint8_t y) { return (uint16_t)(cell_tile[y][x] | (cell_pal[y][x] << 12)); }

void ui_recolor(uint8_t x, uint8_t y, uint8_t w, uint8_t pal)
{
    while (w-- && x < SCR_W) { ui_put(x, y, cell_tile[y][x], pal); x++; }
}

void ui_text(uint8_t x, uint8_t y, const char *s, uint8_t pal)
{
    while (*s && x < SCR_W) ui_put(x++, y, (uint8_t)*s++, pal);
}

void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal)
{
    while (n--) ui_put(x++, y, (uint8_t)(*s ? *s++ : ' '), pal);
}

void ui_fill(uint8_t x, uint8_t y, uint8_t w, uint16_t tile, uint8_t pal)
{
    while (w--) ui_put(x++, y, tile, pal);
}

void ui_num(uint8_t x, uint8_t y, uint16_t v, uint8_t digits, uint8_t pal)
{
    char b[8];
    cv_numfmt(b, v, digits, 0);
    ui_text(x, y, b, pal);
}

void ui_num0(uint8_t x, uint8_t y, uint16_t v, uint8_t digits, uint8_t pal)
{
    char b[8];
    cv_numfmt(b, v, digits, 1);
    ui_text(x, y, b, pal);
}

void ui_hex(uint8_t x, uint8_t y, uint16_t v, uint8_t digits, uint8_t pal)
{
    static const char hx[] = "0123456789ABCDEF";
    int8_t i;
    for (i = (int8_t)(digits - 1); i >= 0; i--) { ui_put((uint8_t)(x + i), y, (uint8_t)hx[v & 15], pal); v >>= 4; }
}

void ui_note(uint8_t x, uint8_t y, uint8_t midi, uint8_t pal)
{
    static const char nn[12][3] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
    ui_put(x, y, (uint8_t)nn[midi % 12][0], pal);
    ui_put((uint8_t)(x + 1), y, (uint8_t)nn[midi % 12][1], pal);
    ui_put((uint8_t)(x + 2), y, (uint8_t)('0' + (midi / 12 - 1) % 10), pal);
}

/* a slider in the spec's style: 1 px track, fill, thumb */
void ui_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max, uint8_t pal)
{
    int px = x * 8, py = y * 8, W = w * 8 - 2, f = max ? (int)((uint32_t)v * W / max) : 0;
    uint8_t i;
    for (i = 0; i < w; i++) { cell_tile[y][x + i] = 0xFFFF; ui_textbuf[y][x + i] = '#'; }
    cv_rect(px, py, w * 8, 8, CV_BG);
    cv_hline(px, py + 4, W, CV_SURF3);
    cv_rect(px, py + 3, f, 3, pal == PAL_ALERT ? CV_ACCENT : CV_NOTE);
    cv_rect(px + f, py + 1, 2, 7, pal == PAL_CURSOR || pal == PAL_ALERT ? CV_ACCENT : CV_TEXT);
}

/* ---- header + screen bar (rows 0..2) and hint line (row 19) ------------------- */
void ui_header(const char *screen)
{
    uint8_t x, y;
    (void)screen;
    for (y = 0; y < 3; y++) for (x = 0; x < SCR_W; x++) { cell_tile[y][x] = 0xFFFF; ui_textbuf[y][x] = ' '; }
    cv_header();
    cv_tabs(app_screen);
    ui_header_update();
}

void ui_header_update(void)
{
    static uint8_t last = 0xFF;
    uint8_t st = (uint8_t)((seq_playing ? 1 : 0) | (seq_fill ? 2 : 0) | (((seq_step >> 2) & 3) << 2));
    if (st == last && cell_tile[0][29] == 0xFFFF) return;
    last = st;
    /* play indicator in the header's right margin: accent triangle while playing */
    cv_rect(235, 2, 5, 7, CV_BG);
    if (seq_playing) { cv_vline(236, 2, 7, CV_ACCENT); cv_vline(237, 3, 5, CV_ACCENT); cv_vline(238, 4, 3, CV_ACCENT); cv_point(239, 5, CV_ACCENT); }
}

static void hint_draw(void)
{
    uint8_t x;
    for (x = 0; x < SCR_W; x++) { cell_tile[SCR_H - 1][x] = 0xFFFF; ui_textbuf[SCR_H - 1][x] = ' '; }
    cv_hint(help_text, 0);
}

void ui_help(const char *s)
{
    uint8_t i;
    for (i = 0; i < sizeof help_text - 1 && s[i]; i++) help_text[i] = s[i];
    help_text[i] = 0;
    if (!msg_timer) hint_draw();
}

void ui_msg(const char *s)
{
    uint8_t x;
    for (x = 0; x < SCR_W; x++) { cell_tile[SCR_H - 1][x] = 0xFFFF; ui_textbuf[SCR_H - 1][x] = ' '; }
    cv_hint(s, 1);
    msg_timer = 90;
}

void ui_msg_tick(void)
{
    if (msg_timer && !--msg_timer) hint_draw();
}

/* ---- input ------------------------------------------------------------------ */
/* The VBlank interrupt samples the keys every frame and keeps every press and
 * release until the main loop reads them, so a press shorter than a slow UI
 * pass still arrives.  A key pressed and released within one pass counts as
 * held for that pass (so A+D-pad combos and taps both still work). */
static volatile uint16_t ks_down, ks_up, ks_last;
void ui_key_sample(void)
{
    uint16_t k = (uint16_t)(~REG_KEYINPUT & 0x03FF);
    ks_down |= (uint16_t)(k & ~ks_last);
    ks_up |= (uint16_t)(~k & ks_last);
    ks_last = k;
}

void ui_input(void)
{
    uint16_t k, down, up;
    uint32_t ime = REG_IME;
    REG_IME = 0;
    k = ks_last; down = ks_down; up = ks_up;
    ks_down = 0; ks_up = 0;
    REG_IME = ime;
    k = (uint16_t)(k | (down & ~prev_keys));             /* tapped within the pass: held for it */
    key_pressed = (uint16_t)((k & ~prev_keys) | (down & ~prev_keys));
    key_released = (uint16_t)((up & prev_keys) | (prev_keys & ~k));
    key_held = k;
    {   /* a key both pressed and released inside the pass is released next pass */
        uint16_t quick = (uint16_t)(down & up & ~ks_last);
        if (quick) ks_up |= quick;
    }
    key_repeat = key_pressed;
    if (k & KEY_DPAD & prev_keys) {
        if (++rep_timer >= 18) { key_repeat |= (uint16_t)(k & KEY_DPAD); rep_timer = 14; }
    } else rep_timer = 0;
    if (key_pressed) evo_stir((uint32_t)key_pressed << 16 | REG_VCOUNT);   /* entropy from timing */
    if (key_pressed & KEY_A) a_edited = 0;
    if (key_pressed & KEY_B) b_used = 0;
    prev_keys = k;
}

uint8_t ui_nav(void)
{
    if (key_held & (KEY_A | KEY_B | KEY_SELECT)) return 0;
    return (uint8_t)((key_repeat & KEY_DPAD) >> 4);       /* bit0 R, bit1 L, bit2 U, bit3 D */
}

int8_t ui_edit(uint8_t coarse)
{
    int8_t d = 0;
    if (!(key_held & KEY_A) || (key_held & (KEY_B | KEY_SELECT))) return 0;
    if (key_repeat & KEY_RIGHT) d = 1;
    if (key_repeat & KEY_LEFT) d = -1;
    if (key_repeat & KEY_UP) d = (int8_t)coarse;
    if (key_repeat & KEY_DOWN) d = (int8_t)-coarse;
    if (d) a_edited = 1;
    return d;
}

uint8_t ui_a_tap(void)
{
    if ((key_released & KEY_A) && !a_edited && !(key_held & (KEY_B | KEY_SELECT))) { a_edited = 1; return 1; }
    return 0;
}

uint8_t ui_clear(void)
{
    if ((key_held & KEY_B) && (key_pressed & KEY_A)) { b_used = 1; a_edited = 1; return 1; }
    return 0;
}

uint8_t ui_b_dir(void)
{
    if (!(key_held & KEY_B) || (key_held & KEY_A)) return 0;
    if (key_repeat & (KEY_DPAD | KEY_L | KEY_R)) b_used = 1;
    return (uint8_t)((key_repeat & KEY_DPAD) >> 4);
}

uint8_t ui_b_tap(void)
{
    if ((key_released & KEY_B) && !b_used && !(key_held & (KEY_A | KEY_SELECT))) { b_used = 1; return 1; }
    return 0;
}

void ui_consume(void) { a_edited = 1; b_used = 1; }

uint8_t clampadd(uint8_t v, int8_t d, uint8_t lo, uint8_t hi)
{
    int16_t r = (int16_t)v + d;
    if (r < lo) r = lo;
    if (r > hi) r = hi;
    return (uint8_t)r;
}
