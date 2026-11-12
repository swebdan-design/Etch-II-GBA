/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Project persistence in battery-backed SRAM.
 *
 * Layout: 4 slots x 8 192 bytes.  Each slot = 32-byte header + project2_t:
 *   0  "ETC2"            4  version          5  album id
 *   6  size (LE16)       8  CRC-32 (LE32) of the data
 *   12 name[10]          22 sequence number (LE32, last saved wins)
 *   26 reserved          32 data ...
 * A save snapshots the project in a short critical section (so playback and
 * evolution keep running and can never leave a half-updated image), clears
 * the magic, writes the data, computes the CRC from what is actually in
 * SRAM, writes the header last and reads everything back.  Missing, foreign
 * or corrupted slots are reported as EMPTY / BAD and are never loaded; the
 * working project is replaced only after full verification.
 * SRAM is only ever accessed with 8-bit reads and writes, as required.
 */
#include "storage.h"
#include "gba.h"
#include "seq.h"

#define HDR 32
_Static_assert(sizeof(project2_t) + HDR <= SLOT_BYTES, "project does not fit a save slot");

/* save-type marker for emulators and flash cartridges */
__attribute__((used, aligned(4))) const char save_type_id[12] = "SRAM_V113";

static project2_t snap EWRAM_BSS;
static uint32_t seq_no;
static uint32_t crc_tab[256] EWRAM_BSS;

/* CRC-32 (IEEE, as zlib), table driven */
static void crc_init(void)
{
    uint32_t i, c;
    uint8_t k;
    for (i = 0; i < 256; i++) {
        for (c = i, k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        crc_tab[i] = c;
    }
}

uint32_t crc32_buf(const uint8_t *p, uint32_t n, uint32_t crc)
{
    uint32_t i;
    crc = ~crc;
    for (i = 0; i < n; i++) crc = crc_tab[(crc ^ p[i]) & 255] ^ (crc >> 8);
    return ~crc;
}

/* replace the working project in short critical sections (the audio IRQ is
 * never held off for more than a few hundred cycles) */
void copy_in_chunks(void *dst, const void *src, uint32_t n)
{
    uint32_t off;
    for (off = 0; off < n; off += 256) {
        uint32_t k = n - off < 256 ? n - off : 256;
        CRITICAL { memcpy((uint8_t *)dst + off, (const uint8_t *)src + off, k); }
    }
}

static volatile uint8_t *base(uint8_t slot) { return SRAM + (uint32_t)slot * SLOT_BYTES; }

static uint32_t rd32(volatile uint8_t *b) { return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
static void wr32(volatile uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }

static uint32_t sram_crc(volatile uint8_t *b, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu, i;
    for (i = 0; i < n; i++) crc = crc_tab[(crc ^ b[i]) & 255] ^ (crc >> 8);
    return ~crc;
}

static uint8_t check(uint8_t slot)
{
    volatile uint8_t *b = base(slot);
    uint16_t size;
    if (b[0] != 'E' || b[1] != 'T' || b[2] != 'C' || b[3] != '2') return SLOT_EMPTY;
    size = (uint16_t)(b[6] | (b[7] << 8));
    if (b[4] != SAVE_VERSION || size != sizeof(project2_t)) return SLOT_BAD;
    if (sram_crc(b + HDR, size) != rd32(b + 8)) return SLOT_BAD;
    return SLOT_OK;
}

void storage_init(void)
{
    uint8_t s;
    (void)*(volatile const char *)save_type_id;     /* keep the marker in the ROM */
    crc_init();
    seq_no = 0;
    for (s = 0; s < SAVE_SLOTS; s++)
        if (check(s) == SLOT_OK && rd32(base(s) + 22) > seq_no) seq_no = rd32(base(s) + 22);
}

uint8_t storage_slot_status(uint8_t slot, char *name_out)
{
    uint8_t r, i;
    if (slot >= SAVE_SLOTS) return SLOT_BAD;
    r = check(slot);
    if (r == SLOT_OK && name_out)
        for (i = 0; i < E2_NAME_LEN; i++) name_out[i] = (char)base(slot)[12 + i];
    return r;
}

uint8_t storage_save(uint8_t slot)
{
    volatile uint8_t *b;
    const uint8_t *src = (const uint8_t *)&snap;
    uint32_t i, crc;
    if (slot >= SAVE_SLOTS) return 0;
    /* consistent snapshot without blocking the audio interrupt: the IRQ only
     * changes the project by committing an evolution, so copy until no
     * commit happened during the copy */
    {
        uint8_t c;
        do { c = evo_commits; memcpy(&snap, &proj, sizeof snap); } while (c != evo_commits);
    }
    b = base(slot);
    b[0] = 0;                                         /* invalidate while writing */
    for (i = 0; i < sizeof snap; i++) b[HDR + i] = src[i];
    crc = sram_crc(b + HDR, sizeof snap);
    if (crc != crc32_buf(src, sizeof snap, 0)) return 0;   /* read-back mismatch */
    b[4] = SAVE_VERSION;
    b[5] = snap.album_id;
    b[6] = (uint8_t)sizeof snap;
    b[7] = (uint8_t)(sizeof snap >> 8);
    wr32(b + 8, crc);
    for (i = 0; i < E2_NAME_LEN; i++) b[12 + i] = (uint8_t)snap.name[i];
    wr32(b + 22, ++seq_no);
    for (i = 26; i < HDR; i++) b[i] = 0;
    b[1] = 'T'; b[2] = 'C'; b[3] = '2';
    b[0] = 'E';
    return check(slot) == SLOT_OK;
}

uint8_t storage_load(uint8_t slot)
{
    volatile uint8_t *b;
    uint8_t *dst = (uint8_t *)&snap;
    uint32_t i;
    if (slot >= SAVE_SLOTS || check(slot) != SLOT_OK) return 0;
    b = base(slot);
    for (i = 0; i < sizeof snap; i++) dst[i] = b[HDR + i];
    if (crc32_buf(dst, sizeof snap, 0) != rd32(b + 8)) return 0;
    copy_in_chunks(&proj, &snap, sizeof snap);
    return 1;
}

void storage_erase(uint8_t slot)
{
    volatile uint8_t *b;
    if (slot >= SAVE_SLOTS) return;
    b = base(slot);
    b[0] = 0; b[1] = 0; b[2] = 0; b[3] = 0;
}

uint8_t storage_last_slot(void)
{
    uint8_t s, best = 0xFF;
    uint32_t bs = 0;
    for (s = 0; s < SAVE_SLOTS; s++)
        if (check(s) == SLOT_OK && rd32(base(s) + 22) >= bs) { bs = rd32(base(s) + 22); best = s; }
    return best;
}
