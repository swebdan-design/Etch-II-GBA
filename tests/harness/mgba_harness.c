/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Headless mGBA test harness (test tool only, not part of the ROM).
 * Works for both GBA (.gba) and Game Boy (.gb) ROMs via libmgba.
 *
 * Reads one command per line on stdin, answers one line on stdout:
 *   run N              run N frames                       -> "ok FRAMECOUNT"
 *   keys MASK          set held buttons (bit0 A,1 B,2 SEL,3 START,
 *                      4 RIGHT,5 LEFT,6 UP,7 DOWN,8 R,9 L) -> "ok"
 *   peek ADDR N        N bytes from the bus, hex          -> "HEX..."
 *   poke ADDR HEX      write bytes                        -> "ok"
 *   shot FILE          current frame as binary PPM        -> "ok W H"
 *   wav FILE           start recording stereo 16-bit WAV  -> "ok RATE"
 *   wavstop            finish the WAV                     -> "ok FRAMES"
 *   savebat FILE       write battery/save memory          -> "ok BYTES"
 *   quit
 * argv: mgba_harness ROM [SAVEFILE]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <mgba/core/core.h>
#include <mgba/core/blip_buf.h>
#include <mgba-util/vfs.h>
#include <mgba/core/log.h>
#include <stdarg.h>

#define RATE 48000
static void nolog(struct mLogger *l, int cat, enum mLogLevel lvl, const char *fmt, va_list args)
{
    (void)l; (void)cat; (void)lvl; (void)fmt; (void)args;
}
static struct mLogger null_logger = { .log = nolog };
static struct mCore *core;
static color_t *video;
static unsigned vw, vh;
static FILE *wavf;
static uint32_t wav_frames;
static int16_t abuf[8192];

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void wav_header(FILE *f, uint32_t frames)
{
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); put32(f, 36 + frames * 4); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, RATE); put32(f, RATE * 4); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4);
}

static void drain_audio(void)
{
    struct blip_t *l = core->getAudioChannel(core, 0);
    struct blip_t *r = core->getAudioChannel(core, 1);
    int n = blip_samples_avail(l);
    while (n > 0) {
        int k = n > 4096 ? 4096 : n;
        blip_read_samples(l, abuf, k, 1);
        blip_read_samples(r, abuf + 1, k, 1);
        if (wavf) { fwrite(abuf, 4, (size_t)k, wavf); wav_frames += (uint32_t)k; }
        n -= k;
    }
}

int main(int argc, char **argv)
{
    char line[4096];
    if (argc < 2) { fprintf(stderr, "usage: %s ROM [SAVE]\n", argv[0]); return 2; }
    mLogSetDefaultLogger(&null_logger);
    core = mCoreFind(argv[1]);
    if (!core || !core->init(core)) { fprintf(stderr, "no core for %s\n", argv[1]); return 1; }
    mCoreInitConfig(core, NULL);
    core->desiredVideoDimensions(core, &vw, &vh);
    video = calloc((size_t)vw * vh, sizeof(color_t));
    core->setVideoBuffer(core, video, vw);
    core->setAudioBufferSize(core, 8192);
    blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), RATE);
    blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), RATE);
    if (!mCoreLoadFile(core, argv[1])) { fprintf(stderr, "load failed\n"); return 1; }
    if (argc > 2) {
        struct VFile *vf = VFileOpen(argv[2], O_CREAT | O_RDWR);
        if (vf) core->loadSave(core, vf);
    }
    core->reset(core);
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (fgets(line, sizeof line, stdin)) {
        char cmd[32], arg[2048];
        unsigned long a1 = 0, a2 = 0;
        arg[0] = 0;
        if (sscanf(line, "%31s", cmd) != 1) continue;
        if (!strcmp(cmd, "run")) {
            sscanf(line, "%*s %lu", &a1);
            while (a1--) { core->runFrame(core); drain_audio(); }
            printf("ok %u\n", core->frameCounter(core));
        } else if (!strcmp(cmd, "keys")) {
            sscanf(line, "%*s %lu", &a1);
            core->setKeys(core, (uint32_t)a1);
            printf("ok\n");
        } else if (!strcmp(cmd, "peek")) {
            unsigned long i;
            sscanf(line, "%*s %lx %lu", &a1, &a2);
            for (i = 0; i < a2; i++) printf("%02X", core->busRead8(core, (uint32_t)(a1 + i)) & 0xFF);
            printf("\n");
        } else if (!strcmp(cmd, "poke")) {
            size_t i, n;
            sscanf(line, "%*s %lx %2047s", &a1, arg);
            n = strlen(arg) / 2;
            for (i = 0; i < n; i++) {
                unsigned v; sscanf(arg + 2 * i, "%2x", &v);
                core->busWrite8(core, (uint32_t)(a1 + i), (uint8_t)v);
            }
            printf("ok\n");
        } else if (!strcmp(cmd, "shot")) {
            FILE *f; unsigned x, y;
            sscanf(line, "%*s %2047s", arg);
            f = fopen(arg, "wb");
            fprintf(f, "P6\n%u %u\n255\n", vw, vh);
            for (y = 0; y < vh; y++)
                for (x = 0; x < vw; x++) {
                    uint32_t c = (uint32_t)video[y * vw + x];
                    fputc(c & 0xFF, f); fputc((c >> 8) & 0xFF, f); fputc((c >> 16) & 0xFF, f);
                }
            fclose(f);
            printf("ok %u %u\n", vw, vh);
        } else if (!strcmp(cmd, "wav")) {
            sscanf(line, "%*s %2047s", arg);
            drain_audio();
            wavf = fopen(arg, "wb");
            wav_frames = 0;
            wav_header(wavf, 0);
            printf("ok %d\n", RATE);
        } else if (!strcmp(cmd, "wavstop")) {
            if (wavf) { wav_header(wavf, wav_frames); fclose(wavf); wavf = NULL; }
            printf("ok %u\n", wav_frames);
        } else if (!strcmp(cmd, "savebat")) {
            void *data = NULL; size_t n; FILE *f;
            sscanf(line, "%*s %2047s", arg);
            n = core->savedataClone(core, &data);
            f = fopen(arg, "wb");
            if (f) { if (n) fwrite(data, 1, n, f); fclose(f); }
            free(data);
            printf("ok %zu\n", n);
        } else if (!strcmp(cmd, "trace")) {
            /* step until PC enters [lo, hi); print the last 64 PCs */
            static uint32_t ring[64];
            unsigned long lo = 0, hi = 0, maxn = 0, k = 0, i;
            int32_t pc = 0;
            sscanf(line, "%*s %lx %lx %lu", &lo, &hi, &maxn);
            for (k = 0; k < maxn; k++) {
                core->step(core);
                core->readRegister(core, "pc", &pc);
                ring[k & 63] = (uint32_t)pc;
                if ((uint32_t)pc >= lo && (uint32_t)pc < hi) break;
            }
            for (i = (k >= 63 ? k - 63 : 0); i <= k && i < maxn; i++) printf("%08X ", ring[i & 63]);
            printf("\n");
        } else if (!strcmp(cmd, "reg")) {
            int32_t val = 0;
            sscanf(line, "%*s %2047s", arg);
            if (core->readRegister(core, arg, &val)) printf("%08X\n", (uint32_t)val);
            else printf("err no register\n");
        } else if (!strcmp(cmd, "quit")) {
            break;
        } else {
            printf("err unknown command\n");
        }
    }
    if (wavf) { wav_header(wavf, wav_frames); fclose(wavf); }
    core->deinit(core);
    return 0;
}
