/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Direct Sound mixer.
 *
 * Output: Direct Sound A = left, B = right, 8-bit signed, clocked by timer 0
 * at MIX_RATE.  The rate is chosen so that exactly MIX_SPF samples elapse
 * per video frame (280 896 cycles), which keeps the DMA double buffer locked
 * to VBlank -- the scheme commercial GBA sound drivers use:
 *
 *   buffer = [half 0][half 1], MIX_SPF bytes each, per channel
 *   VBlank k: DMA starts playing half k%2 (restarted at the buffer start
 *             when k%2 == 0); the CPU mixes the half that just finished.
 *
 * Mixing runs inside the VBlank interrupt, so it can never be starved by UI
 * work, file operations or CORROSION -- those run in the main loop.  Each
 * frame is rendered as MIX_BLOCKS_PER_FRAME blocks of 16 samples; before each
 * block the sequencer advances (sample-accurate timing, independent of the
 * screen).  CPU use is measured in scanlines with VCOUNT.
 */
#include "audio.h"

static int8_t bufL[2 * MIX_SPF] ALIGN4;
static int8_t bufR[2 * MIX_SPF] ALIGN4;
static int32_t acc[MIX_BLOCK * 3] ALIGN4;            /* interleaved L, R, SEND (sample x 1024) */
static uint8_t started, half;
static int32_t lim_gain = 512;              /* Q9 limiter gain */
static uint32_t test_ph;

volatile uint32_t mix_frames;
volatile uint32_t prof_cyc[6], prof_max[6], prof_vmax[N_MELODIC], prof_vmax_eng[N_MELODIC];
extern uint32_t vframe[N_MELODIC], vframe_eng[N_MELODIC], vlast[N_MELODIC], drums_last, fixed_last;
volatile uint32_t bench_cyc[4];
volatile uint32_t bench_sink;           /* seq, voices, drums, fx, master, total (last frame) */
static inline uint32_t cyc(void) { return ((uint32_t)REG_TM3CNT_L << 16) | REG_TM2CNT_L; }
volatile uint16_t mix_lines_last, mix_lines_max, mix_overruns, mix_clips;
volatile uint32_t mix_lines_sum;
volatile uint8_t mix_peak;
int8_t mix_scope[64];
volatile uint8_t audio_test_tone, audio_solo_engine;

void voices_render(int32_t *acc) IWRAM_CODE;
void drums_render(int32_t *acc) IWRAM_CODE;
void fx_process(int32_t *acc) IWRAM_CODE;
void voice_govern(uint16_t lines) IWRAM_CODE;
extern int16_t sine_iw[1024];

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

static void dma_restart(void)
{
    REG_DMA1CNT_H = 0;
    REG_DMA2CNT_H = 0;
    REG_DMA1SAD = (uint32_t)bufL;
    REG_DMA1DAD = REG_FIFO_A;
    REG_DMA2SAD = (uint32_t)bufR;
    REG_DMA2DAD = REG_FIFO_B;
    REG_DMA1CNT_H = DMA_ENABLE | DMA_SPECIAL | DMA_32 | DMA_REPEAT | DMA_DST_FIXED;
    REG_DMA2CNT_H = DMA_ENABLE | DMA_SPECIAL | DMA_32 | DMA_REPEAT | DMA_DST_FIXED;
}

static void start_hw(void)
{
    REG_SOUNDCNT_X = SNDX_ENABLE;
    REG_SOUNDCNT_L = 0;
    REG_SOUNDCNT_H = SNDH_A_100 | SNDH_B_100 | SNDH_A_L | SNDH_B_R | SNDH_A_T0 | SNDH_B_T0
                   | SNDH_A_RESET | SNDH_B_RESET;
    REG_TM0CNT_H = 0;
    REG_TM0CNT_L = (uint16_t)(65536 - MIX_TIMER_RELOAD);
    dma_restart();
    REG_TM0CNT_H = TM_ENABLE;
}

/* master: limiter + optional tanh drive.  The limiter is preventive with one
 * block of look-behind: each block's peak sets the gain ramp of the next
 * (attack within 1.5 ms), and the final int8 clamp catches what is left. */
static int32_t prev_peak;
static void master_out(int8_t *oL, int8_t *oR, uint32_t blk)
{
    int32_t n, drv = proj.fx.master_drive, pk = 1;
    int32_t g0 = lim_gain, g1, dg, pre = 512 + drv * 128;          /* Q9 drive gain */
    const int32_t *a = acc;
    const int32_t ceiling = 32000;
    int32_t want = (prev_peak * pre) >> 9;
    g1 = g0 + ((512 - g0) >> 6);                                     /* release toward unity (Q9) */
    if (want > 0) {
        int32_t lim = drv ? ceiling * 3 : ceiling;
        if (((want >> 4) * g1 >> 5) > lim) { g1 = (lim << 9) / want; mix_clips++; }
    }
    lim_gain = g1;
    dg = (((g1 - g0) * pre) >> 9) / MIX_BLOCK;
    g0 = (g0 * pre) >> 9;
    if (drv) {
        for (n = 0; n < MIX_BLOCK; n++, a += 3) {
            int32_t l = a[0] >> 11, r = a[1] >> 11, al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
            if (al > pk) pk = al;
            if (ar > pk) pk = ar;
            l = (l * g0) >> 16; r = (r * g0) >> 16;              /* -> /128 units for the curve */
            g0 += dg;
            l = tab_tanh[clamp(l + 512, 0, 1023)];
            r = tab_tanh[clamp(r + 512, 0, 1023)];
            oL[n] = (int8_t)(l >> 8);
            oR[n] = (int8_t)(r >> 8);
        }
    } else {
        for (n = 0; n < MIX_BLOCK; n++, a += 3) {
            int32_t l = a[0] >> 11, r = a[1] >> 11, al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
            if (al > pk) pk = al;
            if (ar > pk) pk = ar;
            l = (l * g0) >> 16; r = (r * g0) >> 16;
            g0 += dg;
            oL[n] = (int8_t)(l > 127 ? 127 : l < -127 ? -127 : l);
            oR[n] = (int8_t)(r > 127 ? 127 : r < -127 ? -127 : r);
        }
    }
    prev_peak = pk << 1;                                             /* back to the sample domain */
    if ((blk & 3) == 0) {
        uint32_t si = (mix_frames * 4 + (blk >> 2)) & 63;
        mix_scope[si] = (int8_t)((oL[0] + oR[0]) >> 1);
    }
}

static void render_frame(int8_t *oL, int8_t *oR)
{
    uint32_t b, n;
    int32_t peak = 0;
    uint32_t pacc[5] = { 0, 0, 0, 0, 0 }, t0, t1, tf = cyc();
    for (b = 0; b < MIX_BLOCKS_PER_FRAME; b++) {
        t0 = cyc();
        seq_block();
        t1 = cyc(); pacc[0] += t1 - t0; t0 = t1;
        for (n = 0; n < MIX_BLOCK * 3; n++) acc[n] = 0;
        if (audio_test_tone) {
            /* diagnostics: a pure 440 Hz sine -- any discontinuity at frame or
             * buffer boundaries shows up as a click in the capture */
            for (n = 0; n < MIX_BLOCK; n++) {
                acc[3 * n] = acc[3 * n + 1] = (sine_iw[test_ph >> 22] >> 1) << 10;
                test_ph += (uint32_t)(440.0 * 4294967296.0 / (16777216.0 / MIX_TIMER_RELOAD));
            }
        } else {
            voices_render(acc);
            t1 = cyc(); pacc[1] += t1 - t0; t0 = t1;
            drums_render(acc);
            t1 = cyc(); pacc[2] += t1 - t0; t0 = t1;
            fx_process(acc);
            t1 = cyc(); pacc[3] += t1 - t0; t0 = t1;
        }
        master_out(oL + b * MIX_BLOCK, oR + b * MIX_BLOCK, b);
        t1 = cyc(); pacc[4] += t1 - t0;
        for (n = 0; n < MIX_BLOCK; n++) {
            int32_t a = oL[b * MIX_BLOCK + n];
            if (a < 0) a = -a;
            if (a > peak) peak = a;
        }
    }
    mix_peak = (uint8_t)peak;
    for (n = 0; n < 5; n++) prof_cyc[n] = pacc[n];
    prof_cyc[5] = cyc() - tf;
    if (prof_cyc[5] > prof_max[5]) {
        for (n = 0; n < 6; n++) prof_max[n] = prof_cyc[n];
        for (n = 0; n < N_MELODIC; n++) { prof_vmax[n] = vframe[n]; prof_vmax_eng[n] = vframe_eng[n]; }
    }
    for (n = 0; n < N_MELODIC; n++) { vlast[n] = vframe[n]; vframe[n] = 0; }
    drums_last = pacc[2];
    fixed_last = pacc[0] + pacc[3] + pacc[4];
}

void mixer_init(void)
{
    uint32_t i;
    for (i = 0; i < 2 * MIX_SPF; i++) bufL[i] = bufR[i] = 0;
    started = 0;
    fx_reset();
}

void audio_vblank(void)
{
    uint16_t v0, v1, lines;
    int8_t *oL, *oR;
    if (!started) {
        start_hw();
        started = 1;
        half = 0;
        return;
    }
    half ^= 1;
    if (half == 0) dma_restart();               /* wrapped: back to the buffer start */
    voice_govern(mix_lines_last);
    v0 = REG_VCOUNT;
    /* mix the half that just finished; it plays after the current one */
    oL = bufL + (half ^ 1) * MIX_SPF;
    oR = bufR + (half ^ 1) * MIX_SPF;
    render_frame(oL, oR);
    mix_frames++;
    v1 = REG_VCOUNT;
    lines = (uint16_t)((v1 + 228 - v0) % 228);
    if (REG_IF & IRQ_VBLANK) { mix_overruns++; lines = (uint16_t)(lines + 228); }
    mix_lines_last = lines;
    mix_lines_sum += lines;
    if (lines > mix_lines_max) mix_lines_max = lines;
}
