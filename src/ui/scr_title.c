/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Title screen, laid out as ETCH DS's (SERIES_SPEC.md 3.3): the wordmark,
 * the subtitle, "by Synth Centric", the version, and an oscilloscope trace
 * that slowly corrodes -- a clean sine is eaten into a stepped, pitted
 * waveform while oxide gathers and flakes beneath it.  Drawn in the main
 * loop; it never touches audio.
 */
#include "ui.h"
#include "canvas.h"
#include "audio.h"
#include "evolve.h"

#define WV_Y 84            /* the trace's band */
#define WV_H 40
static uint8_t rust[240] EWRAM_BSS;                  /* oxide height per column */
extern int16_t sine_iw[1024];

static uint32_t trng = 0x1F2E3D4C;
static uint32_t tr(void) { trng ^= trng << 13; trng ^= trng >> 17; trng ^= trng << 5; return trng; }

/* column by column: each column is cleared and redrawn at once, so the
 * trace never flickers while the screen is being drawn */
static void draw_wave(uint32_t t)
{
    int x, prev = -1;
    uint32_t corr = (t * 3) & 1023;                  /* corrosion amount cycles slowly */
    int amt = corr < 512 ? (int)corr : (int)(1023 - corr);   /* 0..511 */
    int q = 32767 / (1 + (amt >> 6));                 /* step size grows */
    for (x = 6; x < 234; x++) {
        uint32_t r = tr(), ph = (uint32_t)(x * 4 * 1024 / 240 + t * 6) & 1023;
        int s = sine_iw[ph], y, k, a, b;
        if ((r & 63) < (uint32_t)(amt >> 4) && rust[x] < 10) rust[x]++;     /* oxide gathers */
        else if ((r & 255) == 7 && rust[x]) rust[x]--;                        /* and flakes  */
        if (amt > 64) s = (s / q) * q;
        if (amt > 160 && ((x * 7 + (int)t) % 29) < (amt >> 6)) s = s * 3 / 5;   /* pitting */
        y = WV_Y + 14 - (s * 12) / 32767;
        if (prev < 0) prev = y;
        a = prev < y ? prev : y; b = prev < y ? y : prev;
        cv_vline(x, WV_Y, WV_H, CV_BG);
        for (k = 0; k < rust[x]; k++)
            if ((tr() & 3) != 0) cv_point(x, WV_Y + WV_H - 1 - k, (k & 1) ? CV_ACC_TINT : CV_SURF3);
        cv_vline(x, a, b - a + 1, CV_TEXT);
        prev = y;
    }
}

void scr_title_run(void)
{
    uint32_t t = 0;
    int x, w;
    ui_cls();
    /* wordmark: ETCH in black, II in grey, as ETCH DS */
    w = (cv_text_w(F_MAIN, "ETCH") + cv_text_w(F_MAIN, "II") + 2) * 4;
    x = cv_text_scaled(120 - w / 2, 14, F_MAIN, 4, CV_TEXT, "ETCH");
    cv_text_scaled(x + 4, 14, F_MAIN, 4, CV_TEXT3, "II");
    cv_text_c(120, 48, F_SMALL, CV_TEXT2, "A Generative Synthesis Machine");
    cv_hline(104, 57, 32, CV_BORDER);
    cv_text_c(120, 62, F_MAIN, CV_TEXT, "by Synth Centric");
    cv_text_c(120, 74, F_SMALL, CV_TEXT3, "VERSION " ETCH2_VERSION_STR "   CORROSION FROM ETCH 2.5   8 PIECES");
    cv_text_c(120, 150, F_SMALL, CV_TEXT3, "HEADPHONES RECOMMENDED");
    for (;;) {
        vblank_wait_bios();
        ui_input();
        draw_wave(t);
        if ((t & 31) == 0) {
            cv_rect(80, 132, 80, 8, CV_BG);
            if (t & 32) cv_text_c(120, 132, F_MAIN, CV_TEXT, "PRESS START");
            else { uint8_t i; for (i = 0; i < 30; i++) ui_textbuf[16][i] = ' '; }
        }
        if (key_pressed & KEY_START) break;
        t++;
    }
    evo_stir(t);
    ui_cls();
}
