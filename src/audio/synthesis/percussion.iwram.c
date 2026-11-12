/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Procedural percussion.  No samples: every hit is built from
 *   - a sine tone with an exponential pitch envelope (kick, toms, blips),
 *   - white noise through a state-variable filter (snare, clap, hats),
 *   - a bank of six square oscillators at inharmonic ratios (metal, hats),
 *   - a click transient, and clap re-triggering.
 *
 * Each ORIGINAL ETCH drum lane keeps its four kit numbers (CORROSION's
 * grammars choose kits per style), so a kit index means the same kind of
 * sound on ETCH and ETCH II:
 *   KICK  DEEP  PUNCH THUD  CLICK     SNARE SHORT GRIT  CLAP  RIM
 *   CH    CLOSD TICK  SHAKE SHARP     OH    OPEN  METAL BLIP  CRASH
 * The track patch adds a VOICING (ANALOG / FM / DIGITAL), tone, level,
 * pan, delay send and crush; per-lane TUNE / DECAY / LEVEL follow ETCH.
 */
#include "audio.h"
#include "seq.h"

drumvoice_t dvoices[N_DRUMS];
int8_t evo_tune_ofs[NUM_LANES];
extern int16_t sine_iw[1024];

#define Q24 (1 << 24)
#define HZ(x) ((uint32_t)((x) * (4294967296.0 / MIX_RATE)))

typedef struct {
    uint16_t t_start, t_end;   /* tone Hz */
    uint8_t  t_pdec, t_dec;    /* decay table indices (pitch, amp) */
    uint8_t  t_lvl;            /* 0..255 */
    uint8_t  n_mode;           /* 0 off, 1 LP, 2 BP, 3 HP */
    uint16_t n_cut;            /* quarter-semitone index of the filter */
    uint8_t  n_q;              /* 0..15 */
    uint8_t  n_dec, n_lvl;
    uint16_t m_base;           /* metal base Hz, 0 = off */
    uint8_t  m_dec, m_lvl;
    uint8_t  c_lvl, c_dec;     /* click */
    uint8_t  claps;            /* clap re-triggers */
    uint8_t  pad;
} dmodel_t;

/* idx -> tau: 8 ~ 17 ms, 10 ~ 30 ms, 12 ~ 50 ms, 14 ~ 85 ms, 16 ~ 145 ms,
 *             18 ~ 250 ms, 20 ~ 420 ms, 22 ~ 720 ms, 24 ~ 1.2 s */
static const dmodel_t models[16] = {
    /* KICK */
    { 160,  44, 11, 19, 255, 0,   0, 0,  0,   0,    0,  0,  0,  50, 4, 0, 0 },   /* DEEP  */
    { 330,  52,  8, 15, 255, 1, 400, 2,  6,  50,    0,  0,  0, 160, 5, 0, 0 },   /* PUNCH */
    { 130,  62,  9, 13, 230, 1, 320, 0, 10,  90,    0,  0,  0,  40, 6, 0, 0 },   /* THUD  */
    { 240,  58,  6, 12, 220, 3, 480, 4,  4, 140,    0,  0,  0, 255, 3, 0, 0 },   /* CLICK */
    /* SNARE */
    { 220, 170,  8, 11, 160, 2, 420, 3, 12, 200,    0,  0,  0,  90, 4, 0, 0 },   /* SHORT */
    { 190, 150,  9, 13, 130, 3, 400, 2, 15, 220,    0,  0,  0, 120, 4, 0, 0 },   /* GRIT  */
    {   0,   0,  0,  0,   0, 2, 380, 6, 15, 255,    0,  0,  0,   0, 0, 3, 0 },   /* CLAP  */
    { 520, 480,  6,  8, 210, 3, 470, 4,  6,  60,  560,  7, 80, 120, 3, 0, 0 },   /* RIM   */
    /* CLOSED HAT */
    {   0,   0,  0,  0,   0, 3, 470, 2,  9, 110,  330, 10,150,   0, 0, 0, 0 },   /* CLOSD */
    {   0,   0,  0,  0,   0, 3, 500, 0,  6, 200,    0,  0,  0, 200, 2, 0, 0 },   /* TICK  */
    {   0,   0,  0,  0,   0, 2, 470, 5, 12, 170,    0,  0,  0,   0, 0, 0, 0 },   /* SHAKE */
    {   0,   0,  0,  0,   0, 3, 500, 3,  8, 160,  410,  9,200,  80, 2, 0, 0 },   /* SHARP */
    /* OPEN HAT */
    {   0,   0,  0,  0,   0, 3, 470, 2, 18, 110,  330, 19,150,   0, 0, 0, 0 },   /* OPEN  */
    { 680, 660, 20, 18, 110, 2, 480, 9, 14,  60,  470, 21,190,   0, 0, 0, 0 },   /* METAL */
    { 480, 880,  9, 13, 230, 0,   0, 0,  0,   0,    0,  0,  0,  60, 3, 0, 0 },   /* BLIP  */
    {   0,   0,  0,  0,   0, 3, 440, 1, 23, 160,  290, 22,120,  40, 5, 0, 0 },   /* CRASH */
};
/* three inharmonic ratios (Q8) for the XOR metal bank */
static const uint16_t metal_ratio[3] = { 256, 380, 461 };

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint8_t lane_track(uint8_t lane) { return lane == 0 ? TRK_KICK : lane == 1 ? TRK_PERC : TRK_HATS; }
static uint16_t dk(int32_t idx, int32_t ofs) { return tab_env_decay[clamp(idx + ofs, 0, 31)]; }

/* ---- synthesis core (shared by the cache renderer and the fallback) ----- */
static void dsynth_setup(dsynth_t *d, uint8_t lane, uint8_t kit, int8_t tune_ofs)
{
    const dmodel_t *m = &models[(lane & 3) * 4 + (kit & 3)];
    const patch_t *pt = &proj.patch[lane_track(lane)];
    int32_t tune = (int32_t)proj.drum_tune[lane] - 8 + tune_ofs + ((int32_t)pt->p[P_OCTAVE] - 2) * 4;
    int32_t dofs = ((int32_t)proj.drum_decay[lane] - 8) / 2 + ((int32_t)pt->p[P_DECAY] - 8) / 3;
    int32_t tmul = 256 + tune * 16;
    int32_t tone = (int32_t)pt->p[P_TONE] - 16;
    uint32_t i, seed = d->rng ? d->rng : 0x2545F491u;
    if (tmul < 96) tmul = 96;
    memset(d, 0, sizeof *d);
    d->rng = seed;
    d->tinc = HZ(1) * ((m->t_start * (uint32_t)tmul) >> 8);
    d->tinc_end = HZ(1) * ((m->t_end * (uint32_t)tmul) >> 8);
    d->tpk = dk(m->t_pdec, 0);
    d->tamp = m->t_lvl ? Q24 : 0;
    d->tdec = dk(m->t_dec, dofs);
    d->tlvl = m->t_lvl * 128;
    d->tph = 0x40000000u;                                      /* start at the peak: punch */
    d->nmode = m->n_mode;
    d->namp = m->n_mode ? Q24 : 0;
    d->ndec = dk(m->n_dec, dofs);
    d->nlvl = m->n_lvl * 128;
    {
        int32_t ci = m->n_cut + tune * 4 + tone * 6;
        ci = clamp(ci, 40, 4 * 136 - 1);
        d->nf = clamp((int32_t)tab_svf_f[ci] * 2, 200, 30000);
        d->nq = 40000 - m->n_q * 2300;
    }
    d->mamp = m->m_base ? Q24 : 0;
    d->mdec = dk(m->m_dec, dofs);
    d->mlvl = m->m_lvl * 128;
    for (i = 0; i < 3; i++) {
        d->minc[i] = HZ(1) * (((uint32_t)m->m_base * metal_ratio[i] >> 8) * (uint32_t)tmul >> 8);
        d->rng ^= d->rng << 13; d->rng ^= d->rng >> 17; d->rng ^= d->rng << 5;
        d->mph[i] = d->rng;
    }
    d->camp = m->c_lvl ? Q24 : 0;
    d->cdec = tab_env_decay[m->c_dec];
    d->claps = m->claps;
    d->clap_gap = (int32_t)(MIX_RATE / 90);                    /* ~11 ms between claps */
    d->clap_ctr = d->clap_gap;
    d->drive = pt->engine == 2 ? 5 : pt->p[P_DRIVE];
    d->fm = pt->engine == 1;
}

/* Render one block of a hit into tmp (overwrites).  Returns 0 when silent. */
static int dsynth_block(dsynth_t *d, int32_t *tmp)
{
    int32_t n, ta, na, ma, ca;
    d->tinc = d->tinc_end + (uint32_t)(((uint64_t)(d->tinc - d->tinc_end) * (uint32_t)d->tpk) >> 16);
    d->tamp = (int32_t)(((int64_t)d->tamp * d->tdec) >> 16);
    d->namp = (int32_t)(((int64_t)d->namp * d->ndec) >> 16);
    d->mamp = (int32_t)(((int64_t)d->mamp * d->mdec) >> 16);
    d->camp = (int32_t)(((int64_t)d->camp * d->cdec) >> 16);
    if (d->tamp < (1 << 16) && d->namp < (1 << 16) && d->mamp < (1 << 16) && d->camp < (1 << 16) && d->claps == 0) return 0;
    ta = (d->tamp >> 9) * d->tlvl >> 15;
    na = (d->namp >> 9) * d->nlvl >> 15;
    ma = (d->mamp >> 9) * d->mlvl >> 15;
    ca = d->camp >> 9;
    for (n = 0; n < MIX_BLOCK; n++) tmp[n] = 0;
    if (ta) {
        uint32_t ph = d->tph, inc = d->tinc;
        if (d->fm) {                                            /* FM voicing: 1.41x modulator */
            uint32_t mp = d->mph[3], mi = inc + (inc >> 1) - (inc >> 4);
            for (n = 0; n < MIX_BLOCK; n++) {
                tmp[n] = (sine_iw[(ph + (uint32_t)(sine_iw[mp >> 22] * 9000)) >> 22] * ta) >> 15;
                ph += inc; mp += mi;
            }
            d->mph[3] = mp;
        } else {
            for (n = 0; n < MIX_BLOCK; n++) { tmp[n] = (sine_iw[ph >> 22] * ta) >> 15; ph += inc; }
        }
        d->tph = ph;
    }
    if (na || ca) {
        uint32_t r = d->rng;
        int32_t lp = d->nlp, bp = d->nbp, f = d->nf, q = d->nq, mode = d->nmode;
        if (d->claps) {
            d->clap_ctr -= MIX_BLOCK;
            if (d->clap_ctr <= 0) { d->claps--; d->clap_ctr = d->clap_gap; d->namp = Q24; }
        }
#define NOISE_LOOP(OUT)                                                     \
        for (n = 0; n < MIX_BLOCK; n++) {                                   \
            int32_t x, hp;                                                  \
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;                        \
            x = (int32_t)(r >> 16) - 32768;                                 \
            hp = x - lp - ((q * (bp >> 1)) >> 14);                          \
            bp += (f * (hp >> 1)) >> 14;                                    \
            lp += (f * (bp >> 1)) >> 14;                                    \
            tmp[n] += (((OUT) * na) >> 15) + ((x * ca) >> 15);              \
        }
        if (mode == 1) { NOISE_LOOP(lp) }
        else if (mode == 2) { NOISE_LOOP(bp) }
        else { NOISE_LOOP(hp) }
        d->rng = r;
        d->nlp = clamp(lp, -90000, 90000);
        d->nbp = clamp(bp, -90000, 90000);
    }
    if (ma) {
        uint32_t p0 = d->mph[0], p1 = d->mph[1], p2 = d->mph[2];
        uint32_t i0 = d->minc[0], i1 = d->minc[1], i2 = d->minc[2];
        int32_t hpst = d->mhp;
        for (n = 0; n < MIX_BLOCK; n++) {
            int32_t mm = ((int32_t)((p0 ^ p1 ^ p2) >> 31) * 2 - 1) * 24000;
            p0 += i0; p1 += i1; p2 += i2;
            hpst += (mm - hpst) >> 2;
            tmp[n] += ((mm - hpst) * ma) >> 15;
        }
        d->mph[0] = p0; d->mph[1] = p1; d->mph[2] = p2; d->mhp = hpst;
    }
    if (d->drive) {
        int32_t gdr = 4 + d->drive * 3;
        for (n = 0; n < MIX_BLOCK; n++) {
            int32_t i = ((tmp[n] * gdr) >> 10) + 512;
            tmp[n] = tab_tanh[clamp(i, 0, 1023)];
        }
    }
    return 1;
}

/* ---- render cache ---------------------------------------------------------- */
static const uint16_t cache_max[NUM_LANES] = { 9216, 8192, 4608, 14336 };   /* samples (~0.5/0.45/0.25/0.8 s) */
static int8_t cache_mem[2 * (9216 + 8192 + 4608 + 14336)] EWRAM_BSS;
static int16_t render_scratch[14336] EWRAM_BSS;
typedef struct {
    int8_t *buf[2];
    uint16_t len[2];
    int32_t scale[2];
    uint32_t key[2];
    uint8_t kit[2];
    volatile uint8_t front;
} dcache_t;
static dcache_t cache[NUM_LANES];
static volatile uint32_t want_key[NUM_LANES][4];
volatile uint16_t drum_cache_renders, drum_rt_hits;

static uint32_t drum_key(uint8_t lane, uint8_t kit)
{
    const patch_t *pt = &proj.patch[lane_track(lane)];
    uint32_t h = 0x811C9DC5u;
    uint8_t b[9], i;
    b[0] = kit & 3; b[1] = pt->engine; b[2] = proj.drum_tune[lane]; b[3] = proj.drum_decay[lane];
    b[4] = pt->p[P_TONE]; b[5] = pt->p[P_DECAY]; b[6] = pt->p[P_OCTAVE]; b[7] = pt->p[P_DRIVE];
    b[8] = (uint8_t)evo_tune_ofs[lane];
    for (i = 0; i < 9; i++) { h ^= b[i]; h *= 0x01000193u; }
    return h | 1;                                               /* 0 = empty */
}

static void cache_init(void)
{
    int8_t *m = cache_mem;
    uint8_t l;
    for (l = 0; l < NUM_LANES; l++) {
        cache[l].buf[0] = m; m += cache_max[l];
        cache[l].buf[1] = m; m += cache_max[l];
        cache[l].key[0] = cache[l].key[1] = 0;
        cache[l].front = 0;
    }
}

/* background renderer state (main loop) */
static struct {
    uint8_t busy, lane, kit, back;
    uint32_t key, pos;
    int32_t peak;
    dsynth_t syn;
} rj;

/* Called once per main-loop pass: refresh the wanted keys, then advance the
 * render job by a bounded amount of work (~48 blocks). */
void drum_cache_service(void)
{
    static int32_t tmp[MIX_BLOCK];
    uint8_t l, k, nb = 0;
    for (l = 0; l < NUM_LANES; l++) for (k = 0; k < 4; k++) want_key[l][k] = drum_key(l, k);
    if (!rj.busy) {
        /* the kits the current and the next bar actually use */
        uint8_t pats[2], pi;
        pats[0] = seq_cur;
        pats[1] = seq_next_bar_pattern();
        for (pi = 0; pi < 2 && !rj.busy; pi++)
            for (l = 0; l < NUM_LANES && !rj.busy; l++) {
                dcache_t *c = &cache[l];
                uint8_t kit = proj.pat[pats[pi]].kit[l] & 3, back = (uint8_t)(c->front ^ 1);
                uint32_t key = want_key[l][kit];
                if (c->key[c->front] == key) continue;
                /* never overwrite a buffer a sounding voice still reads */
                if (dvoices[l].active && dvoices[l].cached && dvoices[l].buf == c->buf[back]) continue;
                rj.busy = 1; rj.lane = l; rj.kit = kit; rj.back = back; rj.key = key; rj.pos = 0; rj.peak = 1;
                rj.syn.rng = 0x2545F491u ^ key;
                dsynth_setup(&rj.syn, l, kit, evo_tune_ofs[l]);
            }
        if (!rj.busy) return;
    }
    while (rj.busy && nb++ < 48) {
        int n, alive = 0;
        if (rj.pos + MIX_BLOCK <= cache_max[rj.lane]) alive = dsynth_block(&rj.syn, tmp);
        if (alive) {
            for (n = 0; n < MIX_BLOCK; n++) {
                int32_t x = clamp(tmp[n], -32767, 32767), a = x < 0 ? -x : x;
                if (a > rj.peak) rj.peak = a;
                render_scratch[rj.pos + n] = (int16_t)x;
            }
            rj.pos += MIX_BLOCK;
        } else {
            /* finished: normalise into the back buffer, then publish */
            dcache_t *c = &cache[rj.lane];
            int8_t *dst = c->buf[rj.back];
            int32_t sh = 0, i;
            while ((rj.peak >> sh) > 127) sh++;
            for (i = 0; i < (int32_t)rj.pos; i++) dst[i] = (int8_t)(render_scratch[i] >> sh);
            c->len[rj.back] = (uint16_t)rj.pos;
            c->scale[rj.back] = 1 << sh;
            c->key[rj.back] = rj.key;
            c->kit[rj.back] = rj.kit;
            c->front = rj.back;
            drum_cache_renders++;
            rj.busy = 0;
        }
    }
}

/* ---- playback (audio IRQ) -------------------------------------------------- */
void drums_choke(uint8_t lane)
{
    drumvoice_t *d = &dvoices[lane];
    if (!d->active) return;
    if (d->cached) { if (d->len > d->pos + 300) d->len = d->pos + 300; }   /* fast cut */
    else { d->rt.tdec = d->rt.ndec = d->rt.mdec = tab_env_decay[6]; }
}

void drum_trigger(uint8_t lane, uint8_t kit, uint8_t accent)
{
    drumvoice_t *d = &dvoices[lane & 3];
    dcache_t *c = &cache[lane & 3];
    uint8_t f = c->front;
    (void)accent;
    d->lane = lane;
    d->active = 1;
    /* a render of this kit is played even if a parameter has just moved
     * (the fresh render follows within a few frames) */
    if (c->key[f] && (c->key[f] == want_key[lane][kit & 3] || c->kit[f] == (kit & 3))) {
        d->cached = 1;
        d->buf = c->buf[f];
        d->len = c->len[f];
        d->scale = c->scale[f];
        d->pos = 0;
    } else {
        /* no fresh render yet (just edited / new kit): synthesise live */
        d->cached = 0;
        dsynth_setup(&d->rt, lane, kit, evo_tune_ofs[lane]);
        drum_rt_hits++;
    }
}

static void drum_gains(drumvoice_t *d)
{
    const patch_t *pt = &proj.patch[lane_track(d->lane)];
    int32_t a = (int32_t)(proj.drum_level[d->lane] + 1) * (pt->level + 1);    /* 1..256 */
    int32_t pan = pt->pan * 256;
    d->g0 = d->g1;
    d->g1 = d->active ? (a * 128) : 0;                                         /* Q15 */
    if (d->lane == 3) pan += 512;                                               /* open hat a little right */
    if (d->lane == 2) pan -= 384;
    pan = clamp(pan, 0, 4096);
    d->gl = 4096 - pan; d->gr = pan;
    if (d->gl > 2896) d->gl = 2896 + ((d->gl - 2896) >> 2);
    if (d->gr > 2896) d->gr = 2896 + ((d->gr - 2896) >> 2);
    d->gs = pt->send * 180;
    d->crush = pt->engine == 2 ? (uint8_t)(pt->crush > 3 ? pt->crush : 3) : pt->crush;
}

void voice_out(const int32_t *tmp, int32_t *acc, int32_t g0, int32_t g1, int32_t gl, int32_t gr, int32_t gs) IWRAM_CODE;
void mix_out_s8(const int8_t *src, int32_t *acc, const int32_t *g, int32_t unused) IWRAM_CODE;
extern volatile uint8_t fx_send_active;
void crush_block(int32_t *out, uint8_t crush, uint8_t *ctr, int16_t *hold) IWRAM_CODE;

void drums_render(int32_t *acc) IWRAM_CODE;
void drums_render(int32_t *acc)
{
    static int32_t tmp[MIX_BLOCK];
    uint32_t li;
    for (li = 0; li < N_DRUMS; li++) {
        drumvoice_t *d = &dvoices[li];
        int n;
        if (!d->active && d->g1 == 0) continue;
        if (proj.mutes & (1u << lane_track((uint8_t)li))) { d->active = 0; if (d->g1 == 0) continue; }
        if (d->active && d->cached && d->crush == 0) {
            /* cached render: mixed straight from the int8 buffer (assembly) */
            if (d->pos + MIX_BLOCK > d->len) { d->active = 0; drum_gains(d); d->g0 = d->g1 = 0; continue; }
            drum_gains(d);
            if (proj.mutes & (1u << lane_track((uint8_t)li))) d->g1 = 0;
            {
                int32_t g[6], sc = d->scale << 2;          /* int8 -> the >>2 domain of mix_out */
                int32_t l0 = (d->g0 * d->gl) >> 15, r0 = (d->g0 * d->gr) >> 15, s0 = (d->g0 * d->gs) >> 15;
                g[0] = l0 * sc >> 2; g[1] = r0 * sc >> 2; g[2] = s0 * sc >> 2;
                g[3] = ((((d->g1 * d->gl) >> 15) - l0) * sc >> 2) / MIX_BLOCK;
                g[4] = ((((d->g1 * d->gr) >> 15) - r0) * sc >> 2) / MIX_BLOCK;
                g[5] = ((((d->g1 * d->gs) >> 15) - s0) * sc >> 2) / MIX_BLOCK;
                if (d->gs) fx_send_active = 1;
                mix_out_s8(d->buf + d->pos, acc, g, 0);
            }
            d->pos += MIX_BLOCK;
            continue;
        }
        if (d->active) {
            if (d->cached) {
                const int8_t *b = d->buf + d->pos;
                int32_t sc = d->scale;
                if (d->pos + MIX_BLOCK > d->len) { d->active = 0; for (n = 0; n < MIX_BLOCK; n++) tmp[n] = 0; }
                else for (n = 0; n < MIX_BLOCK; n++) tmp[n] = b[n] * sc;
                d->pos += MIX_BLOCK;
            } else if (!dsynth_block(&d->rt, tmp)) {
                d->active = 0;
                for (n = 0; n < MIX_BLOCK; n++) tmp[n] = 0;
            }
        } else for (n = 0; n < MIX_BLOCK; n++) tmp[n] = 0;
        drum_gains(d);
        if (proj.mutes & (1u << lane_track((uint8_t)li))) d->g1 = 0;
        if (d->g0 == 0 && d->g1 == 0) continue;
        if (d->crush) crush_block(tmp, d->crush, &d->crush_ctr, &d->crush_hold);
        if (d->gs) fx_send_active = 1;
        voice_out(tmp, acc, d->g0, d->g1, d->gl, d->gr, d->gs);
    }
}

uint32_t drums_cost(void) IWRAM_CODE;
uint32_t drums_cost(void)
{
    uint32_t c = 0, l;
    for (l = 0; l < N_DRUMS; l++) {
        const drumvoice_t *d = &dvoices[l];
        if (!d->active && !d->g1) continue;
        c += d->cached ? (d->crush ? 9000u : 5000u) : 25000u;
    }
    return c * MIX_SPF / 264u;
}

void drums_init(void) IWRAM_CODE;
void drums_init(void)
{
    memset(dvoices, 0, sizeof dvoices);
    memset(&rj, 0, sizeof rj);
    cache_init();
}
