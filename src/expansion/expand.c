/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * EXPANSION generator (ETCH II only; see include/expand.h).
 *
 * Musical approach:
 *   DRONE    the tonal centre (or its fifth) held through the bar
 *   CHORD    a short progression of scale-degree chords whose rhythm depends
 *            on the STYLE (dub offbeat stabs, held pads, syncopated pushes)
 *   LEAD     a sparse motif in a higher register that prefers the steps the
 *            bass leaves empty, repeated with a variation in the last beat
 *   TEXTURE  a few high, probabilistic events on weak steps, often with an
 *            odd track length so it drifts against the bar (polymeter)
 * Only pure CORROSION helpers that consume no randomness are used
 * (corr_in_scale, corr_range).  Locked extension steps are preserved.
 */
#include <string.h>
#include "expand.h"

static uint32_t xs;

static uint32_t xr(void)
{
    uint32_t x = xs;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return xs = x;
}
static uint8_t xrn(uint8_t n) { return n <= 1 ? 0 : (uint8_t)(((xr() >> 16) * n) >> 16); }
static uint8_t xch(uint8_t c16) { return (uint8_t)((xr() >> 28) < c16); }

uint32_t expand_stream_seed(const gen_t *g, uint16_t xseed, uint8_t variant)
{
    uint32_t h = 0x811C9DC5u;
    uint8_t b[8], i;
    b[0] = (uint8_t)g->seed; b[1] = (uint8_t)(g->seed >> 8); b[2] = g->key; b[3] = g->scale;
    b[4] = g->style; b[5] = (uint8_t)xseed; b[6] = (uint8_t)(xseed >> 8); b[7] = variant;
    for (i = 0; i < 8; i++) { h ^= b[i]; h *= 0x01000193u; }
    h ^= 0xE7C2A11Eu;                                  /* domain separation from everything else */
    return h ? h : 0x1234567u;
}

/* note in scale n degrees above (n > 0) or below a reference */
static uint8_t deg(uint8_t note, int8_t n, const gen_t *g)
{
    while (n > 0) { do { note++; } while (note < 120 && !corr_in_scale(note, g->key, g->scale)); n--; }
    while (n < 0) { do { note--; } while (note > 12 && !corr_in_scale(note, g->key, g->scale)); n++; }
    return note;
}

static uint8_t root_midi(const gen_t *g, uint8_t octave)       /* tonal centre at MIDI octave */
{
    return (uint8_t)(12 * (octave + 1) + g->key % 12);
}

static void put(xtrack_t *t, uint8_t s, uint8_t type, uint8_t note, uint8_t extra)
{
    if (t->flags[s] & STEP_LOCK) return;
    t->note[s] = note;
    t->flags[s] = (uint8_t)(type | extra);
}

/* progressions in scale degrees (from the tonal centre), chosen per variant */
static const int8_t prog[8][4] = {
    { 0, 0, 5, 5 }, { 0, 3, 4, 0 }, { 0, -2, -3, -1 }, { 0, 5, 3, 4 },
    { 0, 2, 0, -2 }, { 0, 0, 0, 0 }, { 0, -1, 0, 1 }, { 0, 4, 5, 3 },
};

void expand_generate(xpattern_t *x, const pattern_t *core, const gen_t *g,
                     uint16_t xseed, uint8_t variant, uint8_t which)
{
    uint8_t s, i, style = g->style % STYLE_COUNT;
    uint8_t centre3 = root_midi(g, 3), centre4 = root_midi(g, 4);
    uint8_t pi = (uint8_t)(variant & 7);
    xs = expand_stream_seed(g, xseed, variant);
    /* the stream is always consumed in the same order, whatever `which`
     * selects, so each track is reproducible on its own */

    /* ---- DRONE ----------------------------------------------------------- */
    {
        xtrack_t t;
        uint8_t fifth = xch(5), n = fifth ? deg(centre3, 4, g) : centre3;
        memset(&t, 0, sizeof t);
        put(&t, 0, STEP_NOTE, (uint8_t)(n - 12 * xch(6)), 0);
        for (s = 1; s < 16; s++) put(&t, s, STEP_TIE, n, 0);
        if (xch(4)) { put(&t, 8, STEP_NOTE, deg(n, xch(8) ? 2 : -1, g), 0); }
        if (which & XG_DRONE) {
            for (s = 0; s < 16; s++) if (!(x->trk[XT_DRONE].flags[s] & STEP_LOCK)) { x->trk[XT_DRONE].note[s] = t.note[s]; x->trk[XT_DRONE].flags[s] = t.flags[s]; }
            x->len[XT_DRONE] = 16;
        }
    }

    /* ---- CHORD ----------------------------------------------------------- */
    {
        static const uint16_t rhythm[STYLE_COUNT][2] = {
            { 0x4444, 0x2424 },     /* ACID: offbeat stabs     */
            { 0x0909, 0x4848 },     /* ELECTRO: pushes          */
            { 0x0001, 0x0101 },     /* MINIMAL: long pads       */
            { 0x2492, 0x0848 },     /* BROKEN: off-grid         */
            { 0x1111, 0x5555 },     /* INDUSTRIAL: hammering    */
            { 0x0001, 0x0011 },     /* SPARSE: held pads        */
        };
        xtrack_t t;
        uint16_t r = rhythm[style][xch(8)];
        uint8_t shape = (uint8_t)(xch(8) ? 0 : xrn(CHORD_SHAPES)), held = (r & 0x00FE) == 0 || style == STYLE_SPARSE || style == STYLE_MINIMAL;
        uint8_t shp[16];
        memset(&t, 0, sizeof t);
        for (s = 0; s < 16; s++) {
            int8_t d = prog[pi][s >> 2];
            uint8_t root = deg(centre3 + 12 * (centre3 < 52), d, g);
            if (r & (1u << s)) {
                put(&t, s, STEP_NOTE, root, (uint8_t)((s & 3) == 2 && xch(5) ? STEP_ACCENT : 0));
            } else if (held && s > 0) {
                put(&t, s, STEP_TIE, root, 0);
            }
            shp[s] = shape;
        }
        if (which & XG_CHORD) {
            for (s = 0; s < 16; s++) if (!(x->trk[XT_CHORD].flags[s] & STEP_LOCK)) {
                x->trk[XT_CHORD].note[s] = t.note[s]; x->trk[XT_CHORD].flags[s] = t.flags[s]; x->shape[s] = shp[s];
            }
            x->len[XT_CHORD] = 16;
        }
    }

    /* ---- LEAD -------------------------------------------------------------- */
    {
        xtrack_t t;
        uint8_t L = (uint8_t)(xch(9) ? 8 : 4), on[8], nt[8], ac[8];
        uint8_t dens = (uint8_t)(3 + (g->density >> 2));          /* /16 chance per weak step */
        memset(&t, 0, sizeof t);
        for (i = 0; i < L; i++) {
            uint8_t bass_here = 0, k;
            for (k = i; k < 16; k += L) if ((core->flags[k] & STEP_TYPE_MASK) == STEP_NOTE) bass_here++;
            on[i] = xch((uint8_t)(bass_here > 1 ? dens / 2 : dens + 3));
            nt[i] = deg(centre4, (int8_t)(xrn(7) - (xch(6) ? 2 : 0)), g);
            ac[i] = xch(4);
        }
        on[0] = on[0] || xch(10);
        for (s = 0; s < 16; s++) {
            uint8_t k = (uint8_t)(s % L);
            if (on[k]) put(&t, s, STEP_NOTE, nt[k], (uint8_t)(ac[k] ? STEP_ACCENT : 0));
            else if (s > 0 && (t.flags[s - 1] & STEP_TYPE_MASK) != STEP_REST && xch(5)) put(&t, s, STEP_TIE, nt[k], 0);
        }
        /* answer in beat 4 */
        if (xch(10)) {
            int8_t mv = (int8_t)(xch(8) ? 2 : -1);
            for (s = 12; s < 16; s++) if ((t.flags[s] & STEP_TYPE_MASK) == STEP_NOTE) t.note[s] = deg(t.note[s], mv, g);
        }
        /* slides between adjacent notes, ghost probability on weak steps */
        for (s = 0; s < 15; s++) {
            if ((t.flags[s] & 3) == STEP_NOTE && (t.flags[s + 1] & 3) == STEP_NOTE && xch((uint8_t)(g->slide >> 2))) t.flags[s] |= STEP_SLIDE;
            if ((t.flags[s] & 3) == STEP_NOTE && (s & 3) && xch(3)) t.flags[s] |= (uint8_t)(2 << XF_PROB_SHIFT);
        }
        if (which & XG_LEAD) {
            for (s = 0; s < 16; s++) if (!(x->trk[XT_LEAD].flags[s] & STEP_LOCK)) { x->trk[XT_LEAD].note[s] = t.note[s]; x->trk[XT_LEAD].flags[s] = t.flags[s]; }
            x->len[XT_LEAD] = 16;
        }
    }

    /* ---- TEXTURE ------------------------------------------------------------ */
    {
        static const uint8_t lens[4] = { 16, 12, 7, 5 };
        xtrack_t t;
        uint8_t len = lens[xrn(4)], hits = (uint8_t)(1 + xrn(3));
        memset(&t, 0, sizeof t);
        for (i = 0; i < hits; i++) {
            uint8_t st = (uint8_t)(1 + xrn((uint8_t)(len - 1)));
            put(&t, st, STEP_NOTE, deg(root_midi(g, 5), (int8_t)xrn(8), g), (uint8_t)((1 + xrn(4)) << XF_PROB_SHIFT));
        }
        if (which & XG_TEXTURE) {
            for (s = 0; s < 16; s++) if (!(x->trk[XT_TEXTURE].flags[s] & STEP_LOCK)) { x->trk[XT_TEXTURE].note[s] = t.note[s]; x->trk[XT_TEXTURE].flags[s] = t.flags[s]; }
            x->len[XT_TEXTURE] = len;
        }
    }
    x->present = 1;
}

/* ---- extended share code ---------------------------------------------------- */
void ext_code_encode(const gen_t *g, uint16_t xseed, char *out)
{
    static const char hx[] = "0123456789ABCDEF";
    corr_code_encode(g, out);                           /* the 12 ORIGINAL digits, untouched */
    out[12] = hx[(xseed >> 12) & 15]; out[13] = hx[(xseed >> 8) & 15];
    out[14] = hx[(xseed >> 4) & 15];  out[15] = hx[xseed & 15];
}

uint8_t ext_code_decode(gen_t *g, uint16_t *xseed, const char *in)
{
    uint16_t v = 0;
    uint8_t i;
    for (i = 12; i < 16; i++) {
        char c = in[i];
        uint8_t d;
        if (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F') d = (uint8_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') d = (uint8_t)(c - 'a' + 10);
        else return 0;
        v = (uint16_t)((v << 4) | d);
    }
    if (!corr_code_decode(g, in)) return 0;             /* ORIGINAL decoder */
    *xseed = v;
    return 1;
}
