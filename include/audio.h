/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Audio engine: Direct Sound mixer, voices, percussion, effects.
 *
 * Everything that produces samples runs in the VBlank interrupt, compiled
 * as ARM code in IWRAM.  The render path uses only integer arithmetic and
 * lookup tables; there is no allocation and no floating point.
 */
#ifndef AUDIO_H
#define AUDIO_H
#include <stdint.h>
#include "gba.h"
#include "tables.h"
#include "etch2.h"

/* ---- voices ------------------------------------------------------------- */
#define V_BASS     0
#define V_LEAD     1
#define V_CHORD0   2          /* paraphonic: one voice, three oscillators */
#define V_DRONE    3
#define V_TEXTURE  4
#define N_MELODIC  5
#define N_DRUMS    4          /* one per ORIGINAL drum lane */
#define DL_LEN     1024       /* resonator delay line (samples) */

typedef struct {
    /* identity */
    uint8_t engine, track, active, stage;   /* stage: 0 off 1 attack 2 decay/sustain 3 release */
    uint8_t accented, held, retrig, shape;
    /* pitch (MIDI * 64) */
    int32_t pitch, target;
    int32_t lfo_pitch;
    /* envelopes: Q24 (1 << 24 = full) */
    int32_t amp, sus;
    int32_t menv, aenv, acc_charge;
    uint32_t att_inc;
    uint16_t dec_k, rel_k, menv_k, aenv_k;
    /* oscillators / generators */
    uint32_t ph[4];
    uint32_t inc[4];
    uint32_t brk;                            /* PD break point / pulse width */
    int32_t  s1, s2;                         /* PD slopes (Q16) */
    /* filter and state */
    int32_t lp, bp, f, q, dc;
    int32_t fb1, fb2;                        /* FM feedback history */
    int32_t idx;                             /* FM index / WT position / RES damping */
    int32_t drive;
    /* output stage */
    int32_t g0, g1;                          /* block gain start/end, Q15 */
    int32_t gl, gr, gs;                      /* pan + send, Q12 */
    uint8_t crush, crush_ctr; int16_t crush_hold; uint8_t ctl, stolen;
    /* LFO / modulation randomness (independent stream per voice) */
    uint32_t lfo_ph, rng;
    int32_t lfo_val;
    /* resonator */
    /* paraphonic chord: two more oscillators (offsets from the root) */
    uint8_t nosc, pad1[3];
    int32_t coff[2];
    uint32_t xph[2][2], xinc[2][2];
    int16_t *dl;
    uint32_t dpos;
    uint32_t dlen;                           /* Q16 delay length */
    int32_t exc_left, damp, loopg, exc_lp;
} voice_t;

/* procedural drum synthesis state (one hit) */
typedef struct {
    uint32_t tph, tinc, tinc_end;            /* tone osc (sine) with pitch envelope */
    int32_t tpk, tamp, tdec, tlvl;
    int32_t namp, ndec, nlvl, nf, nq, nlp, nbp, nmode;   /* filtered noise */
    int32_t mamp, mdec, mlvl, mhp;           /* metal bank (3 squares, XOR) */
    uint32_t mph[4], minc[4];
    int32_t camp, cdec;                      /* click */
    int32_t claps, clap_gap, clap_ctr;       /* clap re-triggers */
    uint32_t rng;
    int32_t drive, fm;
} dsynth_t;

typedef struct {
    uint8_t active, lane, cached, pad;
    const int8_t *buf;                       /* cached render being played */
    uint32_t pos, len;
    int32_t scale;                           /* Q8 gain of the cached render */
    dsynth_t rt;                             /* real-time fallback */
    int32_t g0, g1, gl, gr, gs;
    uint8_t crush, crush_ctr; int16_t crush_hold;
} drumvoice_t;

/* render cache: each ORIGINAL drum lane keeps its current hit synthesised
 * into RAM (double buffered); see src/audio/synthesis/percussion.iwram.c */
void drum_cache_service(void) IWRAM_CODE;
void drums_init(void) IWRAM_CODE;
void drum_cache_prime(void) IWRAM_CODE;    /* project load: render the start pattern's kits */     /* main loop: keep renders fresh */
extern volatile uint16_t drum_cache_renders, drum_rt_hits, voices_stolen;

extern voice_t voices[N_MELODIC];
extern drumvoice_t dvoices[N_DRUMS];

/* control interface (called from the sequencer, inside the audio IRQ) */
void voice_note_on(uint8_t v, uint8_t trk, int32_t pitch, uint8_t accent, uint8_t glide) IWRAM_CODE;
void voice_chord_on(uint8_t v, uint8_t trk, int32_t pitch, int32_t off1, int32_t off2, uint8_t accent, uint8_t glide) IWRAM_CODE;
void voice_gate_off(uint8_t v) IWRAM_CODE;
void voice_kill(uint8_t v) IWRAM_CODE;
void voice_step_tick(void) IWRAM_CODE;      /* once per sequencer step (accent charge) */
void drum_trigger(uint8_t lane, uint8_t kit, uint8_t accent) IWRAM_CODE;
void drums_choke(uint8_t lane) IWRAM_CODE;
void audio_all_off(void) IWRAM_CODE;
void voice_init_all(void) IWRAM_CODE;

/* live sound drift (ETCH's evo_cut_ofs / evo_tune_ofs; never saved) */
extern volatile int8_t evo_cut_ofs;
extern int8_t evo_tune_ofs[NUM_LANES];

/* ---- mixer ---------------------------------------------------------------- */
void mixer_init(void) IWRAM_CODE;
void audio_vblank(void) IWRAM_CODE;          /* called first thing on VBlank */
extern volatile uint32_t mix_frames;
extern volatile uint16_t mix_lines_last, mix_lines_max;   /* CPU use in scanlines (228 = one frame) */
extern volatile uint32_t mix_lines_sum;
extern volatile uint16_t mix_overruns;        /* frames where mixing overran the frame */
extern volatile uint8_t  mix_peak;            /* last frame's peak output (0..127) */
extern volatile uint16_t mix_clips;           /* samples that reached the limiter ceiling */
extern int8_t mix_scope[64];                  /* decimated output for the LIVE scope */
extern volatile uint8_t audio_test_tone;      /* diagnostics: 1 = pure sine instead of music */
extern volatile uint8_t audio_solo_engine;    /* diagnostics */

/* effects */
void fx_reset(void) IWRAM_CODE;
void fx_set_tempo(uint8_t bpm) IWRAM_CODE;

/* the sequencer hook the mixer calls once per 16-sample block */
void seq_block(void) IWRAM_CODE;

#endif
