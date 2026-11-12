/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * UI core: tile text drawing, header/help rows, and ETCH's input grammar.
 */
#include "ui.h"
#include "seq.h"
#include "audio.h"
#include "evolve.h"

#define MAP SCREENBLOCK(30)
const char * const scr_names[SCR_COUNT] = { "LIVE", "TRACK", "SYNTH", "MOD", "GEN", "MEMORY", "SONG", "FILE" };
char ui_textbuf[SCR_H][SCR_W + 1] EWRAM_BSS;

uint16_t key_held, key_pressed, key_released, key_repeat;
static uint16_t prev_keys;
static uint8_t rep_timer;
static uint8_t a_edited, b_used;
static uint8_t msg_timer;
static char help_text[SCR_W + 1];

void ui_init(void)
{
    uint32_t i;
    volatile uint32_t *cb = CHARBLOCK(0);
    for (i = 0; i < 256 * 8; i++) cb[i] = gfx_tiles[i];
    for (i = 0; i < 256; i++) PAL_BG[i] = gfx_palette[i];
    REG_BG0CNT = BG_CBB(0) | BG_SBB(30) | BG_4BPP | BG_PRIO(1);
    REG_BG0HOFS = 0; REG_BG0VOFS = 0;
    ui_cls();
}

void ui_cls(void)
{
    uint32_t i;
    for (i = 0; i < 32 * 32; i++) MAP[i] = ' ';
    for (i = 0; i < SCR_H; i++) { memset(ui_textbuf[i], ' ', SCR_W); ui_textbuf[i][SCR_W] = 0; }
}

void ui_put(uint8_t x, uint8_t y, uint16_t tile, uint8_t pal)
{
    if (x >= SCR_W || y >= SCR_H) return;
    MAP[y * 32 + x] = (uint16_t)(tile | (pal << 12));
    ui_textbuf[y][x] = (char)(tile < 128 ? tile : '#');
}

uint16_t ui_screen_tile(uint8_t x, uint8_t y) { return MAP[y * 32 + x]; }

void ui_recolor(uint8_t x, uint8_t y, uint8_t w, uint8_t pal)
{
    while (w-- && x < SCR_W) { MAP[y * 32 + x] = (uint16_t)((MAP[y * 32 + x] & 0x0FFF) | (pal << 12)); x++; }
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
    char b[6];
    int8_t i;
    for (i = (int8_t)(digits - 1); i >= 0; i--) {
        b[i] = (char)('0' + v % 10);
        v /= 10;
        if (!v) { i--; break; }
    }
    for (; i >= 0; i--) b[i] = ' ';
    b[digits] = 0;
    ui_text(x, y, b, pal);
}

void ui_num0(uint8_t x, uint8_t y, uint16_t v, uint8_t digits, uint8_t pal)
{
    char b[6];
    int8_t i;
    for (i = (int8_t)(digits - 1); i >= 0; i--) { b[i] = (char)('0' + v % 10); v /= 10; }
    b[digits] = 0;
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

void ui_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max, uint8_t pal)
{
    uint16_t px = max ? (uint16_t)((uint32_t)v * w * 8 / max) : 0;
    uint8_t i;
    for (i = 0; i < w; i++) {
        uint16_t c = px > 8 ? 8 : px;
        ui_put((uint8_t)(x + i), y, (uint16_t)(T_HBAR + c), pal);
        px = (uint16_t)(px - c);
    }
}

/* ---- header (row 0) and help/message row (row 19) ---------------------- */
static const char *hdr_name;
void ui_header(const char *screen)
{
    hdr_name = screen;
    ui_fill(0, 0, SCR_W, ' ', PAL_HEAD);
    ui_text(0, 0, "ETCH II", PAL_HEAD);
    ui_put(8, 0, T_VRULE, PAL_HEAD);
    ui_text(9, 0, screen, PAL_HEAD);
    ui_header_update();
}

void ui_header_update(void)
{
    /* right side: pattern, BPM, transport, evolution mode */
    static const char mode_c[3] = { 'S', 'D', 'C' };
    ui_put(17, 0, 'P', PAL_HEAD);
    ui_num0(18, 0, (uint16_t)(seq_cur + 1), 2, PAL_HEAD);
    ui_put(20, 0, seq_queued != 0xFF ? '>' : ' ', PAL_HEAD);
    ui_num(21, 0, proj.bpm, 3, PAL_HEAD);
    ui_put(25, 0, (uint8_t)mode_c[proj.gen.mode % 3], proj.gen.mode == EVO_STABLE ? PAL_HEAD : PAL_PLAYHEAD);
    ui_put(26, 0, proj.compat == COMPAT_EXPANDED ? 'X' : 'C', PAL_HEAD);
    ui_put(27, 0, seq_fill ? 'F' : ' ', PAL_HEAD);
    ui_put(29, 0, seq_playing ? T_PLAY : T_STOP, seq_playing ? PAL_PLAYHEAD : PAL_HEAD);
    ui_put(28, 0, seq_playing ? (uint16_t)('1' + ((seq_step >> 2) & 3)) : ' ', PAL_HEAD);
}

void ui_help(const char *s)
{
    uint8_t i;
    for (i = 0; i < SCR_W && s[i]; i++) help_text[i] = s[i];
    help_text[i] = 0;
    if (!msg_timer) ui_textn(0, SCR_H - 1, help_text, SCR_W, PAL_DIM);
}

void ui_msg(const char *s)
{
    ui_textn(0, SCR_H - 1, s, SCR_W, PAL_VALUE);
    msg_timer = 90;
}

void ui_msg_tick(void)
{
    if (msg_timer && !--msg_timer) ui_textn(0, SCR_H - 1, help_text, SCR_W, PAL_DIM);
}

/* ---- input ------------------------------------------------------------------ */
void ui_input(void)
{
    uint16_t k = (uint16_t)(~REG_KEYINPUT & 0x03FF);
    key_pressed = (uint16_t)(k & ~prev_keys);
    key_released = (uint16_t)(~k & prev_keys);
    key_held = k;
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
