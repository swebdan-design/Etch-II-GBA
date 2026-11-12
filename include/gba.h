/*
 * ETCH II -- A Generative Synthesis Machine -- by Synth Centric
 * Minimal Game Boy Advance hardware definitions (written for ETCH II;
 * register names follow GBATEK).
 */
#ifndef GBA_H
#define GBA_H
#include <stdint.h>

/* Functions defined in *.iwram.c files are compiled as ARM and linked into
 * IWRAM by gba_cart.ld; callers in ROM reach them through long calls. */
#define IWRAM_CODE __attribute__((long_call))
#define ROM_CODE   __attribute__((long_call))
#define IWRAM_DATA __attribute__((section(".iwram")))
#define EWRAM_DATA __attribute__((section(".ewram")))
#define EWRAM_BSS  __attribute__((section(".sbss")))
#define ALIGN4     __attribute__((aligned(4)))

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

/* display */
#define REG_DISPCNT   REG16(0x04000000)
#define REG_DISPSTAT  REG16(0x04000004)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG0CNT    REG16(0x04000008)
#define REG_BG1CNT    REG16(0x0400000A)
#define REG_BG2CNT    REG16(0x0400000C)
#define REG_BG3CNT    REG16(0x0400000E)
#define REG_BG0HOFS   REG16(0x04000010)
#define REG_BG0VOFS   REG16(0x04000012)
#define REG_BG1HOFS   REG16(0x04000014)
#define REG_BG1VOFS   REG16(0x04000016)
#define REG_BG2HOFS   REG16(0x04000018)
#define REG_BG2VOFS   REG16(0x0400001A)
#define REG_BLDCNT    REG16(0x04000050)
#define REG_BLDALPHA  REG16(0x04000052)
#define REG_BLDY      REG16(0x04000054)
#define DCNT_MODE0    0x0000
#define DCNT_BG0      0x0100
#define DCNT_BG1      0x0200
#define DCNT_BG2      0x0400
#define DCNT_BG3      0x0800
#define DCNT_BLANK    0x0080
#define DSTAT_VBL_IRQ 0x0008
#define BG_CBB(n)     ((n) << 2)
#define BG_SBB(n)     ((n) << 8)
#define BG_4BPP       0x0000
#define BG_PRIO(n)    (n)

#define PAL_BG   ((volatile uint16_t *)0x05000000)
#define PAL_OBJ  ((volatile uint16_t *)0x05000200)
#define VRAM     ((volatile uint16_t *)0x06000000)
#define CHARBLOCK(n)  ((volatile uint32_t *)(0x06000000 + (n) * 0x4000))
#define SCREENBLOCK(n) ((volatile uint16_t *)(0x06000000 + (n) * 0x800))
#define RGB15(r, g, b) ((uint16_t)((r) | ((g) << 5) | ((b) << 10)))

/* sound */
#define REG_SOUNDCNT_L REG16(0x04000080)
#define REG_SOUNDCNT_H REG16(0x04000082)
#define REG_SOUNDCNT_X REG16(0x04000084)
#define REG_SOUNDBIAS  REG16(0x04000088)
#define REG_FIFO_A     0x040000A0
#define REG_FIFO_B     0x040000A4
#define SNDH_PSG_100   0x0002
#define SNDH_A_100     0x0004
#define SNDH_B_100     0x0008
#define SNDH_A_R       0x0100
#define SNDH_A_L       0x0200
#define SNDH_A_T0      0x0000
#define SNDH_A_RESET   0x0800
#define SNDH_B_R       0x1000
#define SNDH_B_L       0x2000
#define SNDH_B_T0      0x0000
#define SNDH_B_RESET   0x8000
#define SNDX_ENABLE    0x0080

/* DMA */
#define REG_DMA1SAD   REG32(0x040000BC)
#define REG_DMA1DAD   REG32(0x040000C0)
#define REG_DMA1CNT   REG32(0x040000C4)
#define REG_DMA1CNT_H REG16(0x040000C6)
#define REG_DMA2SAD   REG32(0x040000C8)
#define REG_DMA2DAD   REG32(0x040000CC)
#define REG_DMA2CNT   REG32(0x040000D0)
#define REG_DMA2CNT_H REG16(0x040000D2)
#define REG_DMA3SAD   REG32(0x040000D4)
#define REG_DMA3DAD   REG32(0x040000D8)
#define REG_DMA3CNT   REG32(0x040000DC)
#define DMA_DST_FIXED 0x0040
#define DMA_REPEAT    0x0200
#define DMA_32        0x0400
#define DMA_SPECIAL   0x3000
#define DMA_ENABLE    0x8000
#define DMA_SRC_FIXED 0x0100

/* timers */
#define REG_TM0CNT_L  REG16(0x04000100)
#define REG_TM0CNT_H  REG16(0x04000102)
#define REG_TM1CNT_L  REG16(0x04000104)
#define REG_TM1CNT_H  REG16(0x04000106)
#define REG_TM2CNT_L  REG16(0x04000108)
#define REG_TM2CNT_H  REG16(0x0400010A)
#define REG_TM3CNT_L  REG16(0x0400010C)
#define REG_TM3CNT_H  REG16(0x0400010E)
#define TM_ENABLE     0x0080
#define TM_CASCADE    0x0004
#define TM_IRQ        0x0040

/* input */
#define REG_KEYINPUT  REG16(0x04000130)
#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define KEY_R      0x0100
#define KEY_L      0x0200
#define KEY_DPAD   (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)

/* interrupts, system */
#define REG_IE       REG16(0x04000200)
#define REG_IF       REG16(0x04000202)
#define REG_WAITCNT  REG16(0x04000204)
#define REG_IME      REG16(0x04000208)
#define IRQ_VBLANK   0x0001
#define BIOS_IRQ_VECTOR (*(volatile uint32_t *)0x03007FFC)

#define SRAM ((volatile uint8_t *)0x0E000000)

/* critical sections (main-loop side; the audio IRQ never nests) */
static inline uint16_t irq_off(void) { uint16_t v = REG_IME; REG_IME = 0; return v; }
static inline void irq_restore(uint16_t v) { REG_IME = v; }
#define CRITICAL for (uint16_t _ime = irq_off(), _once = 1; _once; irq_restore(_ime), _once = 0)

static inline void vblank_wait_bios(void) { __asm__ volatile("swi 0x05" ::: "r0", "r1", "r2", "r3", "memory"); }

#endif
