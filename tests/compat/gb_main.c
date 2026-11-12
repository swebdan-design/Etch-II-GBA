/*
 * ETCH II -- by Synth Centric.  Game Boy reference ROM for the CORROSION
 * corpus: links the ORIGINAL ETCH generator sources, compiled by GBDK-2020
 * 4.3.0 / SDCC 4.4.1 exactly as ETCH is, and stores the section CRCs in
 * WRAM (symbol _gb_result) for tests/compat/run_compat.py to read.
 */
#include <gbdk/platform.h>
#include "corrosion.h"
#define N_SECT 7
typedef struct { uint32_t magic, n; uint32_t crc[N_SECT]; uint32_t bytes[N_SECT]; } corpus_result_t;
void corpus_run(corpus_result_t *r);
corpus_result_t gb_result;
void main(void)
{
    corpus_run(&gb_result);
    while (1) { wait_vbl_done(); }
}
