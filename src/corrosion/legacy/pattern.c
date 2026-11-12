/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Portable pattern utilities (host-testable).
 */
#ifdef __SDCC
#pragma bank 255
#endif
#include "etch.h"

void pattern_clear(pattern_t *p) ETCH_BANKED
{
    uint8_t i;
    memset(p, 0, sizeof(pattern_t));
    for (i = 0; i < NUM_STEPS; i++) {
        p->note[i] = 12;              /* C2 */
        p->prob[i] = PROB_ALWAYS;
    }
}

void project_drum_defaults(project_t *p) ETCH_BANKED
{
    uint8_t l;
    for (l = 0; l < NUM_LANES; l++) {
        p->drum_tune[l] = 8;
        p->drum_decay[l] = 8;
        p->drum_level[l] = 15;
    }
}

uint8_t pattern_note_count(const pattern_t *p) ETCH_BANKED
{
    uint8_t i, n = 0;
    for (i = 0; i < NUM_STEPS; i++)
        if ((p->flags[i] & STEP_TYPE_MASK) == STEP_NOTE) n++;
    return n;
}

/* Bring a pattern into a valid state.  Locked steps are never modified.
 *   - notes clamped to the playable range, unknown step types -> rest
 *   - a slide needs a following note (or tie) to glide into
 *   - a tie needs something sounding before it
 *   - probability 0..7, condition 0..8, kit 0..3, style < STYLE_COUNT */
void pattern_sanitize(pattern_t *p) ETCH_BANKED
{
    uint8_t i, t, prev, next;
    for (i = 0; i < NUM_STEPS; i++) {
        if (p->flags[i] & STEP_LOCK) {
            if (p->note[i] > NOTE_MAX) p->note[i] = NOTE_MAX;
            continue;
        }
        if (p->note[i] > NOTE_MAX) p->note[i] = NOTE_MAX;
        p->flags[i] &= (uint8_t)(STEP_TYPE_MASK | STEP_ACCENT | STEP_SLIDE | STEP_LOCK);
        t = p->flags[i] & STEP_TYPE_MASK;
        if (t == 3) { p->flags[i] &= (uint8_t)~STEP_TYPE_MASK; t = STEP_REST; }
        if ((p->prob[i] & 0x0F) > 7) p->prob[i] = (uint8_t)((p->prob[i] & 0xF0) | 7);
        if ((p->prob[i] >> 4) >= COND_COUNT) p->prob[i] &= 0x0F;
    }
    for (i = 0; i < NUM_STEPS; i++) {
        if (p->flags[i] & STEP_LOCK) continue;
        t = p->flags[i] & STEP_TYPE_MASK;
        prev = p->flags[(i + 15) & 15] & STEP_TYPE_MASK;
        next = p->flags[(i + 1) & 15] & STEP_TYPE_MASK;
        if (t == STEP_TIE && prev == STEP_REST) {
            p->flags[i] = (uint8_t)(p->flags[i] & ~(STEP_TYPE_MASK | STEP_SLIDE | STEP_ACCENT));
            t = STEP_REST;
        }
        if (t == STEP_REST) p->flags[i] &= (uint8_t)~(STEP_SLIDE | STEP_ACCENT);
        if ((p->flags[i] & STEP_SLIDE) && next != STEP_NOTE) p->flags[i] &= (uint8_t)~STEP_SLIDE;
    }
    for (i = 0; i < NUM_LANES; i++) p->kit[i] &= 3;
    if (p->style >= STYLE_COUNT) p->style = 0;
    p->lock &= (uint8_t)(PLOCK_BASS | (0x0F << 1) | PLOCK_ALL);
}
