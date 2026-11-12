/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Melodic voices: SUBTRACTIVE, FM, WAVETABLE, PHASE DISTORTION, RESONATOR.
 *
 * ARM code in IWRAM.  Each voice is rendered in 16-sample blocks: a control
 * update (envelopes, glide, LFO, coefficient lookup) followed by a tight
 * per-sample loop.  The output stage ramps the gain linearly across the
 * block so control-rate envelopes never click.
 *
 * Fixed point: oscillator phases are 32-bit (one cycle = 2^32); audio is
 * int32 with a nominal +-32767 range; envelopes are Q24; gains Q15.
 * Randomness used here (LFO sample & hold, resonator excitation) comes from
 * a per-voice xorshift stream and NEVER from the CORROSION generator.
 */
#include "audio.h"

voice_t voices[N_MELODIC];
int16_t sine_iw[1024];                    /* tab_sine copied to IWRAM */
static int16_t dl_mem[N_MELODIC][DL_LEN] EWRAM_BSS;
volatile int8_t evo_cut_ofs;

#define Q24 (1 << 24)

static inline uint32_t xs32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}

/* pitch (MIDI * 64) -> 32-bit phase increment */
static inline uint32_t pitch_inc(int32_t p)
{
    int32_t o, r;
    if (p < 0) p = 0;
    if (p > 131 * 64) p = 131 * 64;
    o = (p * 21846) >> 24;                /* p / 768 */
    r = p - o * 768;
    if (r >= 768) { r -= 768; o++; }
    if (r < 0) { r += 768; o--; }
    if (o > 10) o = 10;
    return tab_exp_inc[r] >> (10 - o);
}

/* pitch -> resonator delay length (Q16 samples), no division */
static inline uint32_t pitch_len(int32_t p)
{
    int32_t o, r;
    if (p < 0) p = 0;
    if (p > 131 * 64) p = 131 * 64;
    o = (p * 21846) >> 24;
    r = p - o * 768;
    if (r >= 768) { r -= 768; o++; }
    if (r < 0) { r += 768; o--; }
    return tab_res_len[r] >> o;
}

/* 2^47 / d for d in [2^24, 2^32), via a normalised reciprocal table */
static inline uint32_t recip47(uint32_t d)
{
    uint32_t m = d, e = 0;
    while (m >= 1024) { m >>= 1; e++; }
    if (m < 256) m = 256;
    return e >= 16 ? tab_recip[m - 256] >> (e - 16) : tab_recip[m - 256] << (16 - e);
}

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline int32_t sine_at(uint32_t ph) { return sine_iw[ph >> 22]; }

/* ratio table for FM (Q8) */
static const uint16_t fm_ratio[16] = { 128, 256, 384, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048, 2304, 2816, 3328, 362 };
static const uint16_t fm_cratio[4] = { 256, 512, 768, 128 };

/* ---------------------------------------------------------------------------- */
void voice_init_all(void) IWRAM_CODE;
void voice_init_all(void)
{
    uint32_t i;
    for (i = 0; i < 1024; i++) sine_iw[i] = tab_sine[i];
    for (i = 0; i < N_MELODIC; i++) {
        voice_t *v = &voices[i];
        memset(v, 0, sizeof *v);
        v->dl = dl_mem[i];
        v->rng = 0x9E3779B9u ^ (i * 0x01000193u);
        v->g0 = v->g1 = 0;
    }
}

void voice_kill(uint8_t vi)
{
    voice_t *v = &voices[vi];
    v->active = 0; v->stage = 0; v->amp = 0; v->g0 = v->g1 = 0; v->held = 0;
}

void audio_all_off(void)
{
    uint32_t i;
    for (i = 0; i < N_MELODIC; i++) voice_kill((uint8_t)i);
    for (i = 0; i < N_DRUMS; i++) dvoices[i].active = 0;
}

/* Start a note.  glide = 1: the gate is held from the previous note and the
 * pitch slides (envelopes do not restart, as in ETCH / TB-style slides). */
void voice_note_on(uint8_t vi, uint8_t trk, int32_t pitch, uint8_t accent, uint8_t glide)
{
    voice_t *v = &voices[vi];
    const patch_t *pt = &proj.patch[trk];
    uint8_t d;

    v->track = trk;
    v->nosc = 1;
    if (glide && v->active && v->held) {
        v->target = pitch;
        if (accent) {                     /* accent on a slid note: sweep + loudness, no reset */
            v->aenv = Q24;
            v->accented = 1;
            v->acc_charge += 40;
            if (v->acc_charge > 120) v->acc_charge = 120;
        }
        return;
    }
    if (v->engine != pt->engine || !v->active) {
        /* fresh voice state when the engine changes or the voice was idle */
        v->lp = v->bp = v->dc = v->fb1 = v->fb2 = 0;
        v->ph[0] = v->ph[1] = v->ph[2] = v->ph[3] = 0;
        v->xph[0][0] = 0x55555555u; v->xph[0][1] = 0; v->xph[1][0] = 0xAAAAAAAAu; v->xph[1][1] = 0;
        v->exc_lp = 0;
        v->g0 = 0;
        if (pt->engine == ENG_RES) memset(v->dl, 0, DL_LEN * 2);
    }
    v->engine = pt->engine;
    v->pitch = v->target = pitch;
    v->active = 1;
    v->held = 1;
    v->stage = 1;
    v->accented = accent;
    v->retrig = 1;
    if (pt->p[P_ATTACK] == 0) { v->amp = Q24; v->stage = 2; }
    v->att_inc = tab_env_attack[pt->p[P_ATTACK] * 2];
    d = (uint8_t)(pt->p[P_DECAY] * 2 + 1);
    v->dec_k = tab_env_decay[d];
    v->sus = (int32_t)pt->p[P_SUSTAIN] * (Q24 / 15);
    v->rel_k = tab_env_decay[pt->p[P_RELEASE] * 2];
    d = (uint8_t)(pt->p[P_MDECAY] * 2 + 1);
    if (accent && d > 12) d = 12;          /* accented notes snap shut (ETCH) */
    v->menv_k = tab_env_decay[d];
    v->menv = Q24;
    v->aenv_k = tab_env_decay[10];
    if (accent) {
        v->aenv = Q24;
        v->acc_charge += 40;
        if (v->acc_charge > 120) v->acc_charge = 120;
    }
}

void voice_gate_off(uint8_t vi)
{
    voice_t *v = &voices[vi];
    if (!v->active) return;
    v->held = 0;
    v->stage = 3;
    if (v->engine == ENG_RES && v->exc_left > 0x10000) v->exc_left = 0;
}

/* CHORD: one paraphonic voice -- three oscillators (root + two offsets in
 * MIDI*64 units) sharing one envelope, filter and output stage */
void voice_chord_on(uint8_t vi, uint8_t trk, int32_t pitch, int32_t off1, int32_t off2, uint8_t accent, uint8_t glide)
{
    voice_t *v = &voices[vi];
    voice_note_on(vi, trk, pitch, accent, glide);
    v->nosc = 3;
    v->coff[0] = off1;
    v->coff[1] = off2;
}

/* once per sequencer step: accent charge bleeds away (ETCH synth_step_tick) */
void voice_step_tick(void)
{
    uint32_t i;
    for (i = 0; i < N_MELODIC; i++) voices[i].acc_charge -= voices[i].acc_charge >> 3;
}

/* ---- CPU governor -----------------------------------------------------------
 * Audio continuity comes first.  If the previous frame used more than ~85 %
 * of the frame (or overran), free the least important sounding voice:
 * releasing tails first (quietest first), then TEXTURE, CHORD (top voice
 * first), DRONE, LEAD.  BASS and the drum lanes are never stolen.  The voice
 * fades over one block, so stealing is a soft cut, never a click. */
volatile uint16_t voices_stolen;
volatile uint8_t fx_send_active;           /* set when any voice feeds the delay this block */
volatile uint32_t vprof[7], vcount[5];
uint32_t vframe[N_MELODIC], vframe_eng[N_MELODIC];
uint32_t vlast[N_MELODIC];                    /* measured cycles of each voice in the last frame */
uint32_t drums_last, fixed_last;              /* measured, last frame */   /* profiling: control, engines x5, mix-out */
static inline uint32_t cyc(void) { return ((uint32_t)REG_TM3CNT_L << 16) | REG_TM2CNT_L; }
static const uint8_t steal_rank[TRK_COUNT] = { 0xFF, 4, 2, 3, 0xFF, 0xFF, 0xFF, 1 };

/* measured cost per frame at 264 samples (mGBA, cycles): control + mix-out
 * ~15.3k per voice, plus synthesis: SUB 23.6k (BASS 2x filter ~30k), FM
 * 11.9k, WT 17.3k, PD 17.0k, RES 20.7k; paraphonic chord oscillators add
 * ~5k (SUB/WT) or a full render each (FM/PD). */
#define SCALE(c) ((uint32_t)(c) * MIX_SPF / 264u)
static const uint32_t eng_cost[ENG_COUNT] = { SCALE(23600), SCALE(11900), SCALE(17300), SCALE(17000), SCALE(20700) };

uint32_t voice_cost(const voice_t *v) IWRAM_CODE;
uint32_t voice_cost(const voice_t *v)
{
    uint32_t c = SCALE(15300) + eng_cost[v->engine % ENG_COUNT];
    if (v->track == TRK_BASS && v->engine == ENG_SUB) c += SCALE(7000);
    if (v->nosc == 3) c += (v->engine == ENG_FM || v->engine == ENG_PD) ? 2 * eng_cost[v->engine] + SCALE(14000) : SCALE(6000);
    return c;
}

static int steal_one(void)
{
    uint32_t i, bscore = 0xFFFFFFFFu;
    int best = -1;
    for (i = 0; i < N_MELODIC; i++) {
        voice_t *v = &voices[i];
        uint32_t sc;
        if (!v->active || steal_rank[v->track] == 0xFF) continue;
        sc = v->stage == 3 ? (uint32_t)(v->amp >> 8) : 0x01000000u * steal_rank[v->track] + (uint32_t)(N_MELODIC - i);
        if (sc < bscore) { bscore = sc; best = (int)i; }
    }
    if (best < 0) return -1;
    voices[best].active = 0;
    voices[best].stage = 0;
    voices[best].held = 0;
    voices[best].stolen = 1;
    voices_stolen++;
    return best;
}

/* drum_cost: provided by percussion (sum over sounding lanes) */
uint32_t drums_cost(void) IWRAM_CODE;
volatile uint32_t gov_estimate;

void voice_govern(uint16_t lines) IWRAM_CODE;
void voice_govern(uint16_t lines)
{
    const uint32_t budget = 280896u * 82u / 100u;
    uint32_t dc = drums_cost(), i, c;
    uint32_t est = (fixed_last > SCALE(34000) ? fixed_last : SCALE(34000)) + SCALE(6000);
    est += drums_last > dc ? drums_last : dc;
    for (i = 0; i < N_MELODIC; i++)
        if (voices[i].active || voices[i].g1) {
            c = voice_cost(&voices[i]);
            est += vlast[i] > c ? vlast[i] : c;              /* self-calibrating */
        }
    gov_estimate = est;
    while (est > budget) {
        int b = steal_one();
        if (b < 0) break;
        c = voice_cost(&voices[b]);
        est -= vlast[b] > c ? vlast[b] : c;
        vlast[b] = 0;
    }
    /* backstop: measured overload of the previous frame (lines = 0: in-frame check) */
    if (lines >= 205) steal_one();
    if (lines >= 228) steal_one();
}

/* ---- control update ----------------------------------------------------- */
static __attribute__((noinline)) void voice_control(voice_t *v)
{
    const patch_t *pt = &proj.patch[v->track];
    int32_t p, tone, lfo = 0, a;
    uint32_t inc;
    uint8_t dest = pt->lfo_dest;

    /* glide */
    if (v->pitch != v->target) {
        int32_t d = v->target - v->pitch;
        int32_t st = (int32_t)(((int64_t)d * tab_glide[pt->p[P_GLIDE] & 15]) >> 16);
        if (st == 0) st = d > 0 ? 1 : -1;
        v->pitch += st;
    }
    /* LFO (free running per voice) */
    if (pt->lfo_depth) {
        uint32_t old = v->lfo_ph;
        v->lfo_ph += tab_lfo_inc[pt->lfo_rate * 2 + 1];
        switch (pt->lfo_shape) {
        case LFOS_TRI: { int32_t x = (int32_t)(v->lfo_ph >> 16) - 32768; v->lfo_val = (x < 0 ? -x : x) * 2 - 32768; break; }
        case LFOS_SQR: v->lfo_val = (v->lfo_ph & 0x80000000u) ? 32767 : -32767; break;
        case LFOS_SAW: v->lfo_val = 32767 - (int32_t)(v->lfo_ph >> 16); break;
        default: if (v->lfo_ph < old) v->lfo_val = (int32_t)(xs32(&v->rng) >> 16) - 32768; break;
        }
        lfo = (v->lfo_val * (int32_t)(pt->lfo_depth * pt->lfo_depth)) >> 8;   /* +-32767 * 225/256 */
    }
    /* envelopes */
    switch (v->stage) {
    case 1:
        v->amp += (int32_t)v->att_inc;
        if (v->amp >= Q24) { v->amp = Q24; v->stage = 2; }
        break;
    case 2:
        if (v->amp > v->sus) v->amp = v->sus + (int32_t)(((int64_t)(v->amp - v->sus) * v->dec_k) >> 16);
        break;
    case 3:
        v->amp = (int32_t)(((int64_t)v->amp * v->rel_k) >> 16);
        if (v->amp < (1 << 16)) { v->amp = 0; v->stage = 0; v->active = 0; }   /* -48 dB: done */
        break;
    }
    v->menv = (int32_t)(((int64_t)v->menv * v->menv_k) >> 16);
    v->aenv = (int32_t)(((int64_t)v->aenv * v->aenv_k) >> 16);

    /* the full parameter refresh runs every other block (and on new notes);
     * envelopes, glide and the gain ramp run every block */
    if (!v->retrig && !(v->ctl ^= 1)) goto gain;

    /* pitch */
    p = v->pitch + ((int32_t)pt->p[P_OCTAVE] - 2) * 768 + ((int32_t)pt->p[P_FINE] - 8) * 4;
    if (dest == LFO_TO_PITCH) p += (lfo * 381) >> 14;             /* up to ~+-12 semitones */
    inc = pitch_inc(p);

    /* tone (0..1023 internal) = TONE knob + env + accent + LFO + live drift */
    tone = pt->p[P_TONE] * 32
         + (int32_t)(((int64_t)v->menv * (pt->p[P_ENVMOD] * 40)) >> 24)
         + (int32_t)(((int64_t)v->aenv * (pt->p[P_ACCENT] * 24)) >> 24)
         + (v->acc_charge >> 1);
    if (dest == LFO_TO_TONE) tone += lfo >> 6;
    if (v->track == TRK_BASS) tone += evo_cut_ofs * 20;
    tone = clamp(tone, 0, 1023);

    switch (v->engine) {
    case ENG_SUB: {
        int32_t ci = 4 * 22 + ((tone * 420) >> 10);           /* quarter-semitone index */
        ci += ((p >> 6) - 48) * 2;                             /* 50 % keyboard tracking */
        ci = clamp(ci, 0, 4 * 136 - 1);
        v->shape = v->track == TRK_BASS;                       /* 2x oversampled filter on BASS */
        v->f = v->shape ? tab_svf_f[ci] : tab_svf1_f[ci];
        v->q = 46000 - pt->p[P_C] * 2900;                      /* Q15 damping 1.4 .. 0.08 */
        v->inc[0] = inc;
        if (pt->p[P_A] == 2) {                                 /* PULSE: width from B */
            v->inc[1] = inc;
            v->brk = 0x80000000u - (uint32_t)pt->p[P_B] * 0x07000000u;
        } else if (pt->p[P_B]) {                               /* detuned 2nd oscillator */
            v->inc[1] = inc + (inc >> 9) * pt->p[P_B];
        } else v->inc[1] = 0;
        v->drive = pt->p[P_DRIVE];
        break;
    }
    case ENG_FM: {
        v->inc[0] = (uint32_t)(((uint64_t)inc * fm_cratio[pt->p[P_C] & 3]) >> 8);   /* carrier */
        v->inc[1] = (uint32_t)(((uint64_t)inc * fm_ratio[pt->p[P_A] & 15]) >> 8);   /* modulator */
        v->idx = (tone * tone) >> 6;                                                 /* 0..16k */
        v->q = pt->p[P_B] * 600;                                                     /* feedback */
        v->drive = pt->p[P_DRIVE];
        break;
    }
    case ENG_WT: {
        v->inc[0] = inc;
        v->inc[1] = pt->p[P_B] ? inc + (inc >> 10) * pt->p[P_B] : 0;
        v->idx = (tone * 7 * 256) >> 10;                       /* frame position 0..1792 */
        v->f = 4000 + pt->p[P_C] * 1900;                       /* one-pole brightness, Q15 */
        v->brk = (uint32_t)((pt->p[P_A] & 7) * 8 * 3 * 256);   /* bank offset */
        a = p >> 6;
        v->shape = (uint8_t)(a < 52 ? 0 : a < 73 ? 1 : 2);     /* mip level */
        v->drive = pt->p[P_DRIVE];
        break;
    }
    case ENG_PD: {
        uint32_t d;
        v->inc[0] = inc;
        v->shape = pt->p[P_A] % 6;
        if (v->shape >= 3 && v->shape <= 4) {
            /* resonant: window x cosine at ratio 1..16 */
            v->inc[1] = (uint32_t)(((uint64_t)inc * (256 + (uint32_t)tone * 15 / 4)) >> 8);
        } else {
            uint32_t skew = (uint32_t)pt->p[P_B] * 0x01000000u;
            d = 0x80000000u - (uint32_t)tone * 0x001F0000u;     /* 0.5 .. ~0.02 */
            if (v->shape == 1 || v->shape == 5) d = (d >> 1);
            if (d < 0x01000000u) d = 0x01000000u;
            d += skew >> 3;
            v->brk = d;
            v->s1 = (int32_t)recip47(d);                                    /* Q16: 0.5/d */
            v->s2 = (int32_t)recip47(0u - d);                               /* Q16: 0.5/(1-d) */
        }
        v->drive = pt->p[P_DRIVE];
        break;
    }
    case ENG_RES: {
        uint32_t len;
        len = pitch_len(p);                                    /* fs / f in Q16 */
        if (len > ((DL_LEN - 4) << 16)) len = (DL_LEN - 4) << 16;
        if (len < (4 << 16)) len = 4 << 16;
        v->dlen = len;
        v->damp = 2000 + tone * 30;                            /* loop low-pass, Q15 */
        if (v->damp > 32767) v->damp = 32767;
        v->loopg = 32000 + pt->p[P_C] * 50;                    /* loop gain Q15 */
        if (v->held && pt->p[P_A] == 2) v->loopg = 32700;
        v->shape = pt->p[P_A] & 3;
        v->f = 3000 + tone * 28;                               /* exciter brightness */
        v->drive = pt->p[P_DRIVE];
        if (v->retrig) {
            /* inject the excitation over one delay period (BOWED: while held) */
            v->exc_left = (int32_t)(len >> 16) + 8;
            if (v->exc_left < 32) v->exc_left = 32;
            if (v->shape == 2) v->exc_left = 0x7FFFFFFF;
        }
        break;
    }
    }
    if (v->nosc == 3) {
        uint32_t c1 = pitch_inc(p + v->coff[0]), c2 = pitch_inc(p + v->coff[1]);
        switch (v->engine) {
        case ENG_SUB: case ENG_WT:
            v->inc[1] = 0; v->inc[2] = c1; v->inc[3] = c2;
            break;
        case ENG_FM:
            v->xinc[0][0] = (uint32_t)(((uint64_t)c1 * fm_cratio[pt->p[P_C] & 3]) >> 8);
            v->xinc[0][1] = (uint32_t)(((uint64_t)c1 * fm_ratio[pt->p[P_A] & 15]) >> 8);
            v->xinc[1][0] = (uint32_t)(((uint64_t)c2 * fm_cratio[pt->p[P_C] & 3]) >> 8);
            v->xinc[1][1] = (uint32_t)(((uint64_t)c2 * fm_ratio[pt->p[P_A] & 15]) >> 8);
            break;
        case ENG_PD:
            v->xinc[0][0] = c1; v->xinc[1][0] = c2;
            v->xinc[0][1] = (uint32_t)(((uint64_t)c1 * (256 + (uint32_t)tone * 15 / 4)) >> 8);
            v->xinc[1][1] = (uint32_t)(((uint64_t)c2 * (256 + (uint32_t)tone * 15 / 4)) >> 8);
            break;
        default: break;                                   /* RESONATOR: root only */
        }
    }
    v->retrig = 0;

gain:
    /* output gain: amp env x level x accent loudness */
    a = v->amp >> 9;                                           /* Q15 */
    a = (a * (pt->level + 1)) >> 4;
    if (v->accented && v->aenv > 0) a += (int32_t)(((int64_t)a * pt->p[P_ACCENT] * (v->aenv >> 12)) >> 18);
    if (dest == LFO_TO_AMP) a = (a * (32768 - ((lfo + 32768) >> 2))) >> 15;
    if (v->track == TRK_CHORD) a = (a * 3) >> 2;          /* three voices share the track */
    v->g0 = v->g1;
    v->g1 = a > 40000 ? 40000 : a;
    {
        int32_t pan = pt->pan * 256;                           /* 0..3840, centre 2048 */
        if (dest == LFO_TO_PAN) pan += lfo >> 4;
        pan = clamp(pan, 0, 4096);
        v->gl = 4096 - pan;
        v->gr = pan;
        if (v->gl > 2896) v->gl = 2896 + ((v->gl - 2896) >> 2);   /* rough constant power */
        if (v->gr > 2896) v->gr = 2896 + ((v->gr - 2896) >> 2);
        v->gs = pt->send * 180;
    }
    v->crush = pt->crush;
}

/* ---- per-sample renderers ------------------------------------------------- */
/* The loops below keep all state in locals and are specialised per variant
 * so that the inner loop has no branches; each renders MIX_BLOCK samples. */
static inline const int16_t *band(const int16_t *base, uint32_t inc)
{
    /* the band whose top fundamental is above this pitch */
    uint32_t k = 0, top = tab_exp_inc[0] >> 9;                 /* MIDI 12 */
    while (k < 10 && inc > top) { k++; top <<= 1; }
    return base + k * 1024;
}

static inline int32_t sat(int32_t x, int32_t lim) { return x > lim ? lim : x < -lim ? -lim : x; }

static __attribute__((noinline)) void drive_block(int32_t *out, int32_t drive)
{
    int n;
    int32_t g = 4 + drive * 3;
    for (n = 0; n < MIX_BLOCK; n++) {
        int32_t i = ((out[n] * g) >> 10) + 512;
        out[n] = tab_tanh[i < 0 ? 0 : i > 1023 ? 1023 : i];
    }
}

#define SVF1(s)                                              \
    do {                                                     \
        int32_t hp_ = (s) - lp - ((q * (bp >> 2)) >> 13);    \
        bp += (f * (sat(hp_, 200000) >> 2)) >> 13;           \
        lp += (f * (bp >> 2)) >> 13;                         \
        bp = sat(bp, 120000);                                \
    } while (0)

#define SVF2(s)                                              \
    do {                                                     \
        int32_t hp_ = (s) - lp - ((q * (bp >> 2)) >> 13);    \
        bp += (f * (sat(hp_, 200000) >> 2)) >> 13;           \
        lp += (f * (bp >> 2)) >> 13;                         \
        hp_ = (s) - lp - ((q * (bp >> 2)) >> 13);            \
        bp += (f * (sat(hp_, 200000) >> 2)) >> 13;           \
        lp += (f * (bp >> 2)) >> 13;                         \
        bp = sat(bp, 120000);                                \
    } while (0)

#define SUB_LOOPS(SVF)                                                         \
    if (w == 2) {                                                              \
        uint32_t brk = v->brk;                                                 \
        for (n = 0; n < MIX_BLOCK; n++) {                                      \
            int32_t s = t[ph0 >> 22] - t[(ph0 + brk) >> 22];                   \
            ph0 += i0; SVF(s); out[n] = lp;                                    \
        }                                                                      \
    } else if (i1) {                                                           \
        for (n = 0; n < MIX_BLOCK; n++) {                                      \
            int32_t s = (t[ph0 >> 22] + t[ph1 >> 22]) >> 1;                    \
            ph0 += i0; ph1 += i1; SVF(s); out[n] = lp;                         \
        }                                                                      \
    } else {                                                                   \
        for (n = 0; n < MIX_BLOCK; n++) {                                      \
            int32_t s = t[ph0 >> 22];                                          \
            ph0 += i0; SVF(s); out[n] = lp;                                    \
        }                                                                      \
    }

static __attribute__((noinline)) void render_sub(voice_t *v, int32_t *out)
{
    const patch_t *pt = &proj.patch[v->track];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1];
    int32_t lp = v->lp, bp = v->bp, f = v->f, q = v->q;
    uint8_t w = pt->p[P_A] & 3;
    const int16_t *t = band(w == 1 ? tab_bl_square : w == 3 ? tab_bl_tri : tab_bl_saw, i0);
    int n;
    if (v->nosc == 3) {
        uint32_t ph2 = v->ph[2], ph3 = v->ph[3], i2 = v->inc[2], i3 = v->inc[3];
        t = band(w == 1 ? tab_bl_square : w == 3 ? tab_bl_tri : tab_bl_saw, i3 > i2 ? i3 : i2);
        for (n = 0; n < MIX_BLOCK; n++) {
            int32_t s = (t[ph0 >> 22] + t[ph2 >> 22] + t[ph3 >> 22]) >> 1;
            ph0 += i0; ph2 += i2; ph3 += i3;
            SVF1(s);
            out[n] = lp;
        }
        v->ph[2] = ph2; v->ph[3] = ph3;
    } else if (v->shape) { SUB_LOOPS(SVF2) } else { SUB_LOOPS(SVF1) }
    lp = sat(lp, 131000);
    if (v->drive) drive_block(out, v->drive);
    v->ph[0] = ph0; v->ph[1] = ph1; v->lp = lp; v->bp = bp;
}

static __attribute__((noinline)) void render_fm(voice_t *v, int32_t *out)
{
    uint32_t pc = v->ph[0], pm = v->ph[1], ic = v->inc[0], im = v->inc[1];
    int32_t idx = v->idx, fb = v->q, y1 = v->fb1;
    const int16_t *sn = sine_iw;
    int n;
    for (n = 0; n < MIX_BLOCK; n++) {
        int32_t m = sn[(pm + ((uint32_t)(y1 * fb) << 3)) >> 22];
        y1 = m;
        out[n] = sn[(pc + ((uint32_t)((m >> 3) * idx) << 6)) >> 22];
        pc += ic; pm += im;
    }
    if (v->drive) drive_block(out, v->drive);
    v->ph[0] = pc; v->ph[1] = pm; v->fb1 = y1;
}

static __attribute__((noinline)) void render_wt(voice_t *v, int32_t *out)
{
    const int8_t *base = tab_wt + v->brk;
    uint32_t pos = (uint32_t)v->idx, fr = pos >> 8;
    int32_t mix = (int32_t)(pos & 255);
    const int8_t *t1, *t2;
    uint32_t p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1];
    int32_t lp = v->lp, k = v->f;
    int n;
    if (fr > 6) { fr = 6; mix = 255; }
    t1 = base + (fr * 3 + v->shape) * 256;
    t2 = base + ((fr + 1) * 3 + v->shape) * 256;
    if (v->nosc == 3) {
        uint32_t p2 = v->ph[2], p3 = v->ph[3], i2 = v->inc[2], i3 = v->inc[3];
        for (n = 0; n < MIX_BLOCK; n++) {
            uint32_t j = p0 >> 24, j2 = p2 >> 24, j3 = p3 >> 24;
            int32_t a = t1[j] + t1[j2] + t1[j3], b = t2[j] + t2[j2] + t2[j3];
            int32_t s = (a << 7) + (((b - a) * mix) >> 1);
            p0 += i0; p2 += i2; p3 += i3;
            lp += ((s - lp) * k) >> 15;
            out[n] = lp;
        }
        v->ph[2] = p2; v->ph[3] = p3;
    } else if (i1) {
        for (n = 0; n < MIX_BLOCK; n++) {
            uint32_t j = p0 >> 24, j2 = p1 >> 24;
            int32_t a = t1[j] + t1[j2], b = t2[j] + t2[j2];
            int32_t s = (a << 7) + (((b - a) * mix) >> 1);
            p0 += i0; p1 += i1;
            lp += ((s - lp) * k) >> 15;
            out[n] = lp;
        }
    } else {
        for (n = 0; n < MIX_BLOCK; n++) {
            uint32_t j = p0 >> 24;
            int32_t a = t1[j], b = t2[j];
            int32_t s = (a << 8) + (b - a) * mix;
            p0 += i0;
            lp += ((s - lp) * k) >> 15;
            out[n] = lp;
        }
    }
    if (v->drive) drive_block(out, v->drive);
    v->ph[0] = p0; v->ph[1] = p1; v->lp = lp;
}

static __attribute__((noinline)) void render_pd(voice_t *v, int32_t *out)
{
    uint32_t ph = v->ph[0], inc = v->inc[0], d = v->brk;
    uint32_t s1 = (uint32_t)v->s1, s2 = (uint32_t)v->s2;
    int32_t dc = v->dc;
    const int16_t *sn = sine_iw;
    uint8_t shape = v->shape;
    int n;
    if (shape == 3 || shape == 4) {
        /* resonant: cosine at the moving ratio, windowed by saw / triangle,
         * hard-synced to the fundamental */
        uint32_t rp = v->ph[1], ri = v->inc[1];
        for (n = 0; n < MIX_BLOCK; n++) {
            uint32_t win = shape == 3 ? (~ph) >> 17 : ((ph < 0x80000000u ? ph : ~ph) >> 16);
            int32_t y = ((sn[(rp + 0x40000000u) >> 22] + 32767) * (int32_t)win >> 15) - 16384;
            rp += ri;
            if (ph + inc < ph) rp = 0;
            ph += inc;
            dc += (y - dc) >> 8;
            out[n] = y - dc;
        }
        v->ph[1] = rp;
    } else {
        /* warped phase: two linear segments meeting at the break point */
        for (n = 0; n < MIX_BLOCK; n++) {
            uint32_t x = shape == 1 ? ph << 1 : ph, w;
            if (x < d) w = (x >> 16) * s1;
            else w = 0x80000000u + ((x - d) >> 16) * s2;
            if (shape == 1) w = (ph < 0x80000000u) ? (w >> 1) : 0x80000000u + (w >> 1);
            else if (shape == 2) w += w >> 1;
            else if (shape == 5) w <<= 1;
            ph += inc;
            out[n] = -sn[(w + 0x40000000u) >> 22];
        }
    }
    if (v->drive) drive_block(out, v->drive);
    v->ph[0] = ph; v->dc = dc;
}

static __attribute__((noinline)) void render_res(voice_t *v, int32_t *out)
{
    int16_t *dl = v->dl;
    uint32_t pos = v->dpos, len = v->dlen;
    int32_t lp = v->lp, damp = v->damp, g = v->loopg, ex = v->exc_lp, kf = v->f;
    int32_t left = v->exc_left;
    uint32_t rng = v->rng;
    uint8_t shape = v->shape;
    int n;
    for (n = 0; n < MIX_BLOCK; n++) {
        uint32_t rp = (pos << 16) - len;
        uint32_t ri = (rp >> 16) & (DL_LEN - 1), fr = (rp >> 1) & 0x7FFF;
        int32_t a = dl[ri], b = dl[(ri + 1) & (DL_LEN - 1)], y;
        y = a + (((b - a) * (int32_t)fr) >> 15);
        lp += ((y - lp) * damp) >> 15;
        y = (lp * g) >> 15;
        if (left > 0) {
            int32_t nz;
            left--;
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            nz = (int32_t)(rng >> 17) - 16384;
            if (shape == 1) nz = (left & 16) ? 12000 : -12000;          /* MALLET */
            else if (shape == 2) nz >>= 3;                              /* BOWED */
            else if (shape == 3) nz = (nz * (left & 3)) >> 1;           /* GRAIN */
            ex += ((nz - ex) * kf) >> 15;
            y += ex;
        }
        y = sat(y, 32000);
        dl[pos & (DL_LEN - 1)] = (int16_t)y;
        pos++;
        out[n] = y;
    }
    if (v->drive) drive_block(out, v->drive);
    v->dpos = pos & (DL_LEN - 1); v->lp = lp; v->exc_lp = ex; v->exc_left = left; v->rng = rng;
}

/* gain ramp + pan + send into the interleaved [L, R, SEND] accumulators
 * (sample x 1024 domain), via the assembly loops in mixer/mixout.s */
void mix_out3(const int32_t *src, int32_t *acc, const int32_t *g) IWRAM_CODE;
void mix_out2(const int32_t *src, int32_t *acc, const int32_t *g) IWRAM_CODE;
void voice_out(const int32_t *tmp, int32_t *acc, int32_t g0, int32_t g1, int32_t gl, int32_t gr, int32_t gs) IWRAM_CODE;
void voice_out(const int32_t *tmp, int32_t *acc, int32_t g0, int32_t g1, int32_t gl, int32_t gr, int32_t gs)
{
    int32_t g[6];
    int32_t l0 = (g0 * gl) >> 15, r0 = (g0 * gr) >> 15, s0 = (g0 * gs) >> 15;
    g[0] = l0; g[1] = r0; g[2] = s0;
    g[3] = (((g1 * gl) >> 15) - l0) / MIX_BLOCK;
    g[4] = (((g1 * gr) >> 15) - r0) / MIX_BLOCK;
    g[5] = (((g1 * gs) >> 15) - s0) / MIX_BLOCK;
    if (gs) mix_out3(tmp, acc, g); else mix_out2(tmp, acc, g);
}

void crush_block(int32_t *out, uint8_t crush, uint8_t *ctr, int16_t *hold) IWRAM_CODE;
void crush_block(int32_t *out, uint8_t crush, uint8_t *ctr, int16_t *hold)
{
    int n;
    uint8_t c = *ctr;
    int32_t h = *hold, mask = ~((1 << (crush + 8)) - 1);
    for (n = 0; n < MIX_BLOCK; n++) {
        if (c == 0) { h = sat(out[n], 32767) & mask; c = crush; } else c--;
        out[n] = h;
    }
    *ctr = c; *hold = (int16_t)h;
}

/* Render all melodic voices for one block and accumulate into L/R/send. */
void voices_render(int32_t *acc) IWRAM_CODE;
void voices_render(int32_t *acc)
{
    static int32_t tmp[MIX_BLOCK];
    uint32_t vi, t0 = cyc(), t1;
    for (vi = 0; vi < N_MELODIC; vi++) {
        voice_t *v = &voices[vi];
        uint32_t tv = cyc();
        t0 = tv;
        if (!v->active && v->g1 == 0) continue;
        if (proj.mutes & (1u << v->track)) {                   /* muted: fade out, then sleep */
            if (v->g1 == 0) continue;
            v->g0 = v->g1; v->g1 = 0;
        } else if (v->active) voice_control(v);
        else { v->g0 = v->g1; v->g1 = 0; }
        if (v->g0 == 0 && v->g1 == 0) continue;
        t1 = cyc(); vprof[0] += t1 - t0; t0 = t1;
        switch (v->engine) {
        case ENG_FM:  render_fm(v, tmp); break;
        case ENG_WT:  render_wt(v, tmp); break;
        case ENG_PD:  render_pd(v, tmp); break;
        case ENG_RES: render_res(v, tmp); break;
        default:      render_sub(v, tmp); break;
        }
        if (v->nosc == 3 && (v->engine == ENG_FM || v->engine == ENG_PD)) {
            /* paraphonic FM / PD: render the other two oscillators with their
             * own phases and sum (shared envelope, control and output) */
            static int32_t tmp2[MIX_BLOCK];
            uint32_t k, sv0, sv1, si0, si1;
            int n;
            for (k = 0; k < 2; k++) {
                sv0 = v->ph[0]; sv1 = v->ph[1]; si0 = v->inc[0]; si1 = v->inc[1];
                v->ph[0] = v->xph[k][0]; v->ph[1] = v->xph[k][1];
                v->inc[0] = v->xinc[k][0]; v->inc[1] = v->xinc[k][1];
                if (v->engine == ENG_FM) render_fm(v, tmp2); else render_pd(v, tmp2);
                v->xph[k][0] = v->ph[0]; v->xph[k][1] = v->ph[1];
                v->ph[0] = sv0; v->ph[1] = sv1; v->inc[0] = si0; v->inc[1] = si1;
                for (n = 0; n < MIX_BLOCK; n++) tmp[n] += tmp2[n];
            }
            for (n = 0; n < MIX_BLOCK; n++) tmp[n] >>= 1;
        }
        t1 = cyc(); vprof[1 + v->engine] += t1 - t0; vcount[v->engine]++; t0 = t1;
        if (v->crush) crush_block(tmp, v->crush, &v->crush_ctr, &v->crush_hold);
        if (v->gs) fx_send_active = 1;
        voice_out(tmp, acc, v->g0, v->g1, v->gl, v->gr, v->gs);
        t1 = cyc(); vprof[6] += t1 - t0; t0 = t1;
        vframe[vi] += t1 - tv; vframe_eng[vi] = (uint32_t)v->engine | ((uint32_t)v->nosc << 8) | ((uint32_t)v->track << 16);
    }
}
