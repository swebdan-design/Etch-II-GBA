/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Sequencer.  A port of ETCH's src/sequencer/seq.c to the GBA mixer clock.
 *
 * Timing: the mixer calls seq_block() before every 16-sample block.  A
 * 32-bit phase accumulator spanning a PAIR of 16th notes advances by an
 * increment derived from BPM; the even 16th begins when it wraps, the odd
 * 16th when it passes the swing point -- exactly ETCH's scheme, with 16-bit
 * phases widened to 32 bits.  Onsets are quantised to one block (1.5 ms at
 * 15 768 Hz; ETCH: 3.9 ms) and never accumulate error.  Gate points are
 * fractions of each step's own length, so high tempo + maximum swing keeps
 * every gate inside its step.
 *
 * Step semantics are ETCH's (TB-style):
 *   NOTE  new note; envelopes retrigger unless the previous step slid into it
 *   TIE   previous note sustains        REST  release at the gate point
 *   SLIDE gate held, pitch glides into the next step if it plays a note
 *   PROB  rolled live each loop with the LIVE dice (never CORROSION's PRNG)
 *   COND  1:2 2:2 1:4 .. 4:4 FILL !FILL
 * The four ORIGINAL drum lanes play their masks (or the fill masks).
 * ETCH II extension tracks (LEAD, CHORD, DRONE, TEXTURE) follow the same
 * NOTE/TIE/REST/SLIDE rules with their own lengths and their OWN dice.
 */
#include "audio.h"
#include "seq.h"
void voice_govern(uint16_t lines) IWRAM_CODE;

volatile uint8_t seq_playing, seq_step, seq_cur, seq_queued = 0xFF, seq_chain_pos, seq_chain_bar;
volatile uint8_t seq_loop, seq_bar_total, seq_phrase, seq_fill, seq_fill_hold, seq_step_event;
volatile uint16_t seq_activity;
volatile uint32_t seq_steps_total, seq_gates_total, seq_gate_late;
volatile uint8_t seq_xpos[XT_COUNT];
uint16_t seq_fill_mask[NUM_LANES];
static uint8_t fill_bars;
static uint8_t chain_jump = 0xFF;

volatile uint8_t evo_state, evo_target, evo_epoch, evo_request, evo_commits, evo_undo_pat = 0xFF, evo_stale_drops;
pattern_t evo_cand, evo_undo;
volatile uint8_t pat_epoch[E2_PATTERNS];

static uint32_t pphase, pinc, swing_pt, gate_even, gate_odd;
static uint8_t gate_done, next_plays, cur_plays, legato, pending_start, last_region;
static uint32_t live_rng = 0x5EED;         /* ETCH's live dice (16-bit xorshift) */
static uint32_t xlive_rng = 0xC0FFEE;      /* extension-track dice (separate)   */

/* extension track state */
static uint8_t x_next[XT_COUNT], x_cur[XT_COUNT], x_leg[XT_COUNT];

static const uint8_t gate_frac[4] = { 6, 9, 12, 15 };

static uint8_t live_rand(void)
{
    uint16_t r = (uint16_t)live_rng;
    r ^= (uint16_t)(r << 7);
    r ^= (uint16_t)(r >> 9);
    r ^= (uint16_t)(r << 8);
    live_rng = r;
    return (uint8_t)r;
}

static uint8_t xlive_rand(void)
{
    uint32_t x = xlive_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    xlive_rng = x;
    return (uint8_t)(x >> 24);
}

/* ---- chain (ETCH logic, 32 positions) ---------------------------------- */
static uint8_t chain_next(uint8_t pos)
{
    uint8_t i;
    for (i = 0; i < E2_SONG_LEN; i++) {
        pos = (uint8_t)((pos + 1) & (E2_SONG_LEN - 1));
        if (proj.song.pat[pos] < E2_PATTERNS) return pos;
        if (proj.song.pat[pos] == 0xFF) pos = E2_SONG_LEN - 1;   /* END -> wrap */
    }
    return 0;
}

static uint8_t peek_next_pattern(uint8_t *loop_out)
{
    uint8_t p = seq_cur;
    *loop_out = (uint8_t)(seq_loop + 1);
    if (proj.song.song && chain_jump != 0xFF) {
        if (proj.song.pat[chain_jump] < E2_PATTERNS) p = proj.song.pat[chain_jump];
    } else if (proj.song.song) {
        uint8_t rep = proj.song.rep[seq_chain_pos];
        if ((uint8_t)(seq_chain_bar + 1) >= (rep ? rep : 1)) {
            uint8_t np = proj.song.pat[chain_next(seq_chain_pos)];
            if (np < E2_PATTERNS) p = np;
        }
    } else if (seq_queued != 0xFF) {
        p = seq_queued;
    }
    if (p != seq_cur) *loop_out = 0;
    return p;
}

uint8_t seq_next_bar_pattern(void)
{
    uint8_t lp, p;
    CRITICAL { p = peek_next_pattern(&lp); }
    return p;
}

static uint8_t cond_ok(uint8_t cond, uint8_t loop)
{
    switch (cond) {
    case COND_1OF2: return (loop & 1) == 0;
    case COND_2OF2: return (loop & 1) == 1;
    case COND_1OF4: return (loop & 3) == 0;
    case COND_2OF4: return (loop & 3) == 1;
    case COND_3OF4: return (loop & 3) == 2;
    case COND_4OF4: return (loop & 3) == 3;
    case COND_FILL: return seq_fill;
    case COND_NOFILL: return !seq_fill;
    default: return 1;
    }
}

/* ETCH decide(): does step s of pattern p play a new bass note? */
static uint8_t decide(const pattern_t *p, uint8_t s, uint8_t loop)
{
    uint8_t f = p->flags[s], pr = p->prob[s];
    if ((f & STEP_TYPE_MASK) != STEP_NOTE) return 0;
    if (proj.mutes & (1u << TRK_BASS)) return 0;
    if (!cond_ok((uint8_t)(pr >> 4), loop)) return 0;
    if ((pr & 7) != 7 && (live_rand() & 7) > (pr & 7)) return 0;
    return 1;
}

static uint8_t xtrk(uint8_t x) { static const uint8_t m[XT_COUNT] = { TRK_LEAD, TRK_CHORD, TRK_DRONE, TRK_TEXTURE }; return m[x]; }

static uint8_t xlen(const xpattern_t *xp, uint8_t x)
{
    uint8_t l = xp->len[x];
    return (uint8_t)(l == 0 || l > 16 ? 16 : l);
}

static uint8_t xdecide(const xpattern_t *xp, uint8_t x, uint8_t s)
{
    uint8_t f = xp->trk[x].flags[s], c;
    if ((f & STEP_TYPE_MASK) != STEP_NOTE) return 0;
    if (proj.compat != COMPAT_EXPANDED) return 0;          /* CLASSIC plays the core only */
    if (proj.mutes & (1u << xtrk(x))) return 0;
    c = (uint8_t)(f >> XF_PROB_SHIFT);
    if (c && (xlive_rand() & 7) < c) return 0;
    return 1;
}

/* bass and extension notes -> pitch in MIDI*64; ETCH note 0 = C1 = MIDI 24 */
int32_t seq_track_pitch(uint8_t trk, uint8_t note)
{
    int32_t midi = trk == TRK_BASS ? (int32_t)note + 24 : (int32_t)note;
    midi += (int32_t)proj.tune - 12;
    return midi * 64;
}

/* type of the next bass step (tie look-ahead) */
static uint8_t next_type(void)
{
    uint8_t lp;
    const pattern_t *np;
    if (seq_step == 15) np = &proj.pat[peek_next_pattern(&lp)];
    else np = &proj.pat[seq_cur];
    return np->flags[(seq_step + 1) & 15] & STEP_TYPE_MASK;
}

/* the step that follows extension track x */
static const xpattern_t *x_next_pat(uint8_t x, uint8_t *pos)
{
    const xpattern_t *xp = &proj.xpat[seq_cur];
    uint8_t p = (uint8_t)(seq_xpos[x] + 1);
    if (seq_step == 15) {
        uint8_t lp, np = peek_next_pattern(&lp);
        if (np != seq_cur) { *pos = 0; return &proj.xpat[np]; }
    }
    if (p >= xlen(xp, x)) p = 0;
    *pos = p;
    return xp;
}

static void bar_boundary(void)
{
    uint8_t prev = seq_cur, x;
    seq_bar_total++;
    if (fill_bars && !seq_fill_hold) {
        if (--fill_bars == 0) seq_fill = 0;
    }
    if (proj.song.song && chain_jump != 0xFF) {
        seq_chain_pos = chain_jump;
        seq_chain_bar = 0;
        chain_jump = 0xFF;
        if (proj.song.pat[seq_chain_pos] < E2_PATTERNS) seq_cur = proj.song.pat[seq_chain_pos];
        seq_queued = 0xFF;
    } else if (proj.song.song) {
        uint8_t rep = proj.song.rep[seq_chain_pos];
        if (++seq_chain_bar >= (rep ? rep : 1)) {
            uint8_t np;
            seq_chain_bar = 0;
            seq_chain_pos = chain_next(seq_chain_pos);
            np = proj.song.pat[seq_chain_pos];
            if (np < E2_PATTERNS) seq_cur = np;
        }
        seq_queued = 0xFF;
    } else if (seq_queued != 0xFF) {
        seq_cur = seq_queued;
        seq_queued = 0xFF;
    }
    if (seq_cur != prev) {
        seq_loop = 0;
        for (x = 0; x < XT_COUNT; x++) seq_xpos[x] = 0xFF;   /* restart at 0 below */
    } else seq_loop++;
    seq_phrase++;
}

/* CORROSION commit at the gate point of step 16, so the new material starts
 * exactly on the next downbeat.  ETCH II adds the edit epoch: a candidate
 * computed before a load, copy, paste or edit of its pattern is refused. */
static void evo_commit_point(void)
{
    uint8_t due, lp, np, nphrase = (uint8_t)(seq_phrase + 1);
    due = evo_request;
    if (proj.gen.mode == EVO_DRIFT && (nphrase & 3) == 0) due = 1;
    if (proj.gen.mode == EVO_CORRODE && (nphrase & 1) == 0) due = 1;
    if (!due || evo_state != EVO_READY) return;
    np = peek_next_pattern(&lp);
    if (evo_target != np || (proj.pat[np].lock & PLOCK_ALL)) return;
    if (evo_epoch != pat_epoch[np]) { evo_state = EVO_IDLE; evo_stale_drops++; return; }
    memcpy(&evo_undo, &proj.pat[np], sizeof(pattern_t));
    memcpy(&proj.pat[np], &evo_cand, sizeof(pattern_t));
    evo_undo_pat = np;
    evo_state = EVO_IDLE;
    evo_request = 0;
    evo_commits++;
    next_plays = decide(&proj.pat[np], 0, lp);              /* re-roll the new downbeat */
}

/* chord shapes: scale degrees above the root (three voices) */
static const uint8_t chord_deg[CHORD_SHAPES][3] = {
    { 0, 2, 4 }, { 0, 3, 4 }, { 0, 2, 6 }, { 0, 4, 7 }, { 0, 1, 4 }, { 0, 2, 5 }, { 0, 3, 6 }, { 0, 1, 2 }
};
static const uint16_t scale_mask_x[SCALE_COUNT] = { 0x05AD, 0x0AB5, 0x06AD, 0x05AB, 0x04A9, 0x0FFF };   /* = CORROSION's */

static uint8_t degree_up(uint8_t note, uint8_t deg)
{
    uint8_t key = proj.gen.key % 12, sc = proj.gen.scale % SCALE_COUNT, n = note, k;
    for (k = 0; k < deg; k++) {
        uint8_t t;
        for (t = 0; t < 12; t++) {
            n++;
            if ((scale_mask_x[sc] >> ((n + 120 - key) % 12)) & 1) break;
        }
    }
    return n;
}

static void x_play(uint8_t x, const xpattern_t *xp, uint8_t s, uint8_t glide)
{
    uint8_t trk = xtrk(x), f = xp->trk[x].flags[s], note = xp->trk[x].note[s];
    uint8_t acc = (f & STEP_ACCENT) != 0;
    if (x == XT_CHORD) {
        uint8_t sh = (uint8_t)(xp->shape[s] & 7);
        int32_t p0 = seq_track_pitch(trk, degree_up(note, chord_deg[sh][0]));
        voice_chord_on(V_CHORD0, trk, p0,
                       seq_track_pitch(trk, degree_up(note, chord_deg[sh][1])) - p0,
                       seq_track_pitch(trk, degree_up(note, chord_deg[sh][2])) - p0, acc, glide);
    } else {
        uint8_t v = x == XT_LEAD ? V_LEAD : x == XT_DRONE ? V_DRONE : V_TEXTURE;
        voice_note_on(v, trk, seq_track_pitch(trk, note), acc, glide);
    }
}

static void x_off(uint8_t x)
{
    if (x == XT_CHORD) voice_gate_off(V_CHORD0);
    else voice_gate_off(x == XT_LEAD ? V_LEAD : x == XT_DRONE ? V_DRONE : V_TEXTURE);
}

static void step_advance(void)
{
    const pattern_t *p;
    const xpattern_t *xp;
    uint8_t s, f, lanes = 0, lp, x;
    uint16_t act = 0;

    if (pending_start) {
        pending_start = 0;
        seq_step = 0;
        cur_plays = 0;
        legato = 0;
        for (x = 0; x < XT_COUNT; x++) { seq_xpos[x] = 0xFF; x_cur[x] = 0; x_leg[x] = 0; }
        next_plays = decide(&proj.pat[seq_cur], 0, seq_loop);
        xp = &proj.xpat[seq_cur];
        for (x = 0; x < XT_COUNT; x++) x_next[x] = xdecide(xp, x, 0);
    } else {
        if (!gate_done) seq_gate_late++;               /* would mean a gate was skipped */
        seq_step = (uint8_t)((seq_step + 1) & 15);
        if (seq_step == 0) bar_boundary();
    }
    s = seq_step;
    p = &proj.pat[seq_cur];
    xp = &proj.xpat[seq_cur];
    gate_done = 0;
    seq_step_event++;
    seq_steps_total++;
    voice_step_tick();

    /* ---- ORIGINAL drum lanes ---------------------------------------- */
    {
        const uint16_t *m = seq_fill ? seq_fill_mask : p->drum;
        uint8_t l;
        for (l = 0; l < NUM_LANES; l++) if (m[l] & (1u << s)) lanes |= (uint8_t)(1 << l);
        if (proj.mutes & (1u << TRK_KICK)) lanes &= (uint8_t)~1;
        if (proj.mutes & (1u << TRK_PERC)) lanes &= (uint8_t)~2;
        if (proj.mutes & (1u << TRK_HATS)) lanes &= (uint8_t)~12;
        if (lanes & 4) drums_choke(3);                       /* closed hat chokes open */
        for (l = 0; l < NUM_LANES; l++) if (lanes & (1 << l)) drum_trigger(l, p->kit[l], 0);
        if (lanes & 1) act |= 1u << TRK_KICK;
        if (lanes & 2) act |= 1u << TRK_PERC;
        if (lanes & 12) act |= 1u << TRK_HATS;
    }

    /* ---- ORIGINAL bass --------------------------------------------- */
    f = p->flags[s];
    if (next_plays) {
        voice_note_on(V_BASS, TRK_BASS, seq_track_pitch(TRK_BASS, p->note[s]), (uint8_t)(f & STEP_ACCENT), legato);
        cur_plays = 1;
        act |= 1u << TRK_BASS;
    } else if ((f & STEP_TYPE_MASK) == STEP_TIE && cur_plays && voices[V_BASS].held) {
        /* sustain */
    } else {
        cur_plays = 0;
        voice_gate_off(V_BASS);
    }
    legato = 0;
    if (s == 15) {
        uint8_t np = peek_next_pattern(&lp);
        next_plays = decide(&proj.pat[np], 0, lp);
    } else {
        next_plays = decide(p, (uint8_t)(s + 1), seq_loop);
    }

    /* ---- ETCH II extension tracks ------------------------------------ */
    for (x = 0; x < XT_COUNT; x++) {
        uint8_t xs, xf, np;
        const xpattern_t *nxp;
        xs = (uint8_t)(seq_xpos[x] + 1);
        if (xs >= xlen(xp, x)) xs = 0;
        seq_xpos[x] = xs;
        xf = xp->trk[x].flags[xs];
        if (x_next[x]) {
            x_play(x, xp, xs, x_leg[x]);
            x_cur[x] = 1;
            act |= 1u << xtrk(x);
        } else if ((xf & STEP_TYPE_MASK) == STEP_TIE && x_cur[x]) {
        } else {
            if (x_cur[x]) x_off(x);
            x_cur[x] = 0;
        }
        x_leg[x] = 0;
        nxp = x_next_pat(x, &np);
        x_next[x] = xdecide(nxp, x, np);
    }
    seq_activity = act;
}

static void gate_point(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    const xpattern_t *xp = &proj.xpat[seq_cur];
    uint8_t f, x;
    gate_done = 1;
    seq_gates_total++;
    if (seq_step == 15) evo_commit_point();
    f = p->flags[seq_step];
    if (cur_plays) {
        if ((f & STEP_SLIDE) && next_plays) legato = 1;      /* hold + glide */
        else if (next_type() != STEP_TIE) voice_gate_off(V_BASS);
    }
    for (x = 0; x < XT_COUNT; x++) {
        uint8_t np, xs = seq_xpos[x];
        const xpattern_t *nxp;
        if (!x_cur[x] || xs >= 16) continue;
        nxp = x_next_pat(x, &np);
        if ((xp->trk[x].flags[xs] & STEP_SLIDE) && x_next[x]) { x_leg[x] = 1; continue; }
        if ((nxp->trk[x].flags[np] & STEP_TYPE_MASK) == STEP_TIE) continue;
        x_off(x);
    }
}

/* called by the mixer before every block */
void seq_block(void)
{
    uint32_t old;
    if (!seq_playing) return;
    if (pending_start) { step_advance(); voice_govern(0); return; }
    old = pphase;
    pphase += pinc;
    /* Steps fire on transitions between the two halves of the pair: the wrap
     * starts an even 16th, entering [swing point, end) an odd one -- and only
     * when the next step has that parity.  ETCH fired the odd step whenever
     * the phase crossed the swing point; if SWING was raised while the phase
     * sat between the old and new swing points, the odd step fired twice and
     * step parity fell out of step with the phase, so every other gate point
     * was never reached (found by tests/emu_test.py, 220 BPM, swing 8). */
    {
        uint8_t region = pphase >= swing_pt, next_odd = (uint8_t)((seq_step + 1) & 1);
        uint8_t fire = 0;
        if (pphase < old) fire = !next_odd;                               /* even 16th */
        else if (!last_region && region) fire = next_odd;                 /* odd 16th  */
        last_region = region;
        if (fire) { step_advance(); voice_govern(0); return; }
    }
    if (!gate_done && pphase >= ((seq_step & 1) ? gate_odd : gate_even)) gate_point();
}

/* Tempo, swing, gate: ETCH's formulas with 32-bit phases.
 * One accumulator turn = two 16ths, so per block the increment is
 * bpm/30 pairs per second * BLOCK / fs * 2^32 = bpm * TIMER * 256 * BLOCK / 30
 * (fs = 2^24 / TIMER). */
void seq_set_tempo(void)
{
    uint32_t bpm = proj.bpm, sw = proj.swing, g = gate_frac[proj.gate & 3];
    uint32_t inc, sp, ge, go;
    if (bpm < 60) bpm = 60;
    if (bpm > 220) bpm = 220;
    if (sw > 8) sw = 8;
    inc = (uint32_t)(((uint64_t)bpm * MIX_TIMER_RELOAD * 256u * MIX_BLOCK) / 30u);
    sp = (0x8000u + sw * 1311u) << 16;
    ge = (uint32_t)(((uint64_t)sp * g) >> 4);
    go = sp + (uint32_t)((((uint64_t)0x100000000ull - sp) * g) >> 4);
    CRITICAL {
        pinc = inc; swing_pt = sp; gate_even = ge; gate_odd = go;
    }
    fx_set_tempo((uint8_t)bpm);
}

void seq_init(void)
{
    seq_playing = 0;
    seq_set_tempo();
}

void seq_seed_live(uint32_t s)
{
    live_rng = (s & 0xFFFF) ? (s & 0xFFFF) : 0x5EED;
    xlive_rng = s ? (s ^ 0xA5A5A5A5u) : 0xC0FFEE;
}

void seq_start(void)
{
    CRITICAL {
        seq_loop = 0;
        seq_phrase = 0;
        pphase = 0;
        last_region = 0;
        seq_chain_bar = 0;
        if (proj.song.song) {
            if (proj.song.pat[seq_chain_pos] >= E2_PATTERNS) seq_chain_pos = 0;
            if (proj.song.pat[seq_chain_pos] < E2_PATTERNS) seq_cur = proj.song.pat[seq_chain_pos];
        }
        pending_start = 1;
        seq_playing = 1;
    }
}

void seq_stop(void)
{
    CRITICAL {
        uint8_t v;
        seq_playing = 0;
        seq_fill = 0;
        seq_fill_hold = 0;
        fill_bars = 0;
        for (v = 0; v < N_MELODIC; v++) voice_gate_off(v);
    }
}

void seq_queue_pattern(uint8_t p)
{
    if (p >= E2_PATTERNS) return;
    CRITICAL {
        if (!seq_playing) { seq_cur = p; seq_queued = 0xFF; }
        else seq_queued = p;
    }
}

void seq_jump_chain(uint8_t pos)
{
    pos &= E2_SONG_LEN - 1;
    if (proj.song.pat[pos] >= E2_PATTERNS) return;
    CRITICAL {
        if (!seq_playing) { seq_chain_pos = pos; seq_chain_bar = 0; seq_cur = proj.song.pat[pos]; }
        else chain_jump = pos;
    }
}

void seq_fill_latch(uint8_t on, const uint16_t *masks)
{
    CRITICAL {
        if (on) {
            memcpy(seq_fill_mask, masks, sizeof(seq_fill_mask));
            seq_fill_hold = 1; seq_fill = 1; fill_bars = 1;
        } else {
            seq_fill_hold = 0; seq_fill = 0; fill_bars = 0;
        }
    }
}

void seq_fill_masks(const uint16_t *masks)
{
    CRITICAL { memcpy(seq_fill_mask, masks, sizeof(seq_fill_mask)); }
}

void seq_trigger_fill(const uint16_t *masks)
{
    CRITICAL {
        memcpy(seq_fill_mask, masks, sizeof(seq_fill_mask));
        fill_bars = (seq_playing && seq_step >= 12) ? 2 : 1;
        seq_fill = 1;
    }
}

void seq_pattern_edited(uint8_t pat)
{
    CRITICAL {
        if (pat < E2_PATTERNS) pat_epoch[pat]++;
        else { uint8_t i; for (i = 0; i < E2_PATTERNS; i++) pat_epoch[i]++; }
        evo_state = EVO_IDLE;
    }
}

void seq_backup_for_undo(uint8_t pat)
{
    CRITICAL {
        memcpy(&evo_undo, &proj.pat[pat], sizeof(pattern_t));
        evo_undo_pat = pat;
        evo_state = EVO_IDLE;
    }
}

uint8_t seq_undo(void)
{
    uint8_t r = 0;
    CRITICAL {
        if (evo_undo_pat < E2_PATTERNS) {
            memcpy(&proj.pat[evo_undo_pat], &evo_undo, sizeof(pattern_t));
            pat_epoch[evo_undo_pat]++;
            evo_undo_pat = 0xFF;
            evo_state = EVO_IDLE;
            evo_request = 0;
            r = 1;
        }
    }
    return r;
}

/* bars until the next scheduled evolution (UI) */
uint8_t seq_bars_to_evolution(void)
{
    uint8_t ph = seq_phrase, k;
    if (proj.gen.mode == EVO_STABLE) return 0xFF;
    for (k = 1; k <= 4; k++) {
        uint8_t n = (uint8_t)(ph + k);
        if (proj.gen.mode == EVO_DRIFT && (n & 3) == 0) return k;
        if (proj.gen.mode == EVO_CORRODE && (n & 1) == 0) return k;
    }
    return 4;
}
