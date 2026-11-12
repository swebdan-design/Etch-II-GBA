/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Project storage in cartridge SRAM (32 KB, the most widely supported GBA
 * save type on flash cartridges and emulators).
 */
#ifndef STORAGE_H
#define STORAGE_H
#include "etch2.h"

#define SAVE_SLOTS     4
#define SLOT_BYTES     8192
#define SAVE_VERSION   1
#define SLOT_EMPTY 0
#define SLOT_OK    1
#define SLOT_BAD   2          /* header present but version/size/checksum wrong */

void storage_init(void);
uint8_t storage_slot_status(uint8_t slot, char *name_out);   /* name_out: 10 chars */
uint8_t storage_save(uint8_t slot);       /* 1 = written and verified by read-back */
uint8_t storage_load(uint8_t slot);       /* 1 = loaded; 0 = empty/corrupt, proj untouched */
void storage_erase(uint8_t slot);
uint8_t storage_last_slot(void);          /* 0xFF if unknown */
void copy_in_chunks(void *dst, const void *src, uint32_t n);
uint32_t crc32_buf(const uint8_t *p, uint32_t n, uint32_t crc);
#endif
