/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * CORROSION compatibility corpus.
 *
 * One portable C file, compiled three ways:
 *   - host gcc against the ORIGINAL ETCH sources and against ETCH II's copy
 *     (full-size byte streams, compared with cmp);
 *   - arm-none-eabi-gcc inside a GBA test ROM (tests/compat/gba_main.c),
 *     run in mGBA: proves the ARM build of the port is bit-exact;
 *   - SDCC 4.4.1 (GBDK-2020 4.3.0, the compiler that built ETCH) inside a
 *     Game Boy test ROM linking the ORIGINAL ETCH files, run in mGBA's GB
 *     core: proves the reference is what ETCH on a Game Boy computes.
 *
 * Every section feeds the canonical pattern bytes (note, flags, prob, drum
 * masks, kits, lock, style, gen, spare -- all 64 bytes) and every return
 * value into a CRC-32 and, on the host, into a file.
 *
 * Parameters are drawn from an LCG that is independent of CORROSION, so the
 * corpus is identical on every platform.  CORPUS_N scales the corpus.
 */
#include "corrosion.h"

#ifndef CORPUS_N
#define CORPUS_N 64
#endif

#define N_SECT 7
typedef struct {
    uint32_t magic;            /* 0xC0DEC0DE once complete */
    uint32_t n;                /* CORPUS_N */
    uint32_t crc[N_SECT];
    uint32_t bytes[N_SECT];
} corpus_result_t;

/* ---- sink ---------------------------------------------------------------- */
static uint32_t crc_tab[256];
static uint32_t cur_crc, cur_bytes;
#ifdef CORPUS_HOST
#include <stdio.h>
FILE *corpus_dumpf_hook;
#define dumpf corpus_dumpf_hook
#endif

static void crc_init(void)
{
    uint16_t i;
    uint8_t k;
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        crc_tab[i] = c;
    }
}

static void emit(const void *p, uint16_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t c = cur_crc;
    uint16_t i;
    for (i = 0; i < n; i++) c = crc_tab[(uint8_t)(c ^ b[i])] ^ (c >> 8);
    cur_crc = c;
    cur_bytes += n;
#ifdef CORPUS_HOST
    if (dumpf) fwrite(p, 1, n, dumpf);
#endif
}

static void emit8(uint8_t v) { emit(&v, 1); }
static void emit16(uint16_t v) { uint8_t b[2]; b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); emit(b, 2); }

/* Patterns are emitted field by field (not as a struct image) so the byte
 * stream is independent of compiler struct layout and endianness. */
static void emit_pat(const pattern_t *p)
{
    uint8_t l;
    emit(p->note, 16);
    emit(p->flags, 16);
    emit(p->prob, 16);
    for (l = 0; l < NUM_LANES; l++) emit16(p->drum[l]);
    emit(p->kit, 4);
    emit8(p->lock); emit8(p->style); emit8(p->gen); emit8(p->spare);
}

/* ---- parameter source (NOT the CORROSION PRNG) --------------------------- */
static uint32_t lcg;
static uint16_t lrand(void)
{
    lcg = lcg * 1103515245UL + 12345UL;
    return (uint16_t)(lcg >> 16);
}
static uint8_t lrn(uint8_t n) { return (uint8_t)(lrand() % n); }

static gen_t g;
static pattern_t a, b, c, anc;

static void random_gen(void)
{
    g.seed = lrand();
    g.key = lrn(12);
    g.scale = lrn(SCALE_COUNT);
    g.style = lrn(STYLE_COUNT);
    g.density = lrn(16);
    g.sync = lrn(16);
    g.accent = lrn(16);
    g.slide = lrn(16);
    g.octave = lrn(3);
    g.mutation = lrn(16);
    g.mode = lrn(EVO_COUNT);
}

static void begin(void) { cur_crc = 0xFFFFFFFFUL; cur_bytes = 0; }
static void end(corpus_result_t *r, uint8_t s) { r->crc[s] = ~cur_crc; r->bytes[s] = cur_bytes; }

void corpus_run(corpus_result_t *r)
{
    uint32_t i;
    uint8_t k, s, l;
    char code[13];

    crc_init();
    r->magic = 0;
    r->n = CORPUS_N;

    /* 0: GENERATE (GEN_ALL) from random codes, plus the code text */
    lcg = 0x2601;
    begin();
    for (i = 0; i < (uint32_t)CORPUS_N * 16; i++) {
        random_gen();
        pattern_clear(&a);
        corr_generate(&a, &g, GEN_ALL);
        emit_pat(&a);
        corr_code_encode(&g, code);
        emit(code, 12);
    }
    end(r, 0);

    /* 1: exhaustive-style seed sweep with the default new-project settings */
    lcg = 0x0E7C;
    begin();
    for (i = 0; i < (uint32_t)CORPUS_N * 16; i++) {
        g.key = 9; g.scale = SCALE_MINOR; g.style = (uint8_t)(i % STYLE_COUNT);
        g.density = 9; g.sync = 5; g.accent = 8; g.slide = 7; g.octave = 0;
        g.seed = (uint16_t)(i * 4099UL + 1);
        pattern_clear(&a);
        corr_generate(&a, &g, GEN_ALL);
        emit_pat(&a);
    }
    end(r, 1);

    /* 2: partial generation over existing content with locks */
    lcg = 0x10C5;
    begin();
    for (i = 0; i < (uint32_t)CORPUS_N * 4; i++) {
        random_gen();
        pattern_clear(&a);
        corr_generate(&a, &g, GEN_ALL);
        for (k = 0; k < 3; k++) a.flags[lrn(16)] |= STEP_LOCK;
        a.lock = (uint8_t)(lrand() & 0x1F);
        g.seed = lrand();
        corr_generate(&a, &g, (uint8_t)(1 + lrn(3)));
        emit_pat(&a);
    }
    end(r, 2);

    /* 3: evolution lineages (DRIFT with anchor, CORRODE, STABLE) */
    lcg = 0xD71F;
    begin();
    for (i = 0; i < (uint32_t)CORPUS_N * 2; i++) {
        random_gen();
        pattern_clear(&a);
        corr_generate(&a, &g, GEN_ALL);
        if (lrn(4) == 0) { a.flags[lrn(16)] |= STEP_LOCK; a.lock = (uint8_t)(PLOCK_LANE0 << lrn(4)); }
        if (lrn(8) == 0) a.lock |= PLOCK_BASS;
        memcpy(&anc, &a, sizeof(pattern_t));
        for (k = 0; k < 12; k++) {
            uint8_t mode = lrn(EVO_COUNT);
            uint8_t inten = lrn(16);
            uint16_t sd = lrand();
            uint8_t ch = corr_mutate(&b, &a, (lrn(3) ? &anc : (const pattern_t *)0), &g, mode, inten, sd);
            emit8(ch);
            emit_pat(&b);
            memcpy(&a, &b, sizeof(pattern_t));
        }
    }
    end(r, 3);

    /* 4: REGEN (material + apply) */
    lcg = 0x4E6E;
    begin();
    for (i = 0; i < (uint32_t)CORPUS_N * 4; i++) {
        uint16_t m;
        random_gen();
        pattern_clear(&a);
        corr_generate(&a, &g, GEN_ALL);
        if (lrn(3) == 0) { a.flags[lrn(16)] |= STEP_LOCK; a.lock = (uint8_t)(PLOCK_LANE0 << lrn(4)); }
        m = corr_regen(&a, &g, lrand(), (uint8_t)(1 + lrn(3)));
        emit16(m);
        emit_pat(&a);
        corr_regen_material(&c, &g, lrand());
        emit_pat(&c);
        m = corr_regen_apply(&a, &c, lrand(), GEN_ALL);
        emit16(m);
        emit_pat(&a);
    }
    end(r, 4);

    /* 5: fills, drum presets */
    lcg = 0xF111;
    begin();
    for (i = 0; i < (uint32_t)CORPUS_N * 4; i++) {
        uint16_t f[NUM_LANES];
        random_gen();
        pattern_clear(&a);
        corr_generate(&a, &g, GEN_ALL);
        corr_fill(&a, lrand(), f);
        for (l = 0; l < NUM_LANES; l++) emit16(f[l]);
        corr_drum_preset(&a, lrn(STYLE_COUNT));
        emit_pat(&a);
    }
    end(r, 5);

    /* 6: helpers, sanitize, code decode (valid + invalid) */
    lcg = 0x5EED;
    begin();
    for (k = 0; k < 12; k++)
        for (s = 0; s < SCALE_COUNT; s++)
            for (l = 0; l <= NOTE_MAX; l++)
                emit8(corr_in_scale(l, k, s));
    for (i = 0; i < (uint32_t)CORPUS_N * 4; i++) {
        uint8_t lo, hi, n;
        random_gen();
        corr_range(&g, &lo, &hi);
        emit8(lo); emit8(hi);
        n = lrn(NOTE_MAX + 1);
        emit8(corr_snap(n, &g));
        emit8(corr_scale_step(n, 1, &g));
        emit8(corr_scale_step(n, -1, &g));
        /* sanitize random garbage */
        for (s = 0; s < 16; s++) { a.note[s] = (uint8_t)lrand(); a.flags[s] = (uint8_t)lrand(); a.prob[s] = (uint8_t)lrand(); }
        for (l = 0; l < NUM_LANES; l++) { a.drum[l] = lrand(); a.kit[l] = (uint8_t)lrand(); }
        a.lock = (uint8_t)lrand(); a.style = (uint8_t)lrand(); a.gen = (uint8_t)lrand(); a.spare = (uint8_t)lrand();
        pattern_sanitize(&a);
        emit_pat(&a);
        emit8(pattern_note_count(&a));
        /* decode: a random 12-char string from a hex+junk alphabet */
        {
            static const char alpha[] = "0123456789ABCDEFabcdefGZ ";
            gen_t h;
            uint8_t ok;
            memset(&h, 0, sizeof h);
            for (s = 0; s < 12; s++) code[s] = alpha[lrn(lrn(8) ? 16 : 25)];
            ok = corr_code_decode(&h, code);
            emit8(ok);
            emit16(h.seed); emit8(h.key); emit8(h.scale); emit8(h.style); emit8(h.density);
            emit8(h.sync); emit8(h.accent); emit8(h.slide); emit8(h.octave);
        }
    }
    end(r, 6);

    r->magic = 0xC0DEC0DEUL;
}
