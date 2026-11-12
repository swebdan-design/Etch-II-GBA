/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * CORROSION -- the generative music engine.
 *
 * Generation is grammar driven, not uniform randomness:
 *   - each rhythmic personality (style) defines per-step note weights,
 *     accent weights, motif lengths, drum templates and habits
 *     (octave jumps, slides, ties, ghost notes, kick avoidance);
 *   - a short motif (2..8 steps) is composed from a small pitch vocabulary
 *     drawn from the key/scale, then repeated across the bar and answered
 *     by a phrase variation in the last quarter;
 *   - drums are the style template plus density-dependent variations.
 *
 * Mutation (DRIFT / CORRODE) applies a budget of small musical operators to a
 * working copy, respects every lock, re-sanitises the result and preserves
 * "musical memory": the downbeat, the tonal centre and the beat-1 motif.
 * CORRODE additionally mutates the motif itself and propagates the change to
 * its repetitions, so the line evolves while keeping its internal repetition.
 */
#ifdef __SDCC
#pragma bank 255
#endif
#include "corrosion.h"

/* ------------------------------------------------------------------------ */
/* PRNG                                                                      */
static uint16_t rng = 1;

void corr_seed(uint16_t seed) ETCH_BANKED
{
    rng = seed ? seed : 0x1D2B;
    corr_rand();
    corr_rand();
}

uint8_t corr_rand(void) ETCH_BANKED
{
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return (uint8_t)(rng ^ (rng >> 8));
}

uint8_t corr_randn(uint8_t n) ETCH_BANKED
{
    if (n <= 1) return 0;
    return (uint8_t)(((uint16_t)corr_rand() * n) >> 8);
}

/* chance out of 16 */
static uint8_t chance16(uint8_t c)
{
    return (corr_rand() & 15) < c;
}

/* ------------------------------------------------------------------------ */
/* scales                                                                    */
static const uint16_t scale_mask[SCALE_COUNT] = {
    /* bit n = semitone n present */
    0x05AD,   /* minor:        0 2 3 5 7 8 10   */
    0x0AB5,   /* major:        0 2 4 5 7 9 11   */
    0x06AD,   /* dorian:       0 2 3 5 7 9 10   */
    0x05AB,   /* phrygian:     0 1 3 5 7 8 10   */
    0x04A9,   /* minor pent.:  0 3 5 7 10       */
    0x0FFF,   /* chromatic                       */
};

static uint8_t mod12(uint8_t n)
{
    while (n >= 12) n -= 12;
    return n;
}

uint8_t corr_in_scale(uint8_t note, uint8_t key, uint8_t scale) ETCH_BANKED
{
    uint8_t pc = mod12((uint8_t)(note + 12 - mod12(key)));
    return (scale_mask[scale % SCALE_COUNT] >> pc) & 1;
}

void corr_range(const gen_t *g, uint8_t *lo, uint8_t *hi) ETCH_BANKED
{
    uint8_t root = (uint8_t)(mod12(g->key) + 12 * (g->octave > 2 ? 2 : g->octave));
    /* two octaves above the root plus a fourth below it */
    *lo = root >= 5 ? (uint8_t)(root - 5) : 0;
    *hi = (uint8_t)(root + 24);
    if (*hi > NOTE_MAX) *hi = NOTE_MAX;
}

uint8_t corr_snap(uint8_t note, const gen_t *g) ETCH_BANKED
{
    uint8_t lo, hi, d;
    corr_range(g, &lo, &hi);
    while (note > hi) note = (uint8_t)(note - 12);
    while (note < lo) note = (uint8_t)(note + 12);
    if (note > hi) note = hi;
    for (d = 0; d < 6; d++) {
        if (note >= d && note - d >= lo && corr_in_scale((uint8_t)(note - d), g->key, g->scale))
            return (uint8_t)(note - d);
        if (note + d <= hi && corr_in_scale((uint8_t)(note + d), g->key, g->scale))
            return (uint8_t)(note + d);
    }
    return note;
}

/* next scale note up (dir>0) or down (dir<0) inside the range */
uint8_t corr_scale_step(uint8_t note, int8_t dir, const gen_t *g) ETCH_BANKED
{
    uint8_t lo, hi, n = note, i;
    corr_range(g, &lo, &hi);
    for (i = 0; i < 12; i++) {
        if (dir > 0) { if (n >= hi) break; n++; }
        else { if (n <= lo) break; n--; }
        if (corr_in_scale(n, g->key, g->scale)) return n;
    }
    return corr_snap(note, g);
}

/* ------------------------------------------------------------------------ */
/* style grammars                                                            */
typedef struct {
    uint8_t  step_w[16];    /* note weight per 16th (0..15)             */
    uint8_t  acc_w[16];     /* accent weight per 16th                   */
    uint8_t  motif[3];      /* candidate motif lengths                  */
    uint8_t  kick_avoid;    /* /16 chance to drop a bass note on a kick */
    uint8_t  oct_jump;      /* /32 chance of an octave jump             */
    uint8_t  slide_bias;    /* 0..15                                    */
    uint8_t  tie;           /* /16 chance a rest after a note becomes tie */
    uint8_t  ghost;         /* /16 chance of a probability<100% note     */
    uint8_t  vocab;         /* base vocabulary size                     */
    uint8_t  answer;        /* /16 chance of a phrase answer in beat 4  */
    uint16_t drum[NUM_LANES];
    uint16_t dvar[NUM_LANES];
    uint8_t  kit[NUM_LANES];
} grammar_t;

static const grammar_t grammars[STYLE_COUNT] = {
    {   /* ACID HOUSE: busy 16th-note lines, offbeat accents, slides */
        {15, 6, 10, 12, 9, 7, 12, 10, 13, 6, 10, 12, 9, 8, 12, 11},
        { 6, 2, 9, 4, 3, 2, 11, 4, 6, 2, 9, 4, 3, 3, 11, 6},
        {4, 8, 8}, 6, 6, 12, 1, 1, 4, 11,
        {0x1111, 0x1010, 0x4444, 0x0000}, {0x0000, 0x0000, 0xAAAA, 0x4040}, {0, 2, 0, 0}
    },
    {   /* ELECTRO: syncopated 808-style, sparse bass on the off-grid */
        {15, 2, 4, 12, 3, 4, 12, 5, 10, 3, 12, 10, 4, 3, 12, 6},
        { 8, 1, 2, 9, 1, 2, 9, 2, 6, 1, 9, 6, 2, 1, 10, 3},
        {8, 16, 8}, 10, 3, 6, 1, 2, 4, 9,
        {0x0441, 0x1010, 0x5555, 0x0000}, {0x4808, 0x8000, 0xAAAA, 0x4000}, {1, 0, 0, 2}
    },
    {   /* MINIMAL: few notes, hypnotic repetition */
        {10, 2, 12, 3, 2, 2, 13, 2, 8, 2, 12, 3, 2, 2, 13, 4},
        { 4, 1, 8, 2, 1, 1, 10, 1, 4, 1, 8, 2, 1, 1, 10, 2},
        {4, 4, 8}, 8, 2, 8, 3, 3, 3, 6,
        {0x1111, 0x1000, 0x4444, 0x0000}, {0x0000, 0x8080, 0xAAAA, 0x0044}, {0, 3, 1, 1}
    },
    {   /* BROKEN: odd-length motifs drift against the bar */
        {14, 4, 6, 11, 3, 10, 4, 8, 12, 3, 9, 5, 10, 4, 7, 9},
        { 7, 2, 3, 8, 1, 6, 2, 4, 7, 1, 5, 2, 7, 2, 4, 5},
        {3, 5, 6}, 4, 4, 8, 2, 5, 5, 8,
        {0x0481, 0x1010, 0x4A4A, 0x0000}, {0x2008, 0x8200, 0x1515, 0x4000}, {1, 1, 2, 0}
    },
    {   /* INDUSTRIAL: relentless root/octave pulse, hard accents */
        {15, 10, 12, 10, 15, 10, 12, 10, 15, 10, 12, 10, 15, 10, 12, 12},
        { 9, 2, 5, 2, 9, 2, 5, 2, 9, 2, 5, 2, 9, 2, 6, 4},
        {4, 4, 2}, 0, 8, 4, 0, 1, 2, 7,
        {0x1111, 0x1010, 0x5555, 0x4444}, {0xC080, 0x8080, 0xAAAA, 0x0000}, {3, 1, 3, 1}
    },
    {   /* SPARSE: long notes, ties, space */
        {14, 1, 2, 4, 1, 1, 8, 1, 6, 1, 2, 6, 1, 1, 7, 2},
        { 8, 1, 1, 3, 1, 1, 6, 1, 4, 1, 1, 4, 1, 1, 6, 1},
        {8, 16, 16}, 12, 2, 12, 9, 4, 3, 5,
        {0x0401, 0x0100, 0x1010, 0x0000}, {0x0080, 0x4000, 0x4444, 0x4000}, {2, 2, 1, 3}
    },
};

/* preferred intervals (semitones above root) in order of tonal importance */
static const int8_t pref_int[10] = { 0, 12, 7, 3, 10, 5, 2, 8, -2, 1 };

static uint8_t vocab[8];
static uint8_t vocab_n;

static void build_vocab(const gen_t *g, const grammar_t *gr)
{
    uint8_t lo, hi, root, want, i, n;
    int8_t iv;
    corr_range(g, &lo, &hi);
    root = (uint8_t)(mod12(g->key) + 12 * (g->octave > 2 ? 2 : g->octave));
    want = (uint8_t)(gr->vocab + (g->density >> 3) + corr_randn(2));
    if (want > 7) want = 7;
    vocab_n = 0;
    vocab[vocab_n++] = root;
    for (i = 1; i < 10 && vocab_n < want; i++) {
        uint8_t j, dup = 0;
        /* skip a candidate now and then so lines differ between seeds */
        if (i > 2 && chance16(4)) continue;
        iv = pref_int[i];
        if ((int16_t)root + iv < (int16_t)lo || (int16_t)root + iv > (int16_t)hi) continue;
        n = corr_snap((uint8_t)(root + iv), g);
        for (j = 0; j < vocab_n; j++) if (vocab[j] == n) dup = 1;
        if (!dup) vocab[vocab_n++] = n;
    }
    if (vocab_n < 2) {
        n = corr_scale_step(root, 1, g);
        if (n != root) vocab[vocab_n++] = n;
    }
}

static uint8_t pick_note(uint8_t strong)
{
    uint8_t r;
    if (strong && chance16(10)) return vocab[0];
    /* triangular weighting toward the first (most tonal) entries */
    r = corr_randn(vocab_n);
    if (r > 0 && chance16(6)) r = corr_randn(r);
    return vocab[r];
}

/* ------------------------------------------------------------------------ */
static void gen_drums_into(pattern_t *out, const gen_t *g, const grammar_t *gr)
{
    uint8_t l, s;
    for (l = 0; l < NUM_LANES; l++) {
        uint16_t m = gr->drum[l];
        uint16_t v = gr->dvar[l];
        for (s = 0; s < 16; s++) {
            uint8_t r = corr_rand() & 15;
            if (v & ((uint16_t)1 << s)) {
                /* density adds variation hits; hats also follow syncopation */
                uint8_t c = (uint8_t)(g->density >> 1);
                if (l >= LANE_CH && (s & 1)) c = (uint8_t)(c + (g->sync >> 2));
                if (r < c) m |= (uint16_t)1 << s;
            }
        }
        /* very low density thins the hats */
        if (l == LANE_CH && g->density < 4) m &= 0x5555;
        out->drum[l] = m;
        out->kit[l] = gr->kit[l];
    }
}

static void gen_bass_into(pattern_t *out, const gen_t *g, const grammar_t *gr, uint16_t kick)
{
    uint8_t L, i, s, notes;
    uint8_t m_on[16], m_note[16], m_acc[16];

    L = gr->motif[corr_randn(3)];
    build_vocab(g, gr);

    for (i = 0; i < L; i++) {
        int16_t w = gr->step_w[i];
        uint8_t thr;
        if (i & 1) w += g->sync / 3;
        else if ((i & 3) == 0 && i) w -= g->sync / 4;
        if (w < 0) w = 0;
        thr = (uint8_t)(((uint16_t)w * (g->density + 4)) / 14);
        if (thr > 15) thr = 15;
        m_on[i] = (corr_rand() & 15) < thr;
        m_note[i] = pick_note((i & 3) == 0);
        if (chance16(gr->oct_jump >> 1) && m_note[i] + 12 <= NOTE_MAX) {
            uint8_t lo, hi;
            corr_range(g, &lo, &hi);
            if (m_note[i] + 12 <= hi) m_note[i] = (uint8_t)(m_note[i] + 12);
        }
        m_acc[i] = (corr_rand() & 15) < (uint8_t)(((uint16_t)gr->acc_w[i] * g->accent) >> 4);
    }
    if (g->density >= 2) {
        m_on[0] = 1;                    /* tonal centre on the downbeat */
        m_note[0] = vocab[0];
    }
    notes = 0;
    for (i = 0; i < L; i++) notes += m_on[i];
    if (!notes) { m_on[0] = 1; m_note[0] = vocab[0]; }

    for (s = 0; s < 16; s++) {
        uint8_t k = s;
        while (k >= L) k = (uint8_t)(k - L);
        out->note[s] = m_note[k];
        out->flags[s] = m_on[k] ? (uint8_t)(STEP_NOTE | (m_acc[k] ? STEP_ACCENT : 0)) : STEP_REST;
        out->prob[s] = PROB_ALWAYS;
    }

    /* phrase answer: beat 4 varies the motif */
    if (L <= 8 && chance16(gr->answer)) {
        uint8_t kind = corr_randn(3);
        for (s = 12; s < 16; s++) {
            if ((out->flags[s] & STEP_TYPE_MASK) != STEP_NOTE) continue;
            if (kind == 0) {
                out->note[s] = corr_scale_step(corr_scale_step(out->note[s], 1, g), 1, g);
            } else if (kind == 1 && s == 14) {
                out->flags[s] = STEP_REST;
            } else if (kind == 2 && out->note[s] + 12 <= NOTE_MAX) {
                uint8_t lo, hi;
                corr_range(g, &lo, &hi);
                if (out->note[s] + 12 <= hi && chance16(8)) out->note[s] = (uint8_t)(out->note[s] + 12);
            }
        }
        if (kind == 1 && (out->flags[13] & STEP_TYPE_MASK) == STEP_REST) {
            out->flags[13] = STEP_NOTE | STEP_ACCENT;
            out->note[13] = vocab[vocab_n > 1 ? 1 : 0];
        }
    }

    /* the bass leaves room for the kick */
    for (s = 1; s < 16; s++) {
        uint8_t r = corr_rand() & 15;
        if ((kick & ((uint16_t)1 << s)) && (out->flags[s] & STEP_TYPE_MASK) == STEP_NOTE
            && r < gr->kick_avoid && pattern_note_count(out) > 3)
            out->flags[s] = STEP_REST;
    }

    /* slides, ties, ghosts */
    for (s = 0; s < 16; s++) {
        uint8_t r1 = corr_rand() & 15, r2 = corr_rand() & 15, r3 = corr_rand() & 15;
        uint8_t t = out->flags[s] & STEP_TYPE_MASK;
        uint8_t nt = out->flags[(s + 1) & 15] & STEP_TYPE_MASK;
        if (t == STEP_NOTE && nt == STEP_NOTE && s != 15 &&
            r1 < (uint8_t)(((uint16_t)g->slide * gr->slide_bias) >> 5))
            out->flags[s] |= STEP_SLIDE;
        if (t == STEP_REST && s > 0 && r2 < gr->tie) {
            uint8_t pt = out->flags[s - 1] & STEP_TYPE_MASK;
            if (pt != STEP_REST) out->flags[s] = STEP_TIE;
        }
        if (t == STEP_NOTE && (s & 3) && r3 < gr->ghost)
            out->prob[s] = (uint8_t)(4 + (r3 & 1));
    }

    /* an accented line should have at least one accent */
    if (g->accent > 5) {
        uint8_t any = 0, best = 0xFF, bw = 0;
        for (s = 0; s < 16; s++) {
            if (out->flags[s] & STEP_ACCENT) any = 1;
            if ((out->flags[s] & STEP_TYPE_MASK) == STEP_NOTE && gr->acc_w[s] > bw) { bw = gr->acc_w[s]; best = s; }
        }
        if (!any && best != 0xFF) out->flags[best] |= STEP_ACCENT;
    }
}

void corr_generate(pattern_t *p, const gen_t *g, uint8_t what) ETCH_BANKED
{
    pattern_t tmp;
    const grammar_t *gr = &grammars[g->style % STYLE_COUNT];
    uint8_t s, l;

    if (p->lock & PLOCK_ALL) return;
    memcpy(&tmp, p, sizeof(tmp));
    corr_seed(g->seed);
    /* drums first so the bass can respond to the kick; both are always
     * generated so the random stream (and thus the seed code) is stable */
    gen_drums_into(&tmp, g, gr);
    gen_bass_into(&tmp, g, gr, (what & GEN_DRUMS) ? tmp.drum[LANE_KICK] : p->drum[LANE_KICK]);
    tmp.style = g->style % STYLE_COUNT;
    tmp.gen = 0;

    if ((what & GEN_DRUMS)) {
        for (l = 0; l < NUM_LANES; l++) {
            if (p->lock & (PLOCK_LANE0 << l)) continue;
            p->drum[l] = tmp.drum[l];
            p->kit[l] = tmp.kit[l];
        }
    }
    if ((what & GEN_BASS) && !(p->lock & PLOCK_BASS)) {
        for (s = 0; s < 16; s++) {
            if (p->flags[s] & STEP_LOCK) continue;
            p->note[s] = tmp.note[s];
            p->flags[s] = tmp.flags[s];
            p->prob[s] = tmp.prob[s];
        }
    }
    p->style = tmp.style;
    p->gen = 0;
    pattern_sanitize(p);
}

/* ------------------------------------------------------------------------ */
/* mutation                                                                  */
enum {
    OP_ACC_MOVE, OP_REST_TOGGLE, OP_NOTE_STEP, OP_OCTAVE, OP_SLIDE,
    OP_DRUM, OP_MEMORY, OP_MOTIF, OP_ROTATE, OP_PROB, OP_RECALL, OP_COUNT
};
/* DRIFT wanders around its anchor (RECALL pulls it back); CORRODE diverges
 * but propagates motif changes to keep internal repetition. */
static const uint8_t op_w_drift[OP_COUNT]   = { 3, 2, 2, 2, 2, 3, 1, 1, 0, 0, 1 };
static const uint8_t op_w_corrode[OP_COUNT] = { 2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 0 };

static const pattern_t *anchor;
static pattern_t scratch;

static uint8_t bass_free(const pattern_t *p, uint8_t s)
{
    return !(p->lock & (PLOCK_BASS | PLOCK_ALL)) && !(p->flags[s] & STEP_LOCK);
}

static uint8_t pick_step(const pattern_t *p, uint8_t want_type, uint8_t avoid0)
{
    /* random start, scan for an eligible step; 0xFF if none */
    uint8_t i, s = corr_randn(16);
    for (i = 0; i < 16; i++, s = (uint8_t)((s + 1) & 15)) {
        if (avoid0 && s == 0) continue;
        if (!bass_free(p, s)) continue;
        if (want_type != 0xFF && (p->flags[s] & STEP_TYPE_MASK) != want_type) continue;
        return s;
    }
    return 0xFF;
}

static uint8_t accent_count(const pattern_t *p)
{
    uint8_t s, n = 0;
    for (s = 0; s < 16; s++)
        if ((p->flags[s] & STEP_TYPE_MASK) == STEP_NOTE && (p->flags[s] & STEP_ACCENT)) n++;
    return n;
}

/* Toggle one hit in lane l.  Toggles prefer the style's variation positions;
 * the template kick and backbeat are structural and stay (memory) unless
 * corroding. */
static void drum_touch(pattern_t *p, uint8_t l, uint8_t corrode)
{
    const grammar_t *gr = &grammars[p->style % STYLE_COUNT];
    uint8_t t, s;
    for (t = 0; t < 12; t++) {
        uint16_t bit;
        s = corr_randn(16);
        bit = (uint16_t)1 << s;
        if ((l == LANE_KICK || l == LANE_SNARE) && (gr->drum[l] & bit) && !corrode) continue;
        if (!(gr->dvar[l] & bit) && !(p->drum[l] & bit) && t < 6) continue;
        p->drum[l] ^= bit;
        return;
    }
}

/* Musical memory, per channel: restore one bass step from the anchor. */
static uint8_t recall_bass(pattern_t *p)
{
    uint8_t k, s = corr_randn(16);
    if (!anchor) return 0;
    for (k = 0; k < 16; k++, s = (uint8_t)((s + 1) & 15)) {
        if (!bass_free(p, s)) continue;
        if (p->note[s] != anchor->note[s] || (p->flags[s] & ~STEP_LOCK) != (anchor->flags[s] & ~STEP_LOCK)
            || p->prob[s] != anchor->prob[s]) {
            p->note[s] = anchor->note[s];
            p->flags[s] = (uint8_t)((anchor->flags[s] & ~STEP_LOCK) | (p->flags[s] & STEP_LOCK));
            p->prob[s] = anchor->prob[s];
            return 1;
        }
    }
    return 0;
}

/* ... and one hit of drum lane l. */
static uint8_t recall_lane(pattern_t *p, uint8_t l)
{
    uint16_t x;
    uint8_t k, s = corr_randn(16);
    if (!anchor) return 0;
    x = p->drum[l] ^ anchor->drum[l];
    if (!x) return 0;
    for (k = 0; k < 16; k++, s = (uint8_t)((s + 1) & 15)) {
        if (x & ((uint16_t)1 << s)) { p->drum[l] ^= (uint16_t)1 << s; return 1; }
    }
    return 0;
}

static uint8_t bass_divergence(const pattern_t *p, const pattern_t *q)
{
    uint8_t s, d = 0;
    for (s = 0; s < 16; s++)
        if (p->note[s] != q->note[s] || p->flags[s] != q->flags[s] || p->prob[s] != q->prob[s]) d++;
    return d;
}

static uint8_t lane_divergence(const pattern_t *p, const pattern_t *q, uint8_t l)
{
    uint16_t x = p->drum[l] ^ q->drum[l];
    uint8_t d = 0;
    while (x) { d = (uint8_t)(d + (x & 1)); x >>= 1; }
    return d;
}

static void apply_op(pattern_t *p, const gen_t *g, uint8_t op, uint8_t corrode)
{
    uint8_t s, t, l;
    
    /* homeostasis: GEN DENSITY and ACCENT set the targets evolution orbits */
    uint8_t notes = pattern_note_count(p);
    uint8_t tgt_notes = (uint8_t)(3 + (g->density * 10) / 15);
    uint8_t accs = accent_count(p);
    uint8_t tgt_acc = (uint8_t)(1 + ((uint16_t)g->accent * notes) / 40);
    switch (op) {
    case OP_ACC_MOVE: {
        uint8_t from = 0xFF;
        for (t = 0; t < 16; t++) {
            s = (uint8_t)((corr_randn(16) + t) & 15);
            if (bass_free(p, s) && (p->flags[s] & STEP_ACCENT)) { from = s; break; }
        }
        if (accs > tgt_acc && from != 0xFF) {        /* too many: remove */
            p->flags[from] &= (uint8_t)~STEP_ACCENT;
            break;
        }
        s = pick_step(p, STEP_NOTE, 0);
        if (s == 0xFF) break;
        if (from != 0xFF && accs >= tgt_acc) p->flags[from] &= (uint8_t)~STEP_ACCENT;
        p->flags[s] |= STEP_ACCENT;
        break;
    }
    case OP_REST_TOGGLE:
        if (notes > tgt_notes + 1) s = pick_step(p, STEP_NOTE, 1);
        else if (notes + 1 < tgt_notes) s = pick_step(p, STEP_REST, 1);
        else s = pick_step(p, 0xFF, 1);              /* never the downbeat */
        if (s == 0xFF) break;
        t = p->flags[s] & STEP_TYPE_MASK;
        if (t == STEP_NOTE && notes > 2) {
            p->flags[s] = STEP_REST;
        } else if (t != STEP_NOTE) {
            /* new note borrowed from a neighbour, nudged in scale */
            uint8_t nb = p->note[(s + 15) & 15];
            p->note[s] = corr_rand() & 1 ? nb : corr_scale_step(nb, (int8_t)(corr_rand() & 1 ? 1 : -1), g);
            p->flags[s] = STEP_NOTE;
            p->prob[s] = PROB_ALWAYS;
        }
        break;
    case OP_NOTE_STEP:
        /* DRIFT keeps the downbeat (tonal centre) */
        s = pick_step(p, STEP_NOTE, !corrode);
        if (s == 0xFF) break;
        if (s == 0) p->note[0] = corr_snap(p->note[0], g);
        else p->note[s] = corr_scale_step(p->note[s], (int8_t)(corr_rand() & 1 ? 1 : -1), g);
        break;
    case OP_OCTAVE: {
        uint8_t lo, hi;
        s = pick_step(p, STEP_NOTE, 1);
        if (s == 0xFF) break;
        corr_range(g, &lo, &hi);
        if (p->note[s] + 12 <= hi && (corr_rand() & 1 || p->note[s] < lo + 12)) p->note[s] = (uint8_t)(p->note[s] + 12);
        else if (p->note[s] >= lo + 12) p->note[s] = (uint8_t)(p->note[s] - 12);
        break;
    }
    case OP_SLIDE:
        s = pick_step(p, STEP_NOTE, 0);
        if (s == 0xFF || s == 15) break;
        if ((p->flags[s + 1] & STEP_TYPE_MASK) == STEP_NOTE || (p->flags[s] & STEP_SLIDE))
            p->flags[s] ^= STEP_SLIDE;
        break;
    case OP_DRUM:
        l = corr_randn(NUM_LANES);
        for (t = 0; t < NUM_LANES; t++, l = (uint8_t)((l + 1) & 3)) {
            if (!(p->lock & ((PLOCK_LANE0 << l) | PLOCK_ALL))) break;
        }
        if (t == NUM_LANES) break;
        drum_touch(p, l, corrode);
        break;
    case OP_MEMORY: {
        /* re-establish repetition: copy beat 1 onto the most divergent beat */
        uint8_t best = 0, bd = 0, k, d;
        if (p->lock & PLOCK_BASS) break;
        for (k = 1; k < 4; k++) {
            d = 0;
            for (t = 0; t < 4; t++) {
                if (p->flags[k * 4 + t] != p->flags[t] || p->note[k * 4 + t] != p->note[t]) d++;
            }
            if (d > bd) { bd = d; best = k; }
        }
        if (bd < 2) break;
        for (t = 0; t < 4; t++) {
            s = (uint8_t)(best * 4 + t);
            if (!bass_free(p, s)) continue;
            p->note[s] = p->note[t];
            p->flags[s] = (uint8_t)((p->flags[t] & ~STEP_LOCK) | (p->flags[s] & STEP_LOCK));
            p->prob[s] = p->prob[t];
        }
        break;
    }
    case OP_MOTIF: {
        /* mutate one step of the beat-1 motif and every repetition of it */
        uint8_t old_n, old_f, nn, nf, k;
        t = corr_randn(4);
        if (!bass_free(p, t)) break;
        old_n = p->note[t];
        old_f = p->flags[t];
        nn = old_n;
        nf = old_f;
        if ((old_f & STEP_TYPE_MASK) == STEP_NOTE) {
            /* the downbeat keeps its pitch (tonal centre): only its accent moves */
            if (t != 0 && (corr_rand() & 1)) nn = corr_scale_step(old_n, (int8_t)(corr_rand() & 1 ? 1 : -1), g);
            else if ((old_f & STEP_ACCENT) || accs < tgt_acc) nf = (uint8_t)(nf ^ STEP_ACCENT);
            else if (t != 0) nn = corr_scale_step(old_n, (int8_t)(corr_rand() & 1 ? 1 : -1), g);
            else break;
        } else if (t != 0 && notes + 2 <= tgt_notes) {
            nf = (uint8_t)((old_f & STEP_LOCK) | STEP_NOTE);
            nn = pick_note(0);
        } else {
            break;
        }
        for (k = 0; k < 4; k++) {
            s = (uint8_t)(k * 4 + t);
            if (!bass_free(p, s)) continue;
            if (k == 0 || (p->note[s] == old_n && (p->flags[s] & ~STEP_LOCK) == (old_f & ~STEP_LOCK))) {
                p->note[s] = nn;
                p->flags[s] = (uint8_t)((nf & ~STEP_LOCK) | (p->flags[s] & STEP_LOCK));
            }
        }
        break;
    }
    case OP_ROTATE: {
        /* rotate one beat (4 steps) by one 16th -- syncopation shifts */
        uint8_t b = (uint8_t)(1 + corr_randn(3)), n0, f0, p0;
        uint8_t base = (uint8_t)(b * 4);
        for (t = 0; t < 4; t++) if (!bass_free(p, (uint8_t)(base + t))) return;
        n0 = p->note[base + 3]; f0 = p->flags[base + 3]; p0 = p->prob[base + 3];
        for (t = 3; t > 0; t--) {
            p->note[base + t] = p->note[base + t - 1];
            p->flags[base + t] = p->flags[base + t - 1];
            p->prob[base + t] = p->prob[base + t - 1];
        }
        p->note[base] = n0; p->flags[base] = f0; p->prob[base] = p0;
        break;
    }
    case OP_RECALL:
        /* musical memory: restore one bass step or drum hit from the anchor */
        if (recall_bass(p)) break;
        for (l = 0; l < NUM_LANES; l++) {
            if (p->lock & (PLOCK_LANE0 << l)) continue;
            if (recall_lane(p, l)) break;
        }
        break;
    case OP_PROB:
        s = pick_step(p, STEP_NOTE, 1);
        if (s == 0xFF) break;
        p->prob[s] = (uint8_t)((p->prob[s] & 0xF0) | ((p->prob[s] & 7) == 7 ? 4 + (corr_rand() & 1) : 7));
        break;
    }
}

uint8_t corr_mutate(pattern_t *dst, const pattern_t *src, const pattern_t *anc,
                    const gen_t *g, uint8_t mode, uint8_t intensity, uint16_t seed) ETCH_BANKED
{
    uint8_t budget, i, op, s, l, changes;
    const uint8_t *w;
    uint8_t wsum;

    memcpy(dst, src, sizeof(pattern_t));
    if (mode == EVO_STABLE || (src->lock & PLOCK_ALL)) return 0;
    if (intensity > 15) intensity = 15;
    if (mode == EVO_CORRODE) {
        budget = (uint8_t)(2 + intensity / 3);
        w = op_w_corrode;
    } else {
        budget = (uint8_t)(1 + intensity / 6);
        w = op_w_drift;
    }
    anchor = anc;
    wsum = 0;
    for (i = 0; i < OP_COUNT; i++) wsum = (uint8_t)(wsum + w[i]);
    corr_seed((uint16_t)(seed ^ ((uint16_t)src->gen << 8) ^ 0x5A17));

    /* the mutation key/scale come from GEN; vocabulary for new notes is the
     * style vocabulary in that key */
    build_vocab(g, &grammars[src->style % STYLE_COUNT]);

    if (mode == EVO_DRIFT) {
        /* DRIFT ("living song"): every phrase touches EVERY channel a little.
         * The bass gets 1-2 small operations and each unlocked drum lane one
         * hit.  Each channel has its own bound around the anchor; once a
         * channel reaches it, its change becomes a recall instead, so the
         * song breathes around itself and never wanders off. */
        uint8_t bmax = (uint8_t)(2 + intensity / 6);
        uint8_t dmax = (uint8_t)(1 + (intensity + 4) / 8);
        uint8_t bass_ops = (uint8_t)(1 + (intensity >= 12));
        for (i = 0; i < bass_ops; i++) {
            uint8_t tries;
            for (tries = 0; tries < 4; tries++) {
                uint8_t r = corr_randn(wsum);
                for (op = 0; op < OP_COUNT; op++) {
                    if (r < w[op]) break;
                    r = (uint8_t)(r - w[op]);
                }
                /* subtle: only single-step operators */
                if (op == OP_DRUM || op == OP_RECALL || op == OP_MEMORY || op == OP_MOTIF ||
                    op == OP_ROTATE || op == OP_PROB) op = OP_NOTE_STEP;
                if (anchor && bass_divergence(dst, anchor) >= bmax) {
                    if (recall_bass(dst)) break;
                    op = OP_ACC_MOVE;
                }
                memcpy(&scratch, dst, sizeof(pattern_t));
                apply_op(dst, g, op, 0);
                if (memcmp(&scratch, dst, 48)) break;       /* something changed */
            }
        }
        for (l = 0; l < NUM_LANES; l++) {
            if (src->lock & (PLOCK_LANE0 << l)) continue;
            if (anchor && lane_divergence(dst, anchor, l) >= dmax && recall_lane(dst, l)) continue;
            drum_touch(dst, l, 0);
        }
    } else {
        for (i = 0; i < budget; i++) {
            uint8_t r = corr_randn(wsum);
            for (op = 0; op < OP_COUNT; op++) {
                if (r < w[op]) break;
                r = (uint8_t)(r - w[op]);
            }
            apply_op(dst, g, op, 1);
        }
    }

    pattern_sanitize(dst);

    /* hard guarantee: locked material is bit-identical to the source */
    if (src->lock & PLOCK_BASS) {
        memcpy(dst->note, src->note, 16);
        memcpy(dst->flags, src->flags, 16);
        memcpy(dst->prob, src->prob, 16);
    } else {
        for (s = 0; s < 16; s++) {
            if (src->flags[s] & STEP_LOCK) {
                dst->note[s] = src->note[s];
                dst->flags[s] = src->flags[s];
                dst->prob[s] = src->prob[s];
            }
        }
    }
    for (l = 0; l < NUM_LANES; l++) {
        if (src->lock & (PLOCK_LANE0 << l)) {
            dst->drum[l] = src->drum[l];
            dst->kit[l] = src->kit[l];
        }
    }
    /* a slide into a step must still be valid after lock restoration */
    pattern_sanitize(dst);
    for (s = 0; s < 16; s++) {                     /* sanitize must not touch locks */
        if (src->flags[s] & STEP_LOCK) dst->flags[s] = src->flags[s];
    }

    changes = 0;
    for (s = 0; s < 16; s++) {
        if (dst->note[s] != src->note[s] || dst->flags[s] != src->flags[s] || dst->prob[s] != src->prob[s])
            changes++;
    }
    for (l = 0; l < NUM_LANES; l++) {
        uint16_t x = dst->drum[l] ^ src->drum[l];
        while (x) { changes += (uint8_t)(x & 1); x >>= 1; }
    }
    dst->gen = (uint8_t)(src->gen + 1);
    return changes;
}

/* ------------------------------------------------------------------------ */
void corr_regen_material(pattern_t *mat, const gen_t *g, uint16_t seed) ETCH_BANKED
{
    gen_t g2;
    memcpy(&g2, g, sizeof(gen_t));
    g2.seed = seed ? seed : 0x2601;
    pattern_clear(mat);
    mat->style = g->style;
    corr_generate(mat, &g2, GEN_ALL);             /* new material, whole bar */
}

uint16_t corr_regen_apply(pattern_t *p, const pattern_t *mat, uint16_t pick_seed, uint8_t what) ETCH_BANKED
{
    uint8_t idx[16], nfree = 0, s, i, l, n;
    uint8_t bass_ok = (what & GEN_BASS) && !(p->lock & PLOCK_BASS);
    uint8_t lanes_ok = 0;
    uint16_t mask = 0;
    if (p->lock & PLOCK_ALL) return 0;
    for (l = 0; l < NUM_LANES; l++)
        if ((what & GEN_DRUMS) && !(p->lock & (PLOCK_LANE0 << l))) lanes_ok |= (uint8_t)(1 << l);

    /* eligible steps: something there may be replaced */
    for (s = 0; s < 16; s++)
        if ((bass_ok && !(p->flags[s] & STEP_LOCK)) || lanes_ok) idx[nfree++] = s;
    corr_seed((uint16_t)(pick_seed ^ 0x3C5A));
    n = nfree < 8 ? nfree : 8;
    for (i = 0; i < n; i++) {                        /* partial Fisher-Yates */
        uint8_t j = (uint8_t)(i + corr_randn((uint8_t)(nfree - i)));
        uint8_t t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        mask |= (uint16_t)1 << idx[i];
    }
    for (s = 0; s < 16; s++) {
        uint16_t bit = (uint16_t)1 << s;
        if (!(mask & bit)) continue;
        if (bass_ok && !(p->flags[s] & STEP_LOCK)) {
            p->note[s] = mat->note[s];
            p->flags[s] = (uint8_t)(mat->flags[s] & ~STEP_LOCK);
            p->prob[s] = mat->prob[s];
        }
        for (l = 0; l < NUM_LANES; l++) {
            if (!(lanes_ok & (1 << l))) continue;
            p->drum[l] = (uint16_t)((p->drum[l] & ~bit) | (mat->drum[l] & bit));
        }
    }
    /* tidy markers that no longer do anything (a slide into silence, a tie
     * after silence): inaudible, keeps the pattern valid */
    pattern_sanitize(p);
    return mask;
}

uint16_t corr_regen(pattern_t *p, const gen_t *g, uint16_t seed, uint8_t what) ETCH_BANKED
{
    if (p->lock & PLOCK_ALL) return 0;
    corr_regen_material(&scratch, g, seed);
    return corr_regen_apply(p, &scratch, seed, what);
}

void corr_fill(const pattern_t *p, uint16_t rnd, uint16_t *out) ETCH_BANKED
{
    uint8_t st = p->style % STYLE_COUNT;
    uint8_t l;
    for (l = 0; l < NUM_LANES; l++) out[l] = p->drum[l];
    corr_seed(rnd);
    switch (st) {
    case STYLE_ELECTRO:
        out[LANE_SNARE] |= 0xA800 | (corr_rand() & 1 ? 0x0200 : 0);
        out[LANE_KICK] = (out[LANE_KICK] & 0x0FFF) | 0x4000;
        out[LANE_CH] = 0xFFFF;
        break;
    case STYLE_MINIMAL:
    case STYLE_SPARSE:
        out[LANE_SNARE] |= 0x9000;
        out[LANE_OH] |= 0x4040;
        out[LANE_CH] |= 0xA0A0;
        break;
    case STYLE_BROKEN:
        out[LANE_SNARE] |= 0xD200;
        out[LANE_KICK] = (out[LANE_KICK] & 0x0FFF) | 0x2400;
        out[LANE_CH] ^= 0x5A5A;
        break;
    case STYLE_INDUSTRIAL:
        out[LANE_SNARE] |= 0xF000 | (corr_rand() & 1 ? 0x0A00 : 0);
        out[LANE_KICK] |= 0x8800;
        out[LANE_CH] = 0xFFFF;
        out[LANE_OH] = 0x0000;
        break;
    default: /* ACID: 16th snare roll into the next bar, open hat lift */
        out[LANE_SNARE] |= 0xF000 | (corr_rand() & 1 ? 0x0400 : 0x0800);
        out[LANE_CH] |= 0xAAAA;
        out[LANE_OH] |= 0x4000;
        out[LANE_KICK] &= 0x7FFF;
        break;
    }
}

void corr_drum_preset(pattern_t *p, uint8_t style) ETCH_BANKED
{
    const grammar_t *gr = &grammars[style % STYLE_COUNT];
    uint8_t l;
    for (l = 0; l < NUM_LANES; l++) {
        if (p->lock & ((PLOCK_LANE0 << l) | PLOCK_ALL)) continue;
        p->drum[l] = gr->drum[l];
        p->kit[l] = gr->kit[l];
    }
    p->style = style % STYLE_COUNT;
}

/* ------------------------------------------------------------------------ */
static char hexd(uint8_t v)
{
    v &= 15;
    return (char)(v < 10 ? '0' + v : 'A' + v - 10);
}

static int8_t unhex(char c)
{
    if (c >= '0' && c <= '9') return (int8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
    return -1;
}

void corr_code_encode(const gen_t *g, char *out) ETCH_BANKED
{
    out[0] = hexd((uint8_t)(g->seed >> 12));
    out[1] = hexd((uint8_t)(g->seed >> 8));
    out[2] = hexd((uint8_t)(g->seed >> 4));
    out[3] = hexd((uint8_t)g->seed);
    out[4] = hexd(g->key);
    out[5] = hexd(g->scale);
    out[6] = hexd(g->style);
    out[7] = hexd(g->density);
    out[8] = hexd(g->sync);
    out[9] = hexd(g->accent);
    out[10] = hexd(g->slide);
    out[11] = hexd(g->octave);
}

uint8_t corr_code_decode(gen_t *g, const char *in) ETCH_BANKED
{
    int8_t v[12];
    uint8_t i;
    for (i = 0; i < 12; i++) {
        v[i] = unhex(in[i]);
        if (v[i] < 0) return 0;
    }
    if (v[4] > 11 || v[5] >= SCALE_COUNT || v[6] >= STYLE_COUNT || v[11] > 2) return 0;
    g->seed = (uint16_t)((v[0] << 12) | (v[1] << 8) | (v[2] << 4) | v[3]);
    g->key = (uint8_t)v[4];
    g->scale = (uint8_t)v[5];
    g->style = (uint8_t)v[6];
    g->density = (uint8_t)v[7];
    g->sync = (uint8_t)v[8];
    g->accent = (uint8_t)v[9];
    g->slide = (uint8_t)v[10];
    g->octave = (uint8_t)v[11];
    return 1;
}
