/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Instruments: engine parameter definitions, default patches and the
 * default project.  Pure data and helpers (also compiled into host tools).
 */
#include <string.h>
#include "etch2.h"
#include "instruments.h"

const char * const trk_names[TRK_COUNT] = { "BASS", "LEAD", "CHORD", "DRONE", "KICK", "PERC", "HATS", "TEXTR" };
const uint8_t xt_of_trk[TRK_COUNT] = { 0xFF, XT_LEAD, XT_CHORD, XT_DRONE, 0xFF, 0xFF, 0xFF, XT_TEXTURE };
const char * const eng_names[ENG_COUNT] = { "SUBTRACT", "FM 2-OP ", "WAVETABL", "PHASE DS", "RESONATR" };
const char * const voicing_names[3] = { "ANALOG  ", "FM METAL", "DIGITAL " };
const char * const lfo_dest_names[LFO_TO_COUNT] = { "PITCH", "TONE ", "AMP  ", "PAN  " };
const char * const lfo_shape_names[LFOS_COUNT] = { "TRI", "SQR", "SAW", "S+H" };
const char * const chord_names[CHORD_SHAPES] = { "TRIAD", "SUS4 ", "SEVEN", "OPEN ", "SUS2 ", "SIXTH", "QUART", "CLUST" };

/* parameter layout per engine: label (6 chars) and maximum (0 = unused) */
const paramdef_t param_defs[ENG_DEFS][P_COUNT] = {
    /* SUBTRACTIVE */
    { {"WAVE  ", 3}, {"DETUNE", 15}, {"CUTOFF", 31}, {"RESO  ", 15}, {"ENVMOD", 15}, {"FDECAY", 15},
      {"ATTACK", 15}, {"DECAY ", 15}, {"SUSTN ", 15}, {"RELEAS", 15}, {"ACCENT", 15}, {"GLIDE ", 15},
      {"DRIVE ", 7}, {"OCTAVE", 4}, {"FINE  ", 15} },
    /* FM */
    { {"RATIO ", 15}, {"FEEDBK", 15}, {"INDEX ", 31}, {"CARRIR", 3}, {"ENVMOD", 15}, {"IDECAY", 15},
      {"ATTACK", 15}, {"DECAY ", 15}, {"SUSTN ", 15}, {"RELEAS", 15}, {"ACCENT", 15}, {"GLIDE ", 15},
      {"DRIVE ", 7}, {"OCTAVE", 4}, {"FINE  ", 15} },
    /* WAVETABLE */
    { {"TABLE ", 7}, {"SPREAD", 15}, {"POSITN", 31}, {"BRIGHT", 15}, {"ENVMOD", 15}, {"PDECAY", 15},
      {"ATTACK", 15}, {"DECAY ", 15}, {"SUSTN ", 15}, {"RELEAS", 15}, {"ACCENT", 15}, {"GLIDE ", 15},
      {"DRIVE ", 7}, {"OCTAVE", 4}, {"FINE  ", 15} },
    /* PHASE DISTORTION */
    { {"SHAPE ", 5}, {"SKEW  ", 15}, {"AMOUNT", 31}, {"------", 0}, {"ENVMOD", 15}, {"DDECAY", 15},
      {"ATTACK", 15}, {"DECAY ", 15}, {"SUSTN ", 15}, {"RELEAS", 15}, {"ACCENT", 15}, {"GLIDE ", 15},
      {"DRIVE ", 7}, {"OCTAVE", 4}, {"FINE  ", 15} },
    /* RESONATOR */
    { {"EXCITE", 3}, {"------", 0}, {"BRIGHT", 31}, {"SUSTN ", 15}, {"ENVMOD", 15}, {"BDECAY", 15},
      {"ATTACK", 15}, {"DECAY ", 15}, {"SUSTN ", 15}, {"RELEAS", 15}, {"ACCENT", 15}, {"GLIDE ", 15},
      {"DRIVE ", 7}, {"OCTAVE", 4}, {"FINE  ", 15} },
    /* DRUM tracks (engine field = VOICING) */
    { {"------", 0}, {"------", 0}, {"TONE  ", 31}, {"------", 0}, {"------", 0}, {"------", 0},
      {"------", 0}, {"DECAY ", 15}, {"------", 0}, {"------", 0}, {"------", 0}, {"------", 0},
      {"DRIVE ", 7}, {"TUNE  ", 4}, {"------", 0} },
};

static const char * const wave_n[4] = { "SAW ", "SQR ", "PULS", "TRI " };
static const char * const ratio_n[16] = { "0.5 ", "1   ", "1.5 ", "2   ", "2.5 ", "3   ", "3.5 ", "4   ",
                                          "5   ", "6   ", "7   ", "8   ", "9   ", "11  ", "13  ", "1.41" };
static const char * const car_n[4] = { "1X  ", "2X  ", "3X  ", "0.5X" };
static const char * const wt_n[8] = { "GLAS", "VOIC", "METL", "PULS", "ORGN", "DIGT", "RUST", "HOLW" };
static const char * const pd_n[6] = { "SAW ", "SQR ", "PULS", "RES1", "RES2", "DBL " };
static const char * const ex_n[4] = { "PLUK", "MALT", "BOWD", "GRAN" };
static const char * const oct_n[5] = { "-2  ", "-1  ", " 0  ", "+1  ", "+2  " };

/* 4-character value text for UI and documentation */
void param_text(uint8_t def, uint8_t slot, uint8_t v, char *out4)
{
    const char *s = 0;
    if (slot == P_A) {
        switch (def) {
        case ENG_SUB: s = wave_n[v & 3]; break;
        case ENG_FM:  s = ratio_n[v & 15]; break;
        case ENG_WT:  s = wt_n[v & 7]; break;
        case ENG_PD:  s = pd_n[v % 6]; break;
        case ENG_RES: s = ex_n[v & 3]; break;
        }
    } else if (slot == P_C && def == ENG_FM) s = car_n[v & 3];
    else if (slot == P_OCTAVE) s = oct_n[v % 5];
    if (s) { memcpy(out4, s, 4); return; }
    if (slot == P_FINE) {
        int d = (int)v - 8;
        out4[0] = d < 0 ? '-' : d > 0 ? '+' : ' ';
        if (d < 0) d = -d;
        out4[1] = (char)('0' + d); out4[2] = ' '; out4[3] = ' ';
        return;
    }
    out4[0] = (char)(v >= 10 ? '0' + v / 10 : ' ');
    out4[1] = (char)('0' + v % 10);
    out4[2] = ' '; out4[3] = ' ';
}

uint8_t param_def_of(const patch_t *pt, uint8_t trk)
{
    return TRK_IS_DRUM(trk) ? ENG_DEFS - 1 : (uint8_t)(pt->engine % ENG_COUNT);
}

/* default sounds: one per track */
void patch_default(patch_t *p, uint8_t trk)
{
    static const uint8_t def[TRK_COUNT][1 + P_COUNT + 8] = {
        /* eng   A  B  TONE C ENV MDEC ATT DEC SUS REL ACC GLI DRV OCT FINE  lvl pan snd crush lfo r d dst shp */
        { ENG_SUB, 0, 0, 9, 11, 10, 6,  0, 10, 13, 2, 10, 6, 2, 2, 8,       12, 8, 2, 0,   5, 0, 1, 0 },   /* BASS  */
        { ENG_FM,  3, 4, 12, 0, 9, 7,   0, 9, 8, 6, 6, 3, 0, 3, 8,          9, 6, 6, 0,    9, 2, 0, 0 },   /* LEAD  */
        { ENG_WT,  0, 3, 10, 9, 6, 10,  3, 12, 11, 9, 4, 0, 0, 2, 8,        8, 9, 8, 0,    3, 4, 1, 0 },   /* CHORD */
        { ENG_PD,  3, 0, 8, 0, 3, 13,   9, 15, 15, 12, 0, 8, 0, 1, 8,       7, 8, 5, 0,    1, 6, 1, 0 },   /* DRONE */
        { 0,       0, 0, 16, 0, 0, 0,   0, 8, 0, 0, 0, 0, 0, 2, 8,          13, 8, 0, 0,   0, 0, 0, 0 },   /* KICK  */
        { 0,       0, 0, 16, 0, 0, 0,   0, 8, 0, 0, 0, 0, 0, 2, 8,          11, 8, 3, 0,   0, 0, 0, 0 },   /* PERC  */
        { 0,       0, 0, 16, 0, 0, 0,   0, 8, 0, 0, 0, 0, 0, 2, 8,          10, 9, 1, 0,   0, 0, 0, 0 },   /* HATS  */
        { ENG_RES, 0, 0, 18, 9, 4, 8,   0, 11, 4, 9, 4, 0, 0, 3, 8,         8, 11, 9, 0,   2, 3, 3, 3 },   /* TEXTR */
    };
    const uint8_t *d = def[trk % TRK_COUNT];
    uint8_t i;
    memset(p, 0, sizeof *p);
    p->engine = d[0];
    for (i = 0; i < P_COUNT; i++) p->p[i] = d[1 + i];
    p->level = d[16]; p->pan = d[17]; p->send = d[18]; p->crush = d[19];
    p->lfo_rate = d[20]; p->lfo_depth = d[21]; p->lfo_dest = d[22]; p->lfo_shape = d[23];
}

void xpattern_clear(xpattern_t *x)
{
    uint8_t t, s;
    memset(x, 0, sizeof *x);
    for (t = 0; t < XT_COUNT; t++) {
        x->len[t] = 16;
        for (s = 0; s < NUM_STEPS; s++) x->trk[t].note[s] = 60;
    }
}

/* FILE > NEW: ETCH's starting point, rendered by ETCH II's default patches */
void project_defaults(project2_t *p)
{
    static const char nm[E2_NAME_LEN] = { 'U','N','T','I','T','L','E','D',' ',' ' };
    uint8_t i;
    memset(p, 0, sizeof *p);
    memcpy(p->name, nm, E2_NAME_LEN);
    p->compat = COMPAT_CLASSIC;
    p->bpm = 124; p->swing = 0; p->gate = 1; p->tune = 12;
    p->album_id = 0xFF;
    p->gen.key = 9; p->gen.scale = SCALE_MINOR; p->gen.style = STYLE_ACID;
    p->gen.density = 9; p->gen.sync = 5; p->gen.accent = 8; p->gen.slide = 7;
    p->gen.octave = 0; p->gen.mutation = 5; p->gen.mode = EVO_STABLE;
    p->gen.seed = 0x2601;
    for (i = 0; i < E2_SONG_LEN; i++) { p->song.pat[i] = 0xFF; p->song.rep[i] = 1; }
    p->song.pat[0] = 0; p->song.rep[0] = 4;
    p->fx.delay_time = 2; p->fx.delay_fb = 6; p->fx.delay_tone = 7; p->fx.master_drive = 0; p->fx.width = 10;
    for (i = 0; i < TRK_COUNT; i++) patch_default(&p->patch[i], i);
    for (i = 0; i < NUM_LANES; i++) { p->drum_tune[i] = 8; p->drum_decay[i] = 8; p->drum_level[i] = 15; }
    for (i = 0; i < E2_PATTERNS; i++) {
        pattern_clear(&p->pat[i]);
        p->pat[i].kit[1] = 2;                 /* clap, as ETCH's NEW */
        xpattern_clear(&p->xpat[i]);
    }
}

/* ETCH's kit names per ORIGINAL drum lane (src/drums/drums.c in ETCH) */
const char * const drum_kit_names2[NUM_LANES][4] = {
    { "DEEP ", "PUNCH", "THUD ", "CLICK" },
    { "SHORT", "GRIT ", "CLAP ", "RIM  " },
    { "CLOSD", "TICK ", "SHAKE", "SHARP" },
    { "OPEN ", "METAL", "BLIP ", "CRASH" },
};
