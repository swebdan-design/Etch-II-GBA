/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Album composer (host tool).  Builds the eight living compositions.
 *
 * Provenance rules (documented per pattern in the recipe file):
 *   - the core groove of every piece (pattern 01) is exactly what the
 *     ORIGINAL CORROSION generator makes from the piece's 12-digit legacy
 *     seed code: enter that code on ETCH I and you hear the same groove;
 *   - further core patterns are made with ORIGINAL CORROSION operations
 *     (GENERATE with other settings, REGEN, CORRODE lineages with fixed
 *     seeds), occasionally followed by a hand edit, which is logged;
 *   - extension tracks (LEAD, CHORD, DRONE, TEXTURE) come from the ETCH II
 *     EXPANSION generator or are written by hand -- never from CORROSION.
 * usage: compose_album OUT.c RECIPE.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "etch2.h"
#include "expand.h"
#include "instruments.h"

project2_t proj;                    /* instruments.c / album share the type */
static FILE *rc;
static int npat;

static void logf_(const char *fmt, const char *a, int b) { fprintf(rc, fmt, a, b); }

static void P(uint8_t trk, uint8_t eng, const uint8_t *v)
{
    patch_t *p = &proj.patch[trk];
    int i;
    p->engine = eng;
    for (i = 0; i < P_COUNT; i++) p->p[i] = v[i];
    p->level = v[15]; p->pan = v[16]; p->send = v[17]; p->crush = v[18];
    p->lfo_rate = v[19]; p->lfo_depth = v[20]; p->lfo_dest = v[21]; p->lfo_shape = v[22];
}
#define PATCH(t, e, ...) do { static const uint8_t v_[23] = { __VA_ARGS__ }; P(t, e, v_); } while (0)

static void code_of(const gen_t *g, char *out)
{
    char c[12];
    corr_code_encode(g, c);
    sprintf(out, "%.4s-%.4s-%.4s", c, c + 4, c + 8);
}

/* ---- pattern construction with ORIGINAL CORROSION ---------------------------- */
static void gen_core(int i, const gen_t *g, const char *why)
{
    char code[16];
    pattern_clear(&proj.pat[i]);
    corr_generate(&proj.pat[i], g, GEN_ALL);
    code_of(g, code);
    fprintf(rc, "  P%02d core: CORROSION GENERATE from code %s %s\n", i + 1, code, why);
    if (i + 1 > npat) npat = i + 1;
}

static void copy_pat(int dst, int src)
{
    proj.pat[dst] = proj.pat[src];
    if (dst + 1 > npat) npat = dst + 1;
}

static void regen(int i, const gen_t *g, uint16_t seed, uint8_t what)
{
    uint16_t m = corr_regen(&proj.pat[i], g, seed, what);
    fprintf(rc, "  P%02d core: + CORROSION REGEN %s seed %04X (steps %04X)\n", i + 1,
            what == GEN_BASS ? "BASS" : what == GEN_DRUMS ? "DRUMS" : "ALL", seed, m);
}

static void corrode(int i, const gen_t *g, int n, uint16_t seed, uint8_t mode, uint8_t inten)
{
    pattern_t b, anc = proj.pat[i];
    int k;
    for (k = 0; k < n; k++) {
        corr_mutate(&b, &proj.pat[i], mode == EVO_DRIFT ? &anc : NULL, g, mode, inten, (uint16_t)(seed + k));
        proj.pat[i] = b;
    }
    fprintf(rc, "  P%02d core: + %d x CORROSION %s (intensity %d, seeds %04X..)\n", i + 1, n,
            mode == EVO_DRIFT ? "DRIFT" : "CORRODE", inten, seed);
}

static void edit(int i, const char *what) { fprintf(rc, "  P%02d core: + hand edit: %s\n", i + 1, what); }

static void expand(int i, const gen_t *g, uint16_t xseed, uint8_t variant, uint8_t which)
{
    expand_generate(&proj.xpat[i], &proj.pat[i], g, xseed, variant, which);
    fprintf(rc, "  P%02d ext : EXPANSION generator xseed %04X variant %d tracks %s%s%s%s\n", i + 1, xseed, variant,
            which & XG_LEAD ? "LEAD " : "", which & XG_CHORD ? "CHORD " : "", which & XG_DRONE ? "DRONE " : "",
            which & XG_TEXTURE ? "TEXTURE" : "");
}

__attribute__((unused)) static void xclear(int i, int xt) { memset(&proj.xpat[i].trk[xt], 0, sizeof(xtrack_t)); }

/* hand-written extension line: notes as MIDI, '.'=rest '-'=tie 'n'=note 'a'=accent 's'=slide */
static void xline(int i, int xt, const char *steps, const uint8_t *notes, const char *why)
{
    int s;
    xtrack_t *t = &proj.xpat[i].trk[xt];
    for (s = 0; s < 16; s++) {
        char c = steps[s];
        t->note[s] = notes[s];
        t->flags[s] = c == 'n' ? STEP_NOTE : c == 'a' ? (STEP_NOTE | STEP_ACCENT) : c == 's' ? (STEP_NOTE | STEP_SLIDE)
                    : c == '-' ? STEP_TIE : c == '?' ? (uint8_t)(STEP_NOTE | (3 << XF_PROB_SHIFT)) : 0;
    }
    proj.xpat[i].present = 1;
    fprintf(rc, "  P%02d ext : hand-written %s: %s\n", i + 1, xt == XT_LEAD ? "LEAD" : xt == XT_CHORD ? "CHORD" : xt == XT_DRONE ? "DRONE" : "TEXTURE", why);
}

static void drop_track(int xt, const char *why)
{
    int i;
    for (i = 0; i < E2_PATTERNS; i++) memset(&proj.xpat[i].trk[xt], 0, sizeof(xtrack_t));
    fprintf(rc, "  arrangement: DRONE layer removed (%s)\n", why);
}

static void song(const uint8_t *pats, const uint8_t *reps, int n)
{
    int k;
    for (k = 0; k < E2_SONG_LEN; k++) { proj.song.pat[k] = 0xFF; proj.song.rep[k] = 1; }
    for (k = 0; k < n; k++) { proj.song.pat[k] = pats[k]; proj.song.rep[k] = reps[k]; }
    proj.song.song = 1;
    fprintf(rc, "  SONG:");
    for (k = 0; k < n; k++) fprintf(rc, " P%02d x%d", pats[k] + 1, reps[k]);
    fprintf(rc, " (loops)\n");
}

static void begin(const char *name, uint8_t id)
{
    project_defaults(&proj);
    memset(proj.name, ' ', E2_NAME_LEN);
    memcpy(proj.name, name, strlen(name));
    proj.album_id = id;
    proj.compat = COMPAT_EXPANDED;
    npat = 0;
    fprintf(rc, "\n== %d. %s ==\n", id + 1, name);
}

/* ---- output ---------------------------------------------------------------------- */
static FILE *out;
static char codes[8][16];
static int npats[8];

static void dump_bytes(const char *name, const uint8_t *b, size_t n)
{
    size_t i;
    fprintf(out, "static const uint8_t %s[%u] = {", name, (unsigned)n);
    for (i = 0; i < n; i++) fprintf(out, "%s0x%02X,", i % 16 ? "" : "\n  ", b[i]);
    fprintf(out, "\n};\n");
}

static void emit(int id)
{
    char nm[32];
    int i;
    gen_t g0 = proj.gen;
    proj.gen.mode = EVO_DRIFT;                       /* every piece begins in DRIFT */
    sprintf(nm, "a%d_hdr", id);
    dump_bytes(nm, (const uint8_t *)&proj, offsetof(project2_t, pat));
    sprintf(nm, "a%d_pat", id);
    dump_bytes(nm, (const uint8_t *)proj.pat, sizeof(pattern_t) * npat);
    sprintf(nm, "a%d_xpat", id);
    dump_bytes(nm, (const uint8_t *)proj.xpat, sizeof(xpattern_t) * npat);
    npats[id] = npat;
    (void)g0; (void)i;
}

int main(int argc, char **argv)
{
    gen_t g;
    if (argc < 3) return 2;
    out = fopen(argv[1], "w");
    rc = fopen(argv[2], "w");
    fprintf(out, "/* AUTOGENERATED by tools/compose_album.c -- do not edit. */\n");
    fprintf(out, "/* ETCH II -- A Generative Synthesis Machine -- by Synth Centric: the album */\n");
    fprintf(out, "#include <stddef.h>\n#include <string.h>\n#include \"album.h\"\n");
    fprintf(out, "_Static_assert(offsetof(project2_t, pat) == %u, \"layout\");\n", (unsigned)offsetof(project2_t, pat));
    fprintf(out, "_Static_assert(sizeof(project2_t) == %u, \"layout\");\n", (unsigned)sizeof(project2_t));
    fprintf(out, "_Static_assert(sizeof(xpattern_t) == %u, \"layout\");\n", (unsigned)sizeof(xpattern_t));
    fprintf(rc, "ETCH II -- album recipe (generated by tools/compose_album.c)\n");
    fprintf(rc, "Core patterns: ORIGINAL CORROSION. Extension tracks: ETCH II EXPANSION or hand-written.\n");

    /* ================================================================ 1 OXIDE */
    begin("OXIDE", 0);
    proj.bpm = 132; proj.swing = 1; proj.gate = 1; proj.tune = 12; proj.xseed = 0x0A1D;
    g = proj.gen;
    g.seed = 0x0A51; g.key = 4; g.scale = SCALE_PHRYGIAN; g.style = STYLE_ACID;
    g.density = 11; g.sync = 6; g.accent = 11; g.slide = 9; g.octave = 0; g.mutation = 6;
    proj.gen = g;
    /*            eng      A  B TONE C ENV MDC ATT DEC SUS REL ACC GLI DRV OCT FIN  lvl pan snd cr  lr ld dst shp */
    PATCH(TRK_BASS, ENG_SUB, 0, 0, 7, 13, 12, 5, 0, 9, 12, 2, 12, 6, 4, 2, 8, 13, 8, 2, 0, 4, 3, 1, 0);
    PATCH(TRK_LEAD, ENG_FM, 13, 6, 9, 0, 11, 4, 0, 6, 0, 4, 8, 0, 3, 3, 8, 7, 11, 7, 0, 0, 0, 0, 0);
    PATCH(TRK_CHORD, ENG_SUB, 1, 0, 12, 9, 9, 6, 0, 8, 4, 6, 6, 0, 2, 2, 8, 6, 5, 9, 1, 7, 4, 1, 1);
    PATCH(TRK_DRONE, ENG_PD, 3, 0, 6, 0, 3, 12, 10, 15, 15, 11, 0, 0, 2, 0, 8, 7, 8, 4, 0, 1, 7, 1, 0);
    PATCH(TRK_KICK, 0, 0, 0, 14, 0, 0, 0, 0, 9, 0, 0, 0, 0, 3, 1, 8, 14, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 2, 0, 0, 18, 0, 0, 0, 0, 7, 0, 0, 0, 0, 2, 2, 8, 11, 7, 4, 0, 0, 0, 0, 0);
    PATCH(TRK_HATS, 0, 0, 0, 20, 0, 0, 0, 0, 8, 0, 0, 0, 0, 1, 2, 8, 10, 10, 2, 0, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_RES, 3, 0, 22, 12, 4, 8, 0, 11, 0, 8, 4, 0, 1, 3, 8, 7, 3, 11, 2, 2, 5, 3, 3);
    proj.fx.delay_time = 2; proj.fx.delay_fb = 7; proj.fx.delay_tone = 6; proj.fx.master_drive = 2; proj.fx.width = 12;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); regen(1, &g, 0x51A2, GEN_BASS);
    copy_pat(2, 0); corrode(2, &g, 6, 0x0D10, EVO_CORRODE, 9);
    { gen_t b = g; b.density = 5; b.seed = 0x0A52; gen_core(3, &b, "(breakdown: density 5)"); }
    proj.pat[3].drum[LANE_KICK] = 0; edit(3, "kick lane cleared for the breakdown");
    expand(0, &g, proj.xseed, 0, XG_DRONE | XG_TEXTURE);
    expand(1, &g, proj.xseed, 1, XG_DRONE | XG_TEXTURE | XG_LEAD);
    expand(2, &g, proj.xseed, 2, XG_ALL);
    expand(3, &g, proj.xseed, 3, XG_DRONE | XG_CHORD | XG_TEXTURE);
    { static const uint8_t pp[] = { 0, 1, 0, 2, 3, 2, 1 }, rr[] = { 4, 4, 4, 4, 2, 4, 4 }; song(pp, rr, 7); }
    code_of(&g, codes[0]);
    emit(0);

    /* ================================================================ 2 FERRIC */
    begin("FERRIC", 1);
    proj.bpm = 126; proj.swing = 0; proj.gate = 1; proj.xseed = 0xFE21;
    g = proj.gen;
    g.seed = 0xFE26; g.key = 1; g.scale = SCALE_MINOR; g.style = STYLE_INDUSTRIAL;
    g.density = 9; g.sync = 7; g.accent = 9; g.slide = 4; g.octave = 0; g.mutation = 5;
    proj.gen = g;
    PATCH(TRK_BASS, ENG_FM, 1, 7, 10, 0, 12, 4, 0, 8, 9, 2, 10, 4, 2, 2, 8, 13, 8, 1, 0, 0, 0, 0, 0);
    PATCH(TRK_LEAD, ENG_FM, 15, 2, 14, 1, 9, 9, 0, 10, 0, 9, 8, 0, 0, 3, 8, 8, 12, 9, 0, 10, 2, 0, 0);
    PATCH(TRK_CHORD, ENG_WT, 2, 0, 9, 9, 6, 8, 0, 9, 3, 8, 4, 0, 0, 2, 8, 6, 4, 8, 0, 2, 3, 1, 0);
    PATCH(TRK_DRONE, ENG_WT, 2, 0, 10, 6, 2, 13, 9, 15, 15, 11, 0, 0, 0, 0, 8, 6, 8, 5, 0, 1, 8, 1, 0);
    PATCH(TRK_KICK, 0, 0, 0, 12, 0, 0, 0, 0, 8, 0, 0, 0, 0, 2, 2, 8, 14, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 1, 0, 0, 20, 0, 0, 0, 0, 8, 0, 0, 0, 0, 1, 2, 8, 11, 6, 5, 0, 0, 0, 0, 0);
    PATCH(TRK_HATS, 1, 0, 0, 22, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 3, 8, 10, 11, 4, 0, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_RES, 1, 0, 20, 13, 3, 9, 0, 11, 0, 8, 4, 0, 0, 3, 8, 8, 2, 10, 0, 3, 4, 3, 3);
    proj.fx.delay_time = 1; proj.fx.delay_fb = 6; proj.fx.delay_tone = 9; proj.fx.master_drive = 1; proj.fx.width = 14;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); corrode(1, &g, 4, 0xFE00, EVO_CORRODE, 7);
    copy_pat(2, 0); regen(2, &g, 0x2E2E, GEN_DRUMS);
    { gen_t b = g; b.density = 13; b.seed = 0xFE27; gen_core(3, &b, "(peak: density 13)"); }
    expand(0, &g, proj.xseed, 0, XG_LEAD | XG_TEXTURE | XG_DRONE);
    expand(1, &g, proj.xseed, 1, XG_ALL);
    expand(2, &g, proj.xseed, 2, XG_LEAD | XG_TEXTURE);
    expand(3, &g, proj.xseed, 3, XG_ALL);
    { static const uint8_t pp[] = { 0, 2, 1, 3, 1, 2 }, rr[] = { 4, 4, 4, 4, 4, 4 }; song(pp, rr, 6); }
    code_of(&g, codes[1]);
    emit(1);

    /* ================================================================ 3 VESSEL */
    begin("VESSEL", 2);
    proj.bpm = 118; proj.swing = 2; proj.gate = 1; proj.xseed = 0x5E55;
    g = proj.gen;
    g.seed = 0x7E55; g.key = 5; g.scale = SCALE_DORIAN; g.style = STYLE_MINIMAL;
    g.density = 7; g.sync = 5; g.accent = 6; g.slide = 3; g.octave = 0; g.mutation = 4;
    proj.gen = g;
    PATCH(TRK_BASS, ENG_SUB, 3, 0, 5, 4, 4, 8, 0, 11, 11, 4, 4, 3, 0, 2, 8, 13, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_LEAD, ENG_RES, 1, 0, 16, 10, 4, 8, 0, 10, 0, 8, 4, 0, 0, 3, 8, 6, 12, 11, 0, 0, 0, 0, 0);
    PATCH(TRK_CHORD, ENG_WT, 4, 2, 12, 6, 7, 7, 0, 9, 2, 8, 4, 0, 0, 2, 8, 9, 6, 13, 0, 3, 5, 1, 0);
    PATCH(TRK_DRONE, ENG_WT, 0, 0, 6, 4, 2, 14, 12, 15, 15, 11, 0, 0, 0, 1, 8, 6, 9, 7, 0, 0, 9, 1, 0);
    PATCH(TRK_KICK, 0, 0, 0, 10, 0, 0, 0, 0, 10, 0, 0, 0, 0, 1, 1, 8, 14, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 0, 0, 0, 14, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 2, 8, 9, 6, 8, 0, 0, 0, 0, 0);
    PATCH(TRK_HATS, 0, 0, 0, 18, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 2, 8, 8, 10, 5, 0, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_RES, 2, 0, 12, 14, 2, 12, 6, 11, 0, 8, 0, 0, 0, 2, 8, 6, 4, 12, 0, 1, 6, 1, 0);
    proj.fx.delay_time = 2; proj.fx.delay_fb = 11; proj.fx.delay_tone = 5; proj.fx.master_drive = 0; proj.fx.width = 15;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); corrode(1, &g, 8, 0x7E00, EVO_DRIFT, 8);
    copy_pat(2, 0); regen(2, &g, 0x7E5E, GEN_BASS);
    {
        /* dub chords: the progression is written by hand (i - VII - iv - i in F dorian) */
        static const uint8_t n0[16] = { 65,65,65,65, 63,63,63,63, 58,58,58,58, 65,65,65,65 };
        static const uint8_t n1[16] = { 65,65,65,65, 68,68,68,68, 63,63,63,63, 60,60,60,60 };
        int k;
        xline(0, XT_CHORD, "..a...n...a...n.", n0, "dub stabs, i-VII-iv-i");
        xline(1, XT_CHORD, "..a...n...a..?n.", n1, "dub stabs, i-iii-VII-v");
        xline(2, XT_CHORD, "..a...n...a...n.", n0, "dub stabs");
        for (k = 0; k < 16; k++) { proj.xpat[0].shape[k] = 5; proj.xpat[1].shape[k] = 2; proj.xpat[2].shape[k] = 5; }
    }
    expand(0, &g, proj.xseed, 0, XG_DRONE | XG_TEXTURE);
    expand(1, &g, proj.xseed, 1, XG_DRONE | XG_TEXTURE | XG_LEAD);
    expand(2, &g, proj.xseed, 2, XG_DRONE | XG_TEXTURE);
    { static const uint8_t pp[] = { 0, 1, 0, 2 }, rr[] = { 8, 8, 8, 8 }; song(pp, rr, 4); }
    code_of(&g, codes[2]);
    emit(2);

    /* ================================================================ 4 DEBRIS */
    begin("DEBRIS", 3);
    proj.bpm = 138; proj.swing = 4; proj.gate = 0; proj.xseed = 0xDEB2;
    g = proj.gen;
    g.seed = 0xDEB5; g.key = 7; g.scale = SCALE_MINPENT; g.style = STYLE_BROKEN;
    g.density = 10; g.sync = 11; g.accent = 9; g.slide = 6; g.octave = 1; g.mutation = 7;
    proj.gen = g;
    PATCH(TRK_BASS, ENG_PD, 0, 4, 10, 0, 11, 5, 0, 7, 6, 2, 10, 3, 3, 1, 8, 13, 8, 1, 0, 0, 0, 0, 0);
    PATCH(TRK_LEAD, ENG_RES, 0, 0, 24, 11, 6, 6, 0, 9, 0, 7, 6, 0, 1, 3, 8, 8, 11, 9, 0, 0, 0, 0, 0);
    PATCH(TRK_CHORD, ENG_SUB, 1, 6, 12, 8, 9, 5, 0, 6, 0, 4, 6, 0, 2, 2, 8, 6, 5, 8, 2, 0, 0, 0, 0);
    PATCH(TRK_DRONE, ENG_WT, 5, 0, 8, 7, 4, 12, 8, 15, 14, 10, 0, 0, 1, 0, 8, 5, 8, 5, 1, 3, 10, 1, 3);
    PATCH(TRK_KICK, 2, 0, 0, 14, 0, 0, 0, 0, 7, 0, 0, 0, 0, 3, 2, 8, 14, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 2, 0, 0, 18, 0, 0, 0, 0, 7, 0, 0, 0, 0, 2, 2, 8, 11, 9, 5, 2, 0, 0, 0, 0);
    PATCH(TRK_HATS, 0, 0, 0, 22, 0, 0, 0, 0, 6, 0, 0, 0, 0, 1, 2, 8, 10, 6, 3, 1, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_RES, 3, 0, 26, 10, 4, 6, 0, 9, 0, 6, 4, 0, 2, 4, 8, 7, 13, 10, 3, 4, 6, 3, 3);
    proj.fx.delay_time = 6; proj.fx.delay_fb = 8; proj.fx.delay_tone = 8; proj.fx.master_drive = 2; proj.fx.width = 13;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); regen(1, &g, 0xDE01, GEN_ALL);
    copy_pat(2, 0); corrode(2, &g, 5, 0xDE10, EVO_CORRODE, 11);
    { gen_t b = g; b.style = STYLE_ELECTRO; b.seed = 0xDEB6; gen_core(3, &b, "(electro interlude)"); }
    expand(0, &g, proj.xseed, 0, XG_LEAD | XG_TEXTURE);
    expand(1, &g, proj.xseed, 1, XG_LEAD | XG_TEXTURE | XG_CHORD);
    expand(2, &g, proj.xseed, 2, XG_ALL);
    expand(3, &g, proj.xseed, 3, XG_DRONE | XG_TEXTURE);
    proj.xpat[0].len[XT_TEXTURE] = 7; proj.xpat[1].len[XT_LEAD] = 12;
    fprintf(rc, "  hand edit: TEXTURE length 7 in P01, LEAD length 12 in P02 (polymeter)\n");
    { static const uint8_t pp[] = { 0, 1, 2, 3, 0, 2 }, rr[] = { 4, 4, 4, 2, 4, 4 }; song(pp, rr, 6); }
    code_of(&g, codes[3]);
    drop_track(XT_DRONE, "the chord layer carries the harmony; keeps the piece inside the CPU budget");
    emit(3);

    /* ================================================================ 5 STATIC */
    begin("STATIC", 4);
    proj.bpm = 108; proj.swing = 0; proj.gate = 2; proj.xseed = 0x57A7;
    g = proj.gen;
    g.seed = 0x5A71; g.key = 2; g.scale = SCALE_MINOR; g.style = STYLE_ELECTRO;
    g.density = 10; g.sync = 6; g.accent = 7; g.slide = 5; g.octave = 1; g.mutation = 5;
    proj.gen = g;
    PATCH(TRK_BASS, ENG_SUB, 2, 7, 11, 6, 7, 7, 0, 10, 10, 3, 6, 2, 1, 1, 8, 13, 8, 1, 0, 0, 0, 0, 0);
    PATCH(TRK_LEAD, ENG_WT, 5, 0, 14, 10, 8, 9, 1, 10, 10, 8, 6, 6, 0, 3, 8, 9, 10, 9, 0, 9, 3, 0, 0);
    PATCH(TRK_CHORD, ENG_WT, 3, 0, 10, 8, 5, 10, 6, 12, 12, 11, 0, 0, 0, 2, 8, 7, 5, 8, 0, 2, 5, 1, 0);
    PATCH(TRK_DRONE, ENG_SUB, 0, 0, 6, 7, 2, 12, 12, 15, 15, 11, 0, 0, 0, 0, 8, 6, 8, 6, 0, 1, 7, 1, 0);
    PATCH(TRK_KICK, 0, 0, 0, 12, 0, 0, 0, 0, 9, 0, 0, 0, 0, 1, 2, 8, 14, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 0, 0, 0, 16, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 2, 8, 12, 8, 9, 0, 0, 0, 0, 0);
    PATCH(TRK_HATS, 2, 0, 0, 18, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 2, 8, 9, 10, 3, 0, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_PD, 4, 0, 18, 0, 6, 9, 0, 11, 0, 8, 4, 0, 0, 3, 8, 6, 3, 12, 0, 5, 6, 3, 3);
    proj.fx.delay_time = 4; proj.fx.delay_fb = 8; proj.fx.delay_tone = 7; proj.fx.master_drive = 1; proj.fx.width = 12;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); regen(1, &g, 0x5A10, GEN_BASS);
    copy_pat(2, 0); corrode(2, &g, 6, 0x5A20, EVO_CORRODE, 8);
    {
        /* the digital melody: written by hand in D minor */
        static const uint8_t m0[16] = { 74,74,74,77, 77,76,76,74, 74,72,72,69, 69,69,72,74 };
        static const uint8_t m1[16] = { 81,81,79,79, 77,77,76,76, 74,74,76,77, 76,76,72,72 };
        static const uint8_t c0[16] = { 62,62,62,62, 58,58,58,58, 65,65,65,65, 60,60,60,60 };
        int k;
        xline(0, XT_LEAD, "n--s-n-.n--s.n--", m0, "melody A (D minor)");
        xline(1, XT_LEAD, "n-s-n-s-n-n-s.n-", m1, "melody B");
        xline(2, XT_LEAD, "n--s-n-.n--s.n--", m0, "melody A");
        for (k = 0; k < 3; k++) xline(k, XT_CHORD, "n---n---n---n---", c0, "i-VI-III-VII pads");
        for (k = 0; k < 16; k++) proj.xpat[0].shape[k] = proj.xpat[1].shape[k] = proj.xpat[2].shape[k] = 0;
    }
    expand(0, &g, proj.xseed, 0, XG_DRONE | XG_TEXTURE);
    expand(1, &g, proj.xseed, 1, XG_DRONE | XG_TEXTURE);
    expand(2, &g, proj.xseed, 2, XG_DRONE | XG_TEXTURE);
    { gen_t b = g; b.density = 6; b.seed = 0x5A72; gen_core(3, &b, "(interlude: density 6)"); }
    expand(3, &g, proj.xseed, 3, XG_DRONE | XG_CHORD | XG_TEXTURE);
    { static const uint8_t pp[] = { 0, 1, 0, 2, 3, 1 }, rr[] = { 4, 4, 4, 4, 4, 4 }; song(pp, rr, 6); }
    code_of(&g, codes[4]);
    drop_track(XT_DRONE, "the chord layer carries the harmony; keeps the piece inside the CPU budget");
    emit(4);

    /* ================================================================ 6 PATINA */
    begin("PATINA", 5);
    proj.bpm = 86; proj.swing = 0; proj.gate = 3; proj.xseed = 0xBA71;
    g = proj.gen;
    g.seed = 0x9A71; g.key = 10; g.scale = SCALE_DORIAN; g.style = STYLE_SPARSE;
    g.density = 6; g.sync = 3; g.accent = 3; g.slide = 6; g.octave = 1; g.mutation = 4;
    proj.gen = g;
    PATCH(TRK_BASS, ENG_WT, 7, 2, 6, 3, 3, 10, 3, 13, 12, 9, 2, 6, 0, 1, 8, 11, 8, 4, 0, 1, 2, 1, 0);
    PATCH(TRK_LEAD, ENG_RES, 1, 0, 14, 12, 3, 10, 0, 11, 0, 8, 2, 0, 0, 3, 8, 6, 12, 13, 0, 0, 0, 0, 0);
    PATCH(TRK_CHORD, ENG_WT, 1, 5, 9, 4, 3, 13, 13, 15, 15, 11, 0, 0, 0, 2, 8, 8, 6, 11, 0, 1, 9, 1, 0);
    PATCH(TRK_DRONE, ENG_PD, 4, 0, 6, 0, 2, 14, 14, 15, 15, 11, 0, 0, 0, 0, 8, 6, 9, 8, 0, 0, 10, 1, 0);
    PATCH(TRK_KICK, 0, 0, 0, 6, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 1, 8, 9, 8, 2, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 0, 0, 0, 10, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 2, 8, 6, 5, 12, 0, 0, 0, 0, 0);
    PATCH(TRK_HATS, 0, 0, 0, 14, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 3, 8, 6, 11, 9, 0, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_RES, 2, 0, 10, 15, 2, 13, 9, 11, 0, 8, 0, 0, 0, 3, 8, 7, 3, 13, 0, 0, 7, 1, 0);
    proj.fx.delay_time = 4; proj.fx.delay_fb = 12; proj.fx.delay_tone = 4; proj.fx.master_drive = 0; proj.fx.width = 15;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); corrode(1, &g, 6, 0x9A00, EVO_DRIFT, 6);
    copy_pat(2, 0); regen(2, &g, 0x9A0F, GEN_BASS);
    expand(0, &g, proj.xseed, 0, XG_ALL);
    expand(1, &g, proj.xseed, 1, XG_DRONE | XG_CHORD | XG_TEXTURE);
    expand(2, &g, proj.xseed, 2, XG_ALL);
    { int k; for (k = 0; k < 3; k++) proj.xpat[k].len[XT_TEXTURE] = (uint8_t)(k == 1 ? 12 : 7); }
    fprintf(rc, "  hand edit: TEXTURE lengths 7/12/7 (slow polymeter)\n");
    { static const uint8_t pp[] = { 0, 1, 2, 1 }, rr[] = { 8, 8, 8, 8 }; song(pp, rr, 4); }
    code_of(&g, codes[5]);
    emit(5);

    /* ================================================================ 7 FRACTURE */
    begin("FRACTURE", 6);
    proj.bpm = 142; proj.swing = 2; proj.gate = 0; proj.xseed = 0xF7AC;
    g = proj.gen;
    g.seed = 0xF7AC; g.key = 11; g.scale = SCALE_PHRYGIAN; g.style = STYLE_BROKEN;
    g.density = 12; g.sync = 13; g.accent = 10; g.slide = 5; g.octave = 0; g.mutation = 8;
    proj.gen = g;
    PATCH(TRK_BASS, ENG_FM, 0, 9, 12, 0, 13, 3, 0, 6, 5, 2, 11, 2, 3, 2, 8, 13, 8, 1, 0, 0, 0, 0, 0);
    PATCH(TRK_LEAD, ENG_SUB, 1, 0, 14, 9, 10, 4, 0, 6, 0, 4, 8, 2, 2, 3, 8, 8, 12, 7, 0, 0, 0, 0, 0);
    PATCH(TRK_CHORD, ENG_WT, 5, 0, 14, 10, 8, 4, 0, 5, 0, 3, 6, 0, 2, 2, 8, 6, 4, 6, 1, 0, 0, 0, 0);
    PATCH(TRK_DRONE, ENG_FM, 15, 3, 6, 3, 2, 12, 10, 15, 14, 11, 0, 0, 1, 0, 8, 5, 8, 4, 0, 2, 6, 1, 2);
    PATCH(TRK_KICK, 1, 0, 0, 12, 0, 0, 0, 0, 7, 0, 0, 0, 0, 3, 2, 8, 14, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 1, 0, 0, 20, 0, 0, 0, 0, 7, 0, 0, 0, 0, 2, 2, 8, 11, 10, 4, 0, 0, 0, 0, 0);
    PATCH(TRK_HATS, 1, 0, 0, 22, 0, 0, 0, 0, 6, 0, 0, 0, 0, 1, 3, 8, 10, 5, 2, 0, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_RES, 1, 0, 24, 11, 4, 6, 0, 8, 0, 6, 6, 0, 1, 4, 8, 8, 13, 8, 0, 0, 0, 0, 0);
    proj.fx.delay_time = 7; proj.fx.delay_fb = 7; proj.fx.delay_tone = 9; proj.fx.master_drive = 2; proj.fx.width = 14;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); corrode(1, &g, 4, 0xF700, EVO_CORRODE, 12);
    copy_pat(2, 0); regen(2, &g, 0xF7F7, GEN_DRUMS);
    copy_pat(3, 1); regen(3, &g, 0xF701, GEN_BASS);
    expand(0, &g, proj.xseed, 0, XG_LEAD | XG_TEXTURE | XG_CHORD);
    expand(1, &g, proj.xseed, 1, XG_ALL);
    expand(2, &g, proj.xseed, 2, XG_LEAD | XG_TEXTURE);
    expand(3, &g, proj.xseed, 3, XG_ALL);
    {
        int k;
        static const uint8_t L[4][4] = { { 5, 16, 7, 5 }, { 12, 16, 16, 7 }, { 7, 16, 16, 5 }, { 5, 12, 16, 7 } };
        for (k = 0; k < 4; k++) memcpy(proj.xpat[k].len, L[k], 4);
        fprintf(rc, "  hand edit: extension track lengths 5/7/12 against the 16-step core (polymeter)\n");
    }
    { static const uint8_t pp[] = { 0, 1, 2, 3, 1, 0 }, rr[] = { 4, 4, 4, 4, 2, 4 }; song(pp, rr, 6); }
    code_of(&g, codes[6]);
    emit(6);

    /* ================================================================ 8 REMNANT */
    begin("REMNANT", 7);
    proj.bpm = 96; proj.swing = 1; proj.gate = 2; proj.xseed = 0x2E77;
    g = proj.gen;
    g.seed = 0x2E77; g.key = 0; g.scale = SCALE_MINOR; g.style = STYLE_MINIMAL;
    g.density = 5; g.sync = 4; g.accent = 4; g.slide = 4; g.octave = 1; g.mutation = 3;
    proj.gen = g;
    PATCH(TRK_BASS, ENG_SUB, 3, 0, 6, 3, 3, 9, 1, 12, 10, 6, 3, 4, 0, 1, 8, 12, 8, 2, 0, 0, 0, 0, 0);
    PATCH(TRK_LEAD, ENG_RES, 1, 0, 12, 13, 3, 10, 0, 11, 0, 8, 2, 0, 0, 3, 8, 7, 11, 12, 0, 0, 0, 0, 0);
    PATCH(TRK_CHORD, ENG_WT, 4, 3, 7, 3, 2, 12, 10, 15, 14, 11, 0, 0, 0, 2, 8, 8, 6, 10, 0, 1, 4, 1, 0);
    PATCH(TRK_DRONE, ENG_WT, 7, 0, 5, 3, 1, 14, 14, 15, 15, 11, 0, 0, 0, 0, 8, 6, 9, 7, 0, 0, 8, 1, 0);
    PATCH(TRK_KICK, 0, 0, 0, 8, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 1, 8, 11, 8, 0, 0, 0, 0, 0, 0);
    PATCH(TRK_PERC, 0, 0, 0, 12, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 2, 8, 7, 6, 10, 0, 0, 0, 0, 0);
    PATCH(TRK_HATS, 0, 0, 0, 14, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 2, 8, 6, 10, 6, 0, 0, 0, 0, 0);
    PATCH(TRK_TEXTURE, ENG_PD, 3, 0, 10, 0, 3, 12, 8, 11, 0, 8, 0, 0, 0, 3, 8, 5, 4, 13, 0, 0, 5, 1, 0);
    proj.fx.delay_time = 3; proj.fx.delay_fb = 10; proj.fx.delay_tone = 5; proj.fx.master_drive = 0; proj.fx.width = 13;
    gen_core(0, &g, "(the core groove)");
    copy_pat(1, 0); corrode(1, &g, 4, 0x2E00, EVO_DRIFT, 4);
    {
        /* a reflective theme, written by hand in C minor */
        static const uint8_t m0[16] = { 72,72,72,72, 70,70,70,70, 67,67,67,67, 68,68,67,67 };
        static const uint8_t m1[16] = { 75,75,75,75, 74,74,72,72, 70,70,70,70, 67,67,67,67 };
        static const uint8_t c0[16] = { 60,60,60,60, 60,60,60,60, 56,56,56,56, 55,55,55,55 };
        static const uint8_t c1[16] = { 58,58,58,58, 58,58,58,58, 53,53,53,53, 55,55,55,55 };
        xline(0, XT_LEAD, "n---n---n---n-n-", m0, "theme A");
        xline(1, XT_LEAD, "n---n-n-n---n---", m1, "theme B");
        xline(0, XT_CHORD, "n-------n---n---", c0, "i - VI - V");
        xline(1, XT_CHORD, "n-------n---n---", c1, "VII - iv - V");
    }
    expand(0, &g, proj.xseed, 0, XG_DRONE | XG_TEXTURE);
    expand(1, &g, proj.xseed, 1, XG_DRONE | XG_TEXTURE);
    { gen_t b = g; b.density = 3; b.seed = 0x2E78; gen_core(2, &b, "(coda: density 3)"); }
    expand(2, &g, proj.xseed, 2, XG_DRONE | XG_CHORD);
    { static const uint8_t pp[] = { 0, 1, 0, 1, 2 }, rr[] = { 4, 4, 4, 4, 4 }; song(pp, rr, 5); }
    code_of(&g, codes[7]);
    drop_track(XT_DRONE, "the chord layer carries the harmony; keeps the piece inside the CPU budget");
    emit(7);

    /* ---- table --------------------------------------------------------------------- */
    {
        static const char *titles[8] = { "OXIDE", "FERRIC", "VESSEL", "DEBRIS", "STATIC", "PATINA", "FRACTURE", "REMNANT" };
        static const char *l1[8] = { "INDUSTRIAL ACID", "METALLIC FM PERCUSSION", "DUB TECHNO", "BROKEN ELECTRONICS",
                                     "DARK SYNTHWAVE", "GENERATIVE AMBIENCE", "RHYTHMIC INTERPLAY", "REFLECTIVE CODA" };
        static const char *l2[8] = { "DRIVEN SAW, RESONANT FILTER", "2-OP FM BASS, METAL KITS", "WAVETABLE CHORDS IN DELAY",
                                     "PHASE DISTORTION, KARPLUS", "WAVETABLE LEAD, GLIDE", "BOWED RESONATOR, SLOW PADS",
                                     "POLYMETER 5 / 7 / 12", "TRIANGLE BASS, MALLETS" };
        int i;
        fprintf(out, "const album_track_t album[ALBUM_TRACKS] = {\n");
        for (i = 0; i < 8; i++)
            fprintf(out, "  { \"%s\", \"%s\", \"%s\", \"%s\", a%d_hdr, %d, a%d_pat, a%d_xpat },\n",
                    titles[i], l1[i], l2[i], codes[i], i, npats[i], i, i);
        fprintf(out, "};\n");
        fprintf(rc, "\nLegacy seed codes (core groove = pattern 01 on ETCH I):\n");
        for (i = 0; i < 8; i++) fprintf(rc, "  %d. %-9s %s\n", i + 1, titles[i], codes[i]);
    }
    fprintf(out,
        "void album_load(uint8_t i, project2_t *p)\n{\n"
        "    const album_track_t *a = &album[i %% ALBUM_TRACKS];\n"
        "    uint8_t k;\n"
        "    project_defaults(p);\n"
        "    memcpy(p, a->hdr, offsetof(project2_t, pat));\n"
        "    for (k = 0; k < a->npat; k++) {\n"
        "        memcpy(&p->pat[k], a->pat + 64 * k, 64);\n"
        "        memcpy(&p->xpat[k], a->xpat + sizeof(xpattern_t) * k, sizeof(xpattern_t));\n"
        "    }\n}\n");
    fclose(out);
    fclose(rc);
    (void)logf_;
    return 0;
}
