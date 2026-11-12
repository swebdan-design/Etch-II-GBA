/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Title screen: the engraved logo, and an oscilloscope trace that slowly
 * corrodes -- a clean sine is eaten into a stepped, rusted waveform while
 * oxide specks gather and flake beneath it.  The animation is drawn in the
 * main loop into RAM and copied to VRAM by DMA; it never touches audio.
 */
#include "ui.h"
#include "audio.h"
#include "evolve.h"

#define BM_X 0
#define BM_Y 10
#define BM_TW 30
#define BM_TH 6
#define BM_TILE0 256
static uint32_t bm[BM_TW * BM_TH * 8] EWRAM_BSS;    /* 4bpp tiles */
static uint8_t rust[240] EWRAM_BSS;                  /* oxide height per column */
extern int16_t sine_iw[1024];

static void plot(int x, int y, uint32_t c)
{
    uint32_t t, *w;
    if (x < 0 || x >= BM_TW * 8 || y < 0 || y >= BM_TH * 8) return;
    t = (uint32_t)((y >> 3) * BM_TW + (x >> 3));
    w = &bm[t * 8 + (y & 7)];
    *w = (*w & ~(15u << ((x & 7) * 4))) | (c << ((x & 7) * 4));
}

static void bm_map(void)
{
    uint8_t x, y;
    for (y = 0; y < BM_TH; y++)
        for (x = 0; x < BM_TW; x++)
            ui_put((uint8_t)(BM_X + x), (uint8_t)(BM_Y + y), (uint16_t)(BM_TILE0 + y * BM_TW + x), PAL_SCOPE);
}

static uint32_t trng = 0x1F2E3D4C;
static uint32_t tr(void) { trng ^= trng << 13; trng ^= trng >> 17; trng ^= trng << 5; return trng; }

static void draw_wave(uint32_t t)
{
    int x, prev = -1;
    uint32_t corr = (t * 3) & 1023;                  /* corrosion amount cycles slowly */
    int amt = corr < 512 ? (int)corr : (int)(1023 - corr);   /* 0..511 */
    REG_DMA3CNT = 0;
    {
        volatile uint32_t zero = 0;
        REG_DMA3SAD = (uint32_t)&zero;
        REG_DMA3DAD = (uint32_t)bm;
        REG_DMA3CNT = (uint32_t)(BM_TW * BM_TH * 8) | ((uint32_t)(DMA_ENABLE | DMA_32 | DMA_SRC_FIXED) << 16);
    }
    /* oxide bed: grows and flakes */
    for (x = 0; x < 240; x += 1) {
        uint32_t r = tr();
        if ((r & 63) < (uint32_t)(amt >> 4) && rust[x] < 14) rust[x]++;
        else if ((r & 255) == 7 && rust[x]) rust[x]--;
    }
    for (x = 0; x < 240; x++) {
        int k;
        for (k = 0; k < rust[x]; k++) if ((tr() & 3) != 0) plot(x, 47 - k, (k & 1) ? 4 : 3);
    }
    /* the trace: sine -> quantised, bitten waveform */
    for (x = 0; x < 240; x++) {
        uint32_t ph = (uint32_t)(x * 4 * 1024 / 240 + t * 6) & 1023;
        int s = sine_iw[ph];                              /* +-32767 */
        int q = 32767 / (1 + (amt >> 6));                 /* step size grows */
        int y;
        if (amt > 64) s = (s / q) * q;
        if (amt > 160 && ((x * 7 + t) % 29) < (amt >> 6)) s = s * 3 / 5;   /* pitting */
        y = 20 - (s * 15) / 32767;
        if (prev >= 0) {
            int a = prev < y ? prev : y, b = prev < y ? y : prev, k;
            for (k = a; k <= b; k++) plot(x, k, 1);
        } else plot(x, y, 1);
        prev = y;
    }
    {
        volatile uint32_t *dst = CHARBLOCK(0) + BM_TILE0 * 8;
        REG_DMA3SAD = (uint32_t)bm;
        REG_DMA3DAD = (uint32_t)dst;
        REG_DMA3CNT = (uint32_t)(BM_TW * BM_TH * 8) | ((uint32_t)(DMA_ENABLE | DMA_32) << 16);
    }
}

void scr_title_run(void)
{
    uint32_t t = 0;
    uint8_t x, y;
    ui_cls();
    for (y = 0; y < LOGO_H; y++)
        for (x = 0; x < LOGO_W; x++)
            ui_put((uint8_t)(8 + x), (uint8_t)(1 + y), (uint16_t)(T_LOGO + y * LOGO_W + x), PAL_LOGO);
    ui_text(0, 6, "A Generative Synthesis Machine", PAL_TEXT);
    ui_text(7, 8, "by Synth Centric", PAL_LABEL);
    bm_map();
    ui_text(2, 18, "CORROSION v2.4  -  8 PIECES", PAL_DIM);
    for (;;) {
        vblank_wait_bios();
        ui_input();
        draw_wave(t);
        ui_textn(10, 17, (t & 32) ? "PRESS START" : "", 11, PAL_VALUE);
        if (key_pressed & KEY_START) break;
        t++;
    }
    evo_stir(t);
    ui_cls();
}
