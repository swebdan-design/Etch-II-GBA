/* temporary bring-up stubs */
#include "ui.h"
#define STUB(n, t) void scr_##n##_enter(void) { ui_header(t); } void scr_##n##_frame(void) {}
STUB(track, "TRACK") STUB(synth, "SYNTH") STUB(mod, "MOD") STUB(gen, "GEN") STUB(memory, "MEMORY") STUB(song, "SONG") STUB(file, "FILE")
