/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Sequencer (ETCH semantics, sample-accurate clock) and the CORROSION
 * hand-off between the main loop and the audio interrupt.
 */
#ifndef SEQ_H
#define SEQ_H
#include "etch2.h"
#include "gba.h"

extern volatile uint8_t seq_playing;
extern volatile uint8_t seq_step;          /* 0..15 currently sounding         */
extern volatile uint8_t seq_cur;           /* current pattern                  */
extern volatile uint8_t seq_queued;        /* 0xFF = none                      */
extern volatile uint8_t seq_chain_pos;
extern volatile uint8_t seq_chain_bar;
extern volatile uint8_t seq_loop;          /* times the current pattern looped */
extern volatile uint8_t seq_bar_total;     /* free-running bar counter         */
extern volatile uint8_t seq_phrase;        /* phrase counter (evolution clock) */
extern volatile uint8_t seq_fill, seq_fill_hold;
extern volatile uint16_t seq_activity;     /* bit per logical track, this step */
extern volatile uint8_t seq_step_event;    /* incremented every step           */
extern volatile uint32_t seq_steps_total;
extern volatile uint8_t seq_xpos[XT_COUNT];/* extension track positions        */
extern uint16_t seq_fill_mask[NUM_LANES];

/* CORROSION hand-off (same protocol as ETCH, plus an edit epoch) */
#define EVO_IDLE  0
#define EVO_READY 1
extern volatile uint8_t evo_state, evo_target, evo_epoch, evo_request, evo_commits, evo_undo_pat;
extern volatile uint8_t evo_stale_drops;   /* candidates refused because the pattern changed */
extern pattern_t evo_cand, evo_undo;

void seq_init(void) IWRAM_CODE;
void seq_start(void) IWRAM_CODE;
void seq_stop(void) IWRAM_CODE;
void seq_seed_live(uint32_t s) IWRAM_CODE;
void seq_set_tempo(void) IWRAM_CODE;
void seq_queue_pattern(uint8_t p) IWRAM_CODE;
void seq_jump_chain(uint8_t pos) IWRAM_CODE;
void seq_trigger_fill(const uint16_t *masks) IWRAM_CODE;
void seq_fill_latch(uint8_t on, const uint16_t *masks) IWRAM_CODE;
void seq_fill_masks(const uint16_t *masks) IWRAM_CODE;
void seq_pattern_edited(uint8_t pat) IWRAM_CODE;      /* bumps the epoch, drops candidates */
uint8_t seq_undo(void) IWRAM_CODE;
void seq_backup_for_undo(uint8_t pat) IWRAM_CODE;
uint8_t seq_next_bar_pattern(void) IWRAM_CODE;
uint8_t seq_bars_to_evolution(void) IWRAM_CODE;
int32_t seq_track_pitch(uint8_t trk, uint8_t note) IWRAM_CODE;  /* note -> pitch (MIDI*64) */

#endif
