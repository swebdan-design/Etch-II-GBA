/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * CORROSION bridge (main-loop side), ported from ETCH v2.5 src/main.c:
 *   - per-pattern DRIFT anchors (8-slot cache, as ETCH);
 *   - the entropy pool stirred by button timing, used ONLY to seed
 *     evolution and REGEN (never to generate from a seed code);
 *   - background preparation of the next mutation for the pattern that
 *     will play after the next bar line; the audio interrupt commits it;
 *   - REGEN material composed ahead of time, FILL, UNDO, sound drift.
 * Everything that changes musical data calls the ORIGINAL CORROSION
 * functions with ETCH's arguments.
 *
 * ETCH II addition: every candidate carries the edit epoch of the pattern it
 * was computed from; the commit refuses a candidate whose pattern has since
 * been loaded, copied, pasted, edited or undone (ETCH documented this race
 * as a potential issue).
 */
#include "evolve.h"
#include "expand.h"
#include "seq.h"
#include "audio.h"

pattern_t anchors[N_ANCHORS] EWRAM_BSS;
uint8_t anchor_id[N_ANCHORS] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t anchor_next;
static uint16_t evo_rng = 0x2601;
static pattern_t evo_tmp EWRAM_BSS;
static uint16_t evo_ctr;
uint32_t ui_entropy;
uint16_t evo_rng_peek;

void evo_init(void)
{
    REG_TM2CNT_H = 0;
    REG_TM2CNT_L = 0;
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = TM_ENABLE | TM_CASCADE;
    REG_TM2CNT_H = TM_ENABLE;                   /* 32-bit cycle counter for entropy */
}

static uint32_t hwclock(void) { return ((uint32_t)REG_TM3CNT_L << 16) | REG_TM2CNT_L; }

void evo_stir(uint32_t x)
{
    ui_entropy = (ui_entropy * 31u) ^ x ^ hwclock();
}

/* ETCH's evo_random(): the pool stirred into a 16-bit xorshift */
uint16_t evo_random(void)
{
    evo_rng ^= (uint16_t)(ui_entropy ^ (ui_entropy >> 16) ^ hwclock());
    evo_rng ^= (uint16_t)(evo_rng << 7);
    evo_rng ^= (uint16_t)(evo_rng >> 9);
    evo_rng ^= (uint16_t)(evo_rng << 8);
    if (!evo_rng) evo_rng = 0x2601;
    evo_rng_peek = evo_rng;
    return evo_rng;
}

void anchors_forget(uint8_t pat)
{
    uint8_t i;
    for (i = 0; i < N_ANCHORS; i++)
        if (pat == 0xFF || anchor_id[i] == pat) anchor_id[i] = 0xFF;
}

static pattern_t *anchor_for(uint8_t pat)
{
    uint8_t i;
    for (i = 0; i < N_ANCHORS; i++)
        if (anchor_id[i] == pat) return &anchors[i];
    i = anchor_next;
    anchor_next = (uint8_t)((anchor_next + 1) & (N_ANCHORS - 1));
    anchor_id[i] = pat;
    memcpy(&anchors[i], &proj.pat[pat], sizeof(pattern_t));
    return &anchors[i];
}

uint8_t anchor_divergence(uint8_t slot)
{
    const pattern_t *a = &anchors[slot], *p;
    uint8_t s, l, d = 0;
    if (anchor_id[slot] >= E2_PATTERNS) return 0;
    p = &proj.pat[anchor_id[slot]];
    for (s = 0; s < 16; s++)
        if (p->note[s] != a->note[s] || p->flags[s] != a->flags[s] || p->prob[s] != a->prob[s]) d++;
    for (l = 0; l < NUM_LANES; l++) {
        uint16_t x = p->drum[l] ^ a->drum[l];
        while (x) { d = (uint8_t)(d + (x & 1)); x >>= 1; }
    }
    return d;
}

/* After each committed evolution the SOUND drifts too (ETCH sound_drift):
 * bass tone and each drum lane's tuning take a small bounded random step.
 * Live offsets only; never saved; reset in STABLE. */
void evo_after_commit(void)
{
    uint8_t l, r;
    int8_t lim = proj.gen.mode == EVO_CORRODE ? 5 : 2;
    int8_t dlim = proj.gen.mode == EVO_CORRODE ? 2 : 1;
    int8_t v;
    if (proj.gen.mode == EVO_STABLE) return;
    r = (uint8_t)evo_random();
    v = (int8_t)(evo_cut_ofs + (int8_t)((r & 3) == 0 ? -1 : (r & 3) == 1 ? 1 : 0));
    if (v > lim) v = lim;
    if (v < -lim) v = -lim;
    evo_cut_ofs = v;
    for (l = 0; l < NUM_LANES; l++) {
        r >>= 2;
        if (l == 2) r = (uint8_t)(evo_random() >> 3);
        v = (int8_t)(evo_tune_ofs[l] + (int8_t)((r & 3) == 0 ? -1 : (r & 3) == 1 ? 1 : 0));
        if (v > dlim) v = dlim;
        if (v < -dlim) v = -dlim;
        evo_tune_ofs[l] = v;
    }
}

void evo_stable_reset(void)
{
    uint8_t l;
    if (proj.gen.mode != EVO_STABLE) return;
    evo_cut_ofs = 0;
    for (l = 0; l < NUM_LANES; l++) evo_tune_ofs[l] = 0;
}

void app_pattern_edited(uint8_t pat)
{
    seq_pattern_edited(pat);
    anchors_forget(pat);                       /* your edit becomes the new memory */
}

/* Prepare the next mutation for the pattern that will play after the next
 * bar line (ETCH evo_service). */
uint8_t evo_service(void)
{
    uint8_t mode = proj.gen.mode, target, ep;
    target = seq_playing ? seq_next_bar_pattern() : seq_cur;
    if (evo_state == EVO_READY && (evo_target != target || evo_epoch != pat_epoch[target])) {
        CRITICAL { if (evo_state == EVO_READY) evo_state = EVO_IDLE; }
    }
    if (evo_state != EVO_IDLE) return 0;
    if (mode == EVO_STABLE && !evo_request) return 0;
    ep = pat_epoch[target];
    corr_mutate(&evo_tmp, &proj.pat[target], anchor_for(target), &proj.gen,
                mode == EVO_STABLE ? EVO_DRIFT : mode, proj.gen.mutation,
                (uint16_t)(evo_random() + (++evo_ctr)));
    CRITICAL {
        if (evo_state == EVO_IDLE && pat_epoch[target] == ep) {
            memcpy(&evo_cand, &evo_tmp, sizeof(pattern_t));
            evo_target = target;
            evo_epoch = ep;
            evo_state = EVO_READY;
        }
    }
    return 1;
}

/* REGEN material is composed in the background so the tap is instant */
static pattern_t regen_mat EWRAM_BSS;
static uint8_t regen_ready;
static gen_t regen_for;

uint8_t regen_service(void)
{
    if (regen_ready && !memcmp(&regen_for, &proj.gen, 8)) return 0;    /* key..octave */
    memcpy(&regen_for, &proj.gen, sizeof(gen_t));
    corr_regen_material(&regen_mat, &proj.gen, evo_random());
    regen_ready = 1;
    return 1;
}

uint16_t app_regen(uint8_t what)
{
    uint16_t mask = 0;
    uint8_t cur = seq_cur;
    pattern_t *p = &proj.pat[cur];
    if (p->lock & PLOCK_ALL) return 0;
    regen_service();
    seq_backup_for_undo(cur);
    CRITICAL { mask = corr_regen_apply(p, &regen_mat, evo_random(), what); }
    regen_ready = 0;
    if (mask) app_pattern_edited(cur);
    return mask;
}

void app_evolve_now(void)
{
    uint8_t cur = seq_cur;
    if (proj.pat[cur].lock & PLOCK_ALL) return;
    evo_request = 1;
    if (seq_playing) return;
    /* stopped: mutate and commit immediately */
    evo_state = EVO_IDLE;
    evo_service();
    CRITICAL {
        if (evo_state == EVO_READY && evo_target == cur && evo_epoch == pat_epoch[cur]) {
            memcpy(&evo_undo, &proj.pat[cur], sizeof(pattern_t));
            memcpy(&proj.pat[cur], &evo_cand, sizeof(pattern_t));
            evo_undo_pat = cur;
            evo_state = EVO_IDLE;
            evo_request = 0;
            evo_commits++;
        }
    }
}

uint8_t app_undo(void)
{
    return seq_undo();                         /* as ETCH: anchors are kept */
}

static uint8_t fill_bar, fill_pat = 0xFF;

void app_fill_toggle(void)
{
    uint16_t m[NUM_LANES];
    if (seq_fill_hold) {
        seq_fill_latch(0, m);
    } else {
        corr_fill(&proj.pat[seq_cur], evo_random(), m);
        seq_fill_latch(1, m);
        fill_bar = seq_bar_total;
        fill_pat = seq_cur;
    }
}

void app_fill(void)
{
    uint16_t m[NUM_LANES];
    corr_fill(&proj.pat[seq_cur], evo_random(), m);
    seq_trigger_fill(m);
}

void app_fill_service(void)
{
    if (seq_fill_hold && (seq_bar_total != fill_bar || seq_cur != fill_pat)) {
        uint16_t m[NUM_LANES];
        fill_bar = seq_bar_total;
        fill_pat = seq_cur;
        corr_fill(&proj.pat[seq_cur], evo_random(), m);
        seq_fill_masks(m);
    }
}

/* GEN ALL / GEN BASS / GEN DRUMS on the current pattern, from proj.gen (a
 * seed code reproduces this exactly).  In EXPANDED mode the extension
 * tracks are composed too, by the separate EXPANSION generator. */
void app_generate(uint8_t what)
{
    uint8_t cur = seq_cur;
    pattern_t tmp;
    seq_backup_for_undo(cur);
    memcpy(&tmp, &proj.pat[cur], sizeof tmp);
    corr_generate(&tmp, &proj.gen, what);                 /* ORIGINAL CORROSION */
    CRITICAL { memcpy(&proj.pat[cur], &tmp, sizeof tmp); }
    if (proj.compat == COMPAT_EXPANDED) {
        xpattern_t x;
        memcpy(&x, &proj.xpat[cur], sizeof x);
        expand_generate(&x, &tmp, &proj.gen, proj.xseed, cur, XG_ALL);
        CRITICAL { memcpy(&proj.xpat[cur], &x, sizeof x); }
    }
    app_pattern_edited(cur);
}

void app_project_loaded(void)
{
    CRITICAL {
        seq_cur = proj.start_pattern < E2_PATTERNS ? proj.start_pattern : 0;
        seq_queued = 0xFF;
        seq_chain_pos = 0;
        seq_chain_bar = 0;
        if (proj.song.song && proj.song.pat[0] < E2_PATTERNS) seq_cur = proj.song.pat[0];
        evo_state = EVO_IDLE;
        evo_request = 0;
        evo_undo_pat = 0xFF;
    }
    seq_pattern_edited(0xFF);                  /* every pattern: new epoch */
    anchors_forget(0xFF);
    regen_ready = 0;
    evo_cut_ofs = 0;
    { uint8_t l; for (l = 0; l < NUM_LANES; l++) evo_tune_ofs[l] = 0; }
    seq_set_tempo();
    drum_cache_prime();
}
