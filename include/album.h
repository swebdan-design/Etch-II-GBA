/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * The album: eight living compositions (data built by tools/compose_album.c).
 */
#ifndef ALBUM_H
#define ALBUM_H
#include "etch2.h"

#define ALBUM_TRACKS 8
typedef struct {
    const char *title;          /* e.g. "OXIDE" */
    const char *line1, *line2;  /* two short description lines (<= 28 chars) */
    const char *code;           /* legacy 12-digit seed of the core groove */
    const uint8_t *hdr;         /* project2_t bytes before .pat */
    uint8_t npat;
    const uint8_t *pat;         /* npat * 64 bytes (ORIGINAL pattern format) */
    const uint8_t *xpat;        /* npat * sizeof(xpattern_t) */
} album_track_t;

extern const album_track_t album[ALBUM_TRACKS];
void album_load(uint8_t i, project2_t *p);

#endif
