/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Shared effects bus: a tempo-synced stereo (ping-pong) delay with a
 * low-pass filter in the feedback path.  Every track has a send level; the
 * delay is computed once for all of them, which is far cheaper than a delay
 * per voice.  Per-voice effects (drive, filter, bit reduction, panning) live
 * in the voice renderers; master drive and the limiter in the mixer.
 */
#include "audio.h"

#define DLY_MAX 8192                            /* half-rate: ~0.9 s at 18 kHz */
static int16_t dlyL[DLY_MAX] EWRAM_BSS;
static int16_t dlyR[DLY_MAX] EWRAM_BSS;
static uint32_t dpos, dlen = 4000, dlen_target = 4000;
static int32_t lpL, lpR;

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void fx_reset(void)
{
    uint32_t i;
    for (i = 0; i < DLY_MAX; i++) dlyL[i] = dlyR[i] = 0;
    dpos = 0; lpL = lpR = 0;
}

/* delay_time 0..7 -> 16th notes: 1, 2, 3 (dotted 8th), 4, 6, 8, 1.5, 2/3 */
static const uint8_t dly_16ths_x6[8] = { 6, 12, 18, 24, 36, 48, 9, 4 };

void fx_set_tempo(uint8_t bpm)
{
    uint32_t sixteenth, len;
    if (bpm < 60) bpm = 60;
    sixteenth = (uint32_t)MIX_RATE * 15u / bpm;            /* samples per 16th */
    len = sixteenth * dly_16ths_x6[proj.fx.delay_time & 7] / 6u;
    while (len >= 2 * DLY_MAX) len >>= 1;
    if (len < 64) len = 64;
    dlen_target = len;
}

/* ping-pong delay: the send enters the left line; each line's filtered
 * output feeds the other (WIDTH blends toward plain stereo echoes).  The
 * bus is skipped entirely once sends are silent and the tail has died. */
static uint32_t idle_blocks;
extern volatile uint8_t fx_send_active;
void fx_process(int32_t *acc) IWRAM_CODE;
void fx_process(int32_t *acc)
{
    int32_t fb = proj.fx.delay_fb * 1900;                  /* Q15, up to 0.87 */
    int32_t k = 3000 + proj.fx.delay_tone * 1800;          /* feedback low-pass, Q15 */
    int32_t w = proj.fx.width;
    int32_t a = lpL, b = lpR, in_any = 0;
    int16_t *dL = dlyL, *dR = dlyR;
    (void)w;
    uint32_t n, p = dpos, d;
    if (dlen != dlen_target) {
        if (dlen < dlen_target) dlen += 1 + ((dlen_target - dlen) >> 5);
        else dlen -= 1 + ((dlen - dlen_target) >> 5);
    }
    in_any = fx_send_active;
    fx_send_active = 0;
    if (!in_any) { if (idle_blocks > 2u * DLY_MAX / MIX_BLOCK) return; idle_blocks++; } else idle_blocks = 0;
    /* the bus runs at half the mixer rate: one delay sample per output pair
     * (the feedback path is low-passed anyway), halving its cost */
    d = DLY_MAX - (dlen >> 1);
    if (w >= 8) {                                          /* ping-pong */
        for (n = 0; n < MIX_BLOCK; n += 2) {
            uint32_t rp = (p + d) & (DLY_MAX - 1);
            int32_t yl = dL[rp], yr = dR[rp], x = (acc[3 * n + 2] + acc[3 * n + 5]) >> 12;
            a += ((yl - a) * k) >> 15;
            b += ((yr - b) * k) >> 15;
            dL[p] = (int16_t)clamp(x + ((b * fb) >> 15), -32767, 32767);
            dR[p] = (int16_t)clamp((a * fb) >> 15, -32767, 32767);
            p = (p + 1) & (DLY_MAX - 1);
            acc[3 * n] += yl << 10; acc[3 * n + 3] += yl << 10;
            acc[3 * n + 1] += yr << 10; acc[3 * n + 4] += yr << 10;
        }
    } else {                                               /* stereo echoes */
        for (n = 0; n < MIX_BLOCK; n += 2) {
            uint32_t rp = (p + d) & (DLY_MAX - 1);
            int32_t yl = dL[rp], yr = dR[rp], x = (acc[3 * n + 2] + acc[3 * n + 5]) >> 12;
            a += ((yl - a) * k) >> 15;
            b += ((yr - b) * k) >> 15;
            dL[p] = (int16_t)clamp(x + ((a * fb) >> 15), -32767, 32767);
            dR[p] = (int16_t)clamp(x + ((b * fb) >> 15), -32767, 32767);
            p = (p + 1) & (DLY_MAX - 1);
            acc[3 * n] += yl << 10; acc[3 * n + 3] += yl << 10;
            acc[3 * n + 1] += yr << 10; acc[3 * n + 4] += yr << 10;
        }
    }
    lpL = a; lpR = b; dpos = p;
}
