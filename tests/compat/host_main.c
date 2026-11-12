/*
 * ETCH II -- by Synth Centric.  Host driver for the CORROSION corpus.
 * usage: corpus_host OUTFILE   (prints section CRCs as JSON on stdout)
 */
#include <stdio.h>
#include "corrosion.h"

#define N_SECT 7
typedef struct {
    uint32_t magic, n;
    uint32_t crc[N_SECT];
    uint32_t bytes[N_SECT];
} corpus_result_t;
void corpus_run(corpus_result_t *r);
extern FILE *corpus_dumpf_hook;

int main(int argc, char **argv)
{
    corpus_result_t r;
    int i;
    if (argc > 1) corpus_dumpf_hook = fopen(argv[1], "wb");
    corpus_run(&r);
    if (corpus_dumpf_hook) fclose(corpus_dumpf_hook);
    printf("{\"n\": %u, \"magic\": \"%08X\", \"crc\": [", (unsigned)r.n, (unsigned)r.magic);
    for (i = 0; i < N_SECT; i++) printf("%s\"%08X\"", i ? ", " : "", (unsigned)r.crc[i]);
    printf("], \"bytes\": [");
    for (i = 0; i < N_SECT; i++) printf("%s%u", i ? ", " : "", (unsigned)r.bytes[i]);
    printf("]}\n");
    return r.magic == 0xC0DEC0DEUL ? 0 : 1;
}
