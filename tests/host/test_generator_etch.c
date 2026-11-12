/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Host-side unit tests for CORROSION (src/generator).  Build: make hosttest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "corrosion.h"

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *NN[12] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};

static void print_pattern(const pattern_t *p)
{
    int s, l;
    const char *ln[4] = {"KK", "SN", "CH", "OH"};
    for (s = 0; s < 16; s++) {
        int t = p->flags[s] & STEP_TYPE_MASK;
        if (t == STEP_NOTE) printf("%s%d%c%c ", NN[p->note[s] % 12], p->note[s] / 12 + 1,
                                   p->flags[s] & STEP_ACCENT ? 'A' : '.', p->flags[s] & STEP_SLIDE ? 'S' : '.');
        else if (t == STEP_TIE) printf("===== ");
        else printf("----- ");
    }
    printf("\n");
    for (l = 0; l < 4; l++) {
        printf("   %s ", ln[l]);
        for (s = 0; s < 16; s++) printf("%c", (p->drum[l] >> s) & 1 ? 'x' : '.');
        printf("\n");
    }
}

static gen_t mkgen(uint16_t seed, int style, int scale, int key)
{
    gen_t g;
    memset(&g, 0, sizeof g);
    g.seed = seed; g.style = style; g.scale = scale; g.key = key;
    g.density = 9; g.sync = 6; g.accent = 8; g.slide = 7; g.octave = 1;
    g.mutation = 8; g.mode = EVO_DRIFT;
    return g;
}

static int valid_pattern(const pattern_t *p, char *why)
{
    int s;
    for (s = 0; s < 16; s++) {
        int t = p->flags[s] & STEP_TYPE_MASK;
        if (p->note[s] > NOTE_MAX) { sprintf(why, "note>max at %d", s); return 0; }
        if (t == 3) { sprintf(why, "bad type at %d", s); return 0; }
        if ((p->prob[s] & 0x0F) > 7) { sprintf(why, "prob at %d", s); return 0; }
        if ((p->prob[s] >> 4) >= COND_COUNT) { sprintf(why, "cond at %d", s); return 0; }
        if (p->flags[s] & 0xE0) { sprintf(why, "stray flag bits at %d", s); return 0; }
        if (!(p->flags[s] & STEP_LOCK)) {
            if ((p->flags[s] & STEP_SLIDE) && (p->flags[(s + 1) & 15] & STEP_TYPE_MASK) != STEP_NOTE) {
                sprintf(why, "slide into non-note at %d", s); return 0;
            }
            if (t == STEP_TIE && (p->flags[(s + 15) & 15] & STEP_TYPE_MASK) == STEP_REST) {
                sprintf(why, "tie after rest at %d", s); return 0;
            }
        }
    }
    for (s = 0; s < 4; s++) if (p->kit[s] > 3) { sprintf(why, "kit"); return 0; }
    if (p->style >= STYLE_COUNT) { sprintf(why, "style"); return 0; }
    return 1;
}

static int notes_in_scale(const pattern_t *p, const gen_t *g)
{
    int s;
    uint8_t lo, hi;
    corr_range(g, &lo, &hi);
    for (s = 0; s < 16; s++) {
        if ((p->flags[s] & STEP_TYPE_MASK) != STEP_NOTE) continue;
        if (!corr_in_scale(p->note[s], g->key, g->scale)) return 0;
        if (p->note[s] < lo || p->note[s] > hi) return 0;
    }
    return 1;
}

static int diff_count(const pattern_t *a, const pattern_t *b)
{
    int s, l, d = 0;
    for (s = 0; s < 16; s++)
        if (a->note[s] != b->note[s] || a->flags[s] != b->flags[s] || a->prob[s] != b->prob[s]) d++;
    for (l = 0; l < 4; l++) {
        uint16_t x = a->drum[l] ^ b->drum[l];
        while (x) { d += x & 1; x >>= 1; }
    }
    return d;
}

/* best (over periods 2..8) fraction of sounding steps repeated one period later */
static double repetition(const pattern_t *p)
{
    int s, P;
    double best = 0;
    for (P = 2; P <= 8; P++) {
        int same = 0, tot = 0;
        for (s = 0; s + P < 16; s++) {
            int t1 = p->flags[s] & 3, t2 = p->flags[s + P] & 3;
            if (t1 == STEP_NOTE || t2 == STEP_NOTE) {
                tot++;
                if (t1 == t2 && p->note[s] == p->note[s + P]) same++;
            }
        }
        if (tot && (double)same / tot > best) best = (double)same / tot;
    }
    return best;
}

int main(void)
{
    pattern_t a, b, c;
    gen_t g;
    int st, sc, k, i, seed;
    char why[64];

    printf("== CORROSION host tests ==\n");

    /* 1. determinism */
    for (st = 0; st < STYLE_COUNT; st++) {
        g = mkgen(0x1234 + st, st, SCALE_MINOR, 9);
        pattern_clear(&a); pattern_clear(&b);
        corr_generate(&a, &g, GEN_ALL);
        corr_generate(&b, &g, GEN_ALL);
        CHECK(memcmp(&a, &b, sizeof a) == 0, "same seed differs (style %d)", st);
    }

    /* 2. different seeds differ */
    {
        int distinct = 0;
        for (seed = 1; seed <= 50; seed++) {
            g = mkgen((uint16_t)seed * 977, STYLE_ACID, SCALE_MINOR, 0);
            pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
            g.seed = (uint16_t)(seed * 977 + 1);
            pattern_clear(&b); corr_generate(&b, &g, GEN_ALL);
            if (memcmp(&a, &b, sizeof a)) distinct++;
        }
        CHECK(distinct >= 45, "only %d/50 seed pairs differ", distinct);
    }

    /* 3. scale + range + validity over a big sweep */
    {
        int bad = 0, invalid = 0, total = 0;
        for (st = 0; st < STYLE_COUNT; st++)
            for (sc = 0; sc < SCALE_COUNT; sc++)
                for (k = 0; k < 12; k++)
                    for (i = 0; i < 3; i++) {
                        g = mkgen((uint16_t)(st * 1000 + sc * 100 + k * 7 + i + 1), st, sc, k);
                        g.octave = i;
                        g.density = (uint8_t)((k + i * 5) & 15);
                        pattern_clear(&a);
                        corr_generate(&a, &g, GEN_ALL);
                        total++;
                        if (!notes_in_scale(&a, &g)) bad++;
                        if (!valid_pattern(&a, why)) { invalid++; if (invalid < 3) printf("  invalid: %s\n", why); }
                    }
        CHECK(bad == 0, "%d/%d generated patterns leave scale/range", bad, total);
        CHECK(invalid == 0, "%d/%d generated patterns invalid", invalid, total);
        printf("  sweep: %d patterns generated, all in scale/range and valid\n", total);
    }

    /* 4. density is meaningful */
    {
        int lo_n = 0, hi_n = 0;
        for (seed = 1; seed <= 40; seed++) {
            g = mkgen((uint16_t)seed * 31, STYLE_ACID, SCALE_MINOR, 0);
            g.density = 2; pattern_clear(&a); corr_generate(&a, &g, GEN_BASS); lo_n += pattern_note_count(&a);
            g.density = 15; pattern_clear(&a); corr_generate(&a, &g, GEN_BASS); hi_n += pattern_note_count(&a);
        }
        CHECK(hi_n > lo_n * 3 / 2, "density 15 (%d notes) not denser than density 2 (%d)", hi_n, lo_n);
        printf("  density: avg notes d2=%.1f d15=%.1f\n", lo_n / 40.0, hi_n / 40.0);
    }

    /* 5. musical structure: motif repetition well above chance */
    {
        double rep = 0;
        int n = 0;
        for (seed = 1; seed <= 60; seed++) {
            g = mkgen((uint16_t)seed * 131, seed % 3 == 0 ? STYLE_MINIMAL : STYLE_ACID, SCALE_MINOR, 0);
            pattern_clear(&a); corr_generate(&a, &g, GEN_BASS);
            rep += repetition(&a); n++;
        }
        rep /= n;
        CHECK(rep > 0.6, "average beat repetition %.2f too low (random lines)", rep);
        printf("  structure: average beat-to-beat repetition %.2f\n", rep);
    }

    /* 6. STABLE never changes anything */
    g = mkgen(77, STYLE_ACID, SCALE_MINOR, 0);
    pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
    CHECK(corr_mutate(&b, &a, NULL, &g, EVO_STABLE, 15, 99) == 0, "STABLE reported changes");
    CHECK(memcmp(&a, &b, sizeof a) == 0, "STABLE changed the pattern");

    /* 7. locks: steps, lanes, bass, whole pattern; over many generations */
    {
        int viol = 0, invalid = 0, mode;
        for (mode = EVO_DRIFT; mode <= EVO_CORRODE; mode++) {
            for (seed = 1; seed <= 20; seed++) {
                g = mkgen((uint16_t)seed * 7, seed % STYLE_COUNT, seed % SCALE_COUNT, seed % 12);
                pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
                a.flags[1] |= STEP_LOCK; a.flags[6] |= STEP_LOCK; a.flags[11] |= STEP_LOCK;
                a.lock = PLOCK_LANE0 << LANE_KICK | PLOCK_LANE0 << LANE_OH;
                memcpy(&c, &a, sizeof a);
                for (i = 0; i < 200; i++) {
                    corr_mutate(&b, &a, NULL, &g, (uint8_t)mode, 15, (uint16_t)(i * 13 + seed));
                    memcpy(&a, &b, sizeof a);
                    if (a.note[1] != c.note[1] || a.flags[1] != c.flags[1] || a.prob[1] != c.prob[1]) viol++;
                    if (a.note[6] != c.note[6] || a.flags[6] != c.flags[6]) viol++;
                    if (a.note[11] != c.note[11] || a.flags[11] != c.flags[11]) viol++;
                    if (a.drum[LANE_KICK] != c.drum[LANE_KICK] || a.drum[LANE_OH] != c.drum[LANE_OH]) viol++;
                    if (!valid_pattern(&a, why)) { invalid++; if (invalid < 3) printf("  invalid after mutation: %s\n", why); }
                    if (!notes_in_scale(&a, &g)) {
                        /* locked steps may legitimately be out of scale; check unlocked only */
                        int s2, out = 0; uint8_t lo, hi; corr_range(&g, &lo, &hi);
                        for (s2 = 0; s2 < 16; s2++) if (!(a.flags[s2] & STEP_LOCK) && (a.flags[s2] & 3) == STEP_NOTE &&
                            (!corr_in_scale(a.note[s2], g.key, g.scale) || a.note[s2] < lo || a.note[s2] > hi)) out++;
                        if (out) invalid++;
                    }
                }
            }
        }
        CHECK(viol == 0, "%d lock violations", viol);
        CHECK(invalid == 0, "%d invalid patterns during mutation", invalid);
        printf("  locks: 8000 mutations, %d violations, %d invalid\n", viol, invalid);

        g = mkgen(5, STYLE_ACID, SCALE_MINOR, 0);
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        a.lock = PLOCK_BASS;
        memcpy(&c, &a, sizeof a);
        for (i = 0; i < 100; i++) { corr_mutate(&b, &a, NULL, &g, EVO_CORRODE, 15, (uint16_t)i); memcpy(&a, &b, sizeof a); }
        CHECK(!memcmp(a.note, c.note, 16) && !memcmp(a.flags, c.flags, 16) && !memcmp(a.prob, c.prob, 16), "bass lock violated");

        a.lock = PLOCK_ALL;
        memcpy(&c, &a, sizeof a);
        CHECK(corr_mutate(&b, &a, NULL, &g, EVO_CORRODE, 15, 1) == 0 && !memcmp(&b, &c, sizeof b), "pattern lock violated");

        /* generate respects locks */
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        a.flags[3] = STEP_NOTE | STEP_LOCK; a.note[3] = 40;
        a.lock = PLOCK_LANE0 << LANE_SNARE; a.drum[LANE_SNARE] = 0x0F0F;
        g.seed = 999;
        corr_generate(&a, &g, GEN_ALL);
        CHECK(a.note[3] == 40 && a.flags[3] == (STEP_NOTE | STEP_LOCK), "generate overwrote locked step");
        CHECK(a.drum[LANE_SNARE] == 0x0F0F, "generate overwrote locked lane");
    }

    /* 8. DRIFT: every channel moves every phrase, bounded around the anchor;
     *    CORRODE travels much further */
    {
        double dd = 0, dc = 0, rd = 0, rc = 0, bd = 0, bc = 0;
        int every = 0, total = 0, maxdiv = 0, l, s2;
        for (seed = 1; seed <= 30; seed++) {
            g = mkgen((uint16_t)seed * 17, STYLE_ACID, SCALE_MINOR, 0);
            pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
            memcpy(&c, &a, sizeof a);
            for (i = 0; i < 16; i++) {
                int bass_changed = 0, lanes_changed = 0;
                corr_mutate(&b, &a, &c, &g, EVO_DRIFT, 8, (uint16_t)i);
                for (s2 = 0; s2 < 16; s2++)
                    if (a.note[s2] != b.note[s2] || a.flags[s2] != b.flags[s2] || a.prob[s2] != b.prob[s2]) bass_changed = 1;
                for (l = 0; l < 4; l++) if (a.drum[l] != b.drum[l]) lanes_changed++;
                if (bass_changed && lanes_changed == 4) every++;
                total++;
                memcpy(&a, &b, sizeof a);
                if (diff_count(&a, &c) > maxdiv) maxdiv = diff_count(&a, &c);
            }
            dd += diff_count(&a, &c); rd += repetition(&a);
            for (s2 = 0; s2 < 16; s2++) if (a.note[s2] != c.note[s2] || a.flags[s2] != c.flags[s2]) bd++;
            memcpy(&a, &c, sizeof a);
            for (i = 0; i < 16; i++) { corr_mutate(&b, &a, NULL, &g, EVO_CORRODE, 8, (uint16_t)i); memcpy(&a, &b, sizeof a); }
            dc += diff_count(&a, &c); rc += repetition(&a);
            for (s2 = 0; s2 < 16; s2++) if (a.note[s2] != c.note[s2] || a.flags[s2] != c.flags[s2]) bc++;
        }
        dd /= 30; dc /= 30; rd /= 30; rc /= 30; bd /= 30; bc /= 30;
        printf("  DRIFT  (16 phrases, intensity 8): every channel changed in %d/%d phrases, divergence %.1f (max %d), bass steps changed %.1f, repetition %.2f\n",
               every, total, dd, maxdiv, bd, rd);
        printf("  CORRODE(16 phrases, intensity 8): divergence %.1f, bass steps changed %.1f, repetition %.2f\n", dc, bc, rc);
        CHECK(every * 100 >= total * 95, "DRIFT left a channel unchanged in %d/%d phrases", total - every, total);
        /* bound: bass (2 + 8/6) + 1 for an accent move, each lane 1 + 8/8 */
        CHECK(maxdiv <= 4 + 4 * 2, "DRIFT exceeded its bound (max divergence %d)", maxdiv);
        CHECK(bd <= 4.5, "DRIFT bass wandered too far (%.1f steps)", bd);
        CHECK(dc > dd * 1.5, "CORRODE does not diverge further than DRIFT");
        CHECK(bc > bd * 2, "CORRODE bass does not travel further than DRIFT");
        CHECK(rc > 0.35, "CORRODE lost all repetition (incoherent)");
        CHECK(rd > 0.4, "DRIFT lost the groove's repetition");
    }

    /* 8b. DRIFT leaves locked lanes alone and still moves the others */
    {
        g = mkgen(91, STYLE_ACID, SCALE_MINOR, 0);
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        a.lock = PLOCK_LANE0 << LANE_KICK;
        memcpy(&c, &a, sizeof a);
        corr_mutate(&b, &a, &c, &g, EVO_DRIFT, 8, 7);
        CHECK(b.drum[LANE_KICK] == a.drum[LANE_KICK], "DRIFT touched a locked lane");
        CHECK(b.drum[LANE_CH] != a.drum[LANE_CH], "DRIFT did not move the hats");
    }

    /* 8c. REGEN: exactly half the bar, nothing else, locks respected */
    {
        int bad_count = 0, outside = 0, inscale = 0, runs = 0, lockv = 0, s2, l2;
        for (seed = 1; seed <= 200; seed++) {
            uint16_t mask; int cnt = 0;
            g = mkgen((uint16_t)(seed * 37), seed % STYLE_COUNT, seed % SCALE_COUNT, seed % 12);
            pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
            if (seed % 3 == 0) { a.flags[2] |= STEP_LOCK; a.lock = PLOCK_LANE0 << LANE_KICK; }
            memcpy(&c, &a, sizeof a);
            mask = corr_regen(&a, &g, (uint16_t)(seed * 977 + 5), GEN_ALL);
            for (s2 = 0; s2 < 16; s2++) cnt += (mask >> s2) & 1;
            if (cnt != 8) bad_count++;
            for (s2 = 0; s2 < 16; s2++) {
                if ((mask >> s2) & 1) continue;
                /* outside the mask only validity corrections are allowed: a slide
                 * that no longer leads into a note, or a tie whose preceding step
                 * became a rest (it turns into a rest) */
                {
                    int tie_fix = (c.flags[s2] & 3) == STEP_TIE && (a.flags[s2] & 3) == STEP_REST
                                  && (a.flags[(s2 + 15) & 15] & 3) == STEP_REST;   /* also cascades through tie chains */
                    if (!tie_fix && (a.note[s2] != c.note[s2] || (a.flags[s2] & ~STEP_SLIDE) != (c.flags[s2] & ~STEP_SLIDE) || a.prob[s2] != c.prob[s2])) outside++;
                }
                for (l2 = 0; l2 < 4; l2++) if (((a.drum[l2] ^ c.drum[l2]) >> s2) & 1) outside++;
            }
            if (seed % 3 == 0) {
                if (a.note[2] != c.note[2] || a.flags[2] != c.flags[2] || a.drum[LANE_KICK] != c.drum[LANE_KICK]) lockv++;
            }
            if (!notes_in_scale(&a, &g)) {
                for (s2 = 0; s2 < 16; s2++)          /* locked step may be out of scale: ignore */
                    if (!(a.flags[s2] & STEP_LOCK) && (a.flags[s2] & 3) == STEP_NOTE && !corr_in_scale(a.note[s2], g.key, g.scale)) { inscale++; break; }
            }
            if (!valid_pattern(&a, why)) inscale++;
            runs++;
        }
        printf("  REGEN: %d runs, %d not exactly 8 steps, %d changes outside the chosen steps, %d lock violations, %d out-of-scale/invalid\n",
               runs, bad_count, outside, lockv, inscale);
        CHECK(bad_count == 0, "REGEN did not pick exactly 8 steps");
        CHECK(outside == 0, "REGEN changed steps it did not pick");
        CHECK(lockv == 0, "REGEN touched locked material");
        CHECK(inscale == 0, "REGEN produced out-of-scale or invalid material");

        g = mkgen(4, STYLE_ACID, SCALE_MINOR, 0);
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        memcpy(&c, &a, sizeof a);
        corr_regen(&a, &g, 1234, GEN_BASS);
        CHECK(!memcmp(a.drum, c.drum, sizeof a.drum), "REGEN BASS changed the drums");
        memcpy(&a, &c, sizeof a);
        corr_regen(&a, &g, 1234, GEN_DRUMS);
        CHECK(!memcmp(a.note, c.note, 16) && !memcmp(a.prob, c.prob, 16), "REGEN DRUMS changed the bass");
        a.lock = PLOCK_ALL;
        memcpy(&c, &a, sizeof a);
        CHECK(corr_regen(&a, &g, 99, GEN_ALL) == 0 && !memcmp(&a, &c, sizeof a), "REGEN ignored the pattern lock");
        /* two regens with different seeds give different results */
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL); memcpy(&b, &a, sizeof a);
        corr_regen(&a, &g, 111, GEN_ALL); corr_regen(&b, &g, 222, GEN_ALL);
        CHECK(memcmp(&a, &b, sizeof a) != 0, "REGEN ignores its seed");
    }

    /* 9. mutation determinism */
    g = mkgen(4242, STYLE_ELECTRO, SCALE_DORIAN, 2);
    pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
    corr_mutate(&b, &a, NULL, &g, EVO_CORRODE, 10, 55);
    corr_mutate(&c, &a, NULL, &g, EVO_CORRODE, 10, 55);
    CHECK(!memcmp(&b, &c, sizeof b), "mutation not deterministic");
    CHECK(b.gen == a.gen + 1, "generation counter not advanced");

    /* 10. seed code */
    {
        char code[13] = {0};
        gen_t h;
        g = mkgen(0xBEEF, STYLE_BROKEN, SCALE_PHRYGIAN, 11);
        g.density = 13; g.sync = 2; g.accent = 15; g.slide = 0; g.octave = 2;
        corr_code_encode(&g, code);
        memset(&h, 0, sizeof h);
        CHECK(corr_code_decode(&h, code), "decode failed for %s", code);
        CHECK(h.seed == g.seed && h.key == g.key && h.scale == g.scale && h.style == g.style &&
              h.density == g.density && h.sync == g.sync && h.accent == g.accent &&
              h.slide == g.slide && h.octave == g.octave, "seed code roundtrip mismatch");
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        h.mutation = 3; h.mode = EVO_CORRODE;      /* not part of the code */
        pattern_clear(&b); corr_generate(&b, &h, GEN_ALL);
        CHECK(!memcmp(&a, &b, sizeof a), "seed code does not reproduce the groove");
        CHECK(!corr_code_decode(&h, "ZZZZ00000000"), "decode accepted garbage");
        printf("  seed code example: %.4s-%.4s-%.4s\n", code, code + 4, code + 8);
    }

    /* 11. fill keeps the groove's identity but adds activity */
    g = mkgen(3, STYLE_ACID, SCALE_MINOR, 0);
    pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
    {
        uint16_t f[4];
        corr_fill(&a, 1234, f);
        CHECK((f[LANE_SNARE] & 0xF000) == 0xF000, "acid fill has no snare roll");
        CHECK((f[LANE_KICK] & 0x1111) == (a.drum[LANE_KICK] & 0x1111), "fill dropped the downbeat kicks");
    }

    /* show some material */
    printf("\n  examples (seed 0x2601, A minor, octave 1):\n");
    for (st = 0; st < STYLE_COUNT; st++) {
        g = mkgen(0x2601, st, SCALE_MINOR, 9);
        pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
        printf("  %s\n  ", corr_style_names[st]);
        print_pattern(&a);
    }
    printf("\n  CORRODE lineage of the ACID line (generations 0, 4, 8, 12):\n");
    g = mkgen(0x2601, STYLE_ACID, SCALE_MINOR, 9);
    pattern_clear(&a); corr_generate(&a, &g, GEN_ALL);
    for (i = 0; i <= 12; i++) {
        if (i % 4 == 0) { printf("  g%-2d ", i); print_pattern(&a); }
        corr_mutate(&b, &a, NULL, &g, EVO_CORRODE, 8, (uint16_t)(i + 1));
        memcpy(&a, &b, sizeof a);
    }

    printf("\n%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
