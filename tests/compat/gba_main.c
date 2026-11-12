/*
 * ETCH II -- by Synth Centric.  GBA test ROM for the CORROSION corpus: runs
 * ETCH II's copy of CORROSION (src/corrosion/legacy), compiled exactly as in
 * the ETCH II ROM (arm-none-eabi-gcc, Thumb, -O2), and stores the section
 * CRCs in gba_result for tests/compat/run_compat.py.
 */
#include <stdint.h>
#define N_SECT 7
typedef struct { uint32_t magic, n; uint32_t crc[N_SECT]; uint32_t bytes[N_SECT]; } corpus_result_t;
void corpus_run(corpus_result_t *r);
corpus_result_t gba_result;
char *fake_heap_end, *fake_heap_start;
int main(void)
{
    corpus_run(&gba_result);
    for (;;) __asm__ volatile("swi 0x02" ::: "memory");   /* Halt */
}
