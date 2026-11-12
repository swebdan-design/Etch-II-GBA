/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Host tests for what ETCH II adds around CORROSION.  Build: make hosttest
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "etch2.h"
#include "expand.h"
#include "instruments.h"

project2_t proj;
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static gen_t mk(uint16_t seed, int st)
{
    gen_t g;
    memset(&g, 0, sizeof g);
    g.seed = seed; g.style = (uint8_t)st; g.key = 9; g.density = 9; g.sync = 5; g.accent = 8; g.slide = 7;
    return g;
}

/* the next 32 values of CORROSION's private PRNG after corr_seed(s) */
static void rng_seq(uint16_t s, uint8_t *out, int interleave)
{
    int i;
    corr_seed(s);
    for (i = 0; i < 32; i++) {
        if (interleave) {
            /* everything ETCH II does around CORROSION, between draws */
            xpattern_t x; pattern_t p; gen_t g = mk(77, i % 6); char c[16]; uint16_t xs;
            pattern_clear(&p);
            xpattern_clear(&x);
            expand_generate(&x, &p, &g, (uint16_t)(i * 31), (uint8_t)i, XG_ALL);
            ext_code_encode(&g, 0xBEEF, c);
            ext_code_decode(&g, &xs, c);
            patch_default(&proj.patch[i & 7], (uint8_t)(i & 7));
            project_defaults(&proj);
            (void)corr_in_scale((uint8_t)i, 3, 2);
        }
        out[i] = corr_rand();
    }
}

int main(void)
{
    pattern_t a, b;
    xpattern_t x1, x2;
    gen_t g;
    int i, s;
    printf("== ETCH II host tests ==\n");

    /* 1. data model: the ORIGINAL pattern format is untouched */
    CHECK(sizeof(pattern_t) == 64, "pattern_t is %u bytes", (unsigned)sizeof(pattern_t));
    CHECK(sizeof(gen_t) == 12, "gen_t is %u bytes", (unsigned)sizeof(gen_t));
    CHECK(sizeof(xpattern_t) == 152, "xpattern_t is %u bytes", (unsigned)sizeof(xpattern_t));
    CHECK(sizeof(project2_t) + 32 <= 8192, "project does not fit a save slot (%u)", (unsigned)sizeof(project2_t));

    /* 2. random stream isolation: expansion, codes, patches never touch CORROSION's PRNG */
    {
        uint8_t r0[32], r1[32];
        rng_seq(0x2601, r0, 0);
        rng_seq(0x2601, r1, 1);
        CHECK(!memcmp(r0, r1, 32), "ETCH II code consumed CORROSION random numbers");
    }

    /* 3. generation is unaffected by anything around it */
    for (s = 0; s < 6; s++) {
        g = mk((uint16_t)(0x1000 + s), s);
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        xpattern_clear(&x1); expand_generate(&x1, &a, &g, 0x1234, 0, XG_ALL);
        pattern_clear(&b); corr_generate(&b, &g, GEN_ALL);
        CHECK(!memcmp(&a, &b, sizeof a), "generation changed after expansion (style %d)", s);
    }

    /* 4. expansion: deterministic, never writes the core, respects locks, in scale */
    {
        int bad = 0, sc;
        for (i = 0; i < 300; i++) {
            pattern_t core, core0;
            g = mk((uint16_t)(i * 211 + 7), i % 6);
            g.scale = (uint8_t)(i % SCALE_COUNT); g.key = (uint8_t)(i % 12);
            pattern_clear(&core); corr_generate(&core, &g, GEN_ALL);
            core0 = core;
            xpattern_clear(&x1); xpattern_clear(&x2);
            expand_generate(&x1, &core, &g, (uint16_t)i, (uint8_t)(i & 7), XG_ALL);
            expand_generate(&x2, &core, &g, (uint16_t)i, (uint8_t)(i & 7), XG_ALL);
            if (memcmp(&x1, &x2, sizeof x1)) bad++;
            if (memcmp(&core, &core0, sizeof core)) bad++;
            for (sc = 0; sc < XT_COUNT; sc++) {
                int st;
                for (st = 0; st < 16; st++) {
                    uint8_t f = x1.trk[sc].flags[st], n = x1.trk[sc].note[st];
                    if ((f & STEP_TYPE_MASK) == STEP_NOTE && !corr_in_scale(n, g.key, g.scale)) bad++;
                    if ((f & STEP_TYPE_MASK) == STEP_NOTE && (n < 12 || n > 108)) bad++;
                }
                if (x1.len[sc] < 1 || x1.len[sc] > 16) bad++;
            }
        }
        CHECK(bad == 0, "expansion: %d determinism/core/scale/length problems in 300 runs", bad);
        /* locks */
        g = mk(99, 0);
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        xpattern_clear(&x1);
        x1.trk[XT_LEAD].flags[3] = STEP_NOTE | STEP_LOCK; x1.trk[XT_LEAD].note[3] = 77;
        expand_generate(&x1, &a, &g, 5, 1, XG_ALL);
        CHECK(x1.trk[XT_LEAD].note[3] == 77 && x1.trk[XT_LEAD].flags[3] == (STEP_NOTE | STEP_LOCK), "expansion overwrote a locked step");
        /* selective generation only touches the selected track */
        xpattern_clear(&x1); expand_generate(&x1, &a, &g, 5, 1, XG_ALL);
        x2 = x1; expand_generate(&x2, &a, &g, 6, 2, XG_LEAD);
        CHECK(!memcmp(&x1.trk[XT_CHORD], &x2.trk[XT_CHORD], sizeof(xtrack_t)) && !memcmp(&x1.trk[XT_DRONE], &x2.trk[XT_DRONE], sizeof(xtrack_t)),
              "XG_LEAD changed other tracks");
    }

    /* 5. codes: the legacy 12 digits are extractable from the extended code */
    {
        int bad = 0;
        for (i = 0; i < 2000; i++) {
            gen_t h, k;
            uint16_t xs = 0;
            char c12[12], c16[16];
            g = mk((uint16_t)(i * 7919), i % 6);
            g.key = (uint8_t)(i % 12); g.scale = (uint8_t)(i % 6); g.octave = (uint8_t)(i % 3);
            g.density = (uint8_t)(i & 15); g.sync = (uint8_t)((i >> 4) & 15);
            corr_code_encode(&g, c12);
            ext_code_encode(&g, (uint16_t)(i * 13), c16);
            if (memcmp(c12, c16, 12)) bad++;
            memset(&h, 0, sizeof h); memset(&k, 0, sizeof k);
            if (!ext_code_decode(&h, &xs, c16) || xs != (uint16_t)(i * 13)) bad++;
            if (!corr_code_decode(&k, c16)) bad++;                         /* first 12 digits = ETCH code */
            if (memcmp(&h, &k, sizeof h)) bad++;
        }
        CHECK(bad == 0, "extended/legacy code problems: %d", bad);
    }

    /* 6. every default patch is within its parameter ranges */
    {
        int t, p, bad = 0;
        for (t = 0; t < TRK_COUNT; t++) {
            patch_t pt;
            uint8_t def;
            patch_default(&pt, (uint8_t)t);
            def = param_def_of(&pt, (uint8_t)t);
            for (p = 0; p < P_COUNT; p++) if (param_defs[def][p].max && pt.p[p] > param_defs[def][p].max) bad++;
        }
        CHECK(bad == 0, "%d default parameters out of range", bad);
    }

    printf("\n%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
