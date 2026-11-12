# ETCH II -- A Generative Synthesis Machine -- by Synth Centric
# Reproducible build of the native Game Boy Advance ROM.
#
# Toolchain: devkitARM (set DEVKITARM) or any arm-none-eabi GCC with newlib
# (Ubuntu: gcc-arm-none-eabi libnewlib-arm-none-eabi).  Python 3 + NumPy +
# Pillow generate tables, graphics and the album data.  See
# docs/BUILD_INSTRUCTIONS.md.

ifneq ($(DEVKITARM),)
PREFIX ?= $(DEVKITARM)/bin/arm-none-eabi-
else
PREFIX ?= arm-none-eabi-
endif
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
PYTHON  ?= python3
HOSTCC  ?= cc

# Mixer: samples per video frame (must divide 280 896 cycles) and samples
# per control block.  264/24 -> 15 768 Hz (release), 304/16 -> 18 157 Hz,
# 224/16 -> 13 379 Hz.  See docs/ETCH_II_Sound_Engine.md for measurements.
SPF     ?= 264
BLOCK   ?= 24

ROMNAME := ETCH_II_by_Synth_Centric
BUILD   := build
GEN     := $(BUILD)/gen

ARCH    := -mcpu=arm7tdmi -mtune=arm7tdmi
CFLAGS  := $(ARCH) -O2 -std=gnu11 -Wall -Wextra -Wshadow -Wno-unused-parameter -ffunction-sections -fdata-sections \
           -fno-strict-aliasing -Iinclude -Isrc/corrosion/legacy -I$(GEN)
THUMB   := -mthumb -mthumb-interwork
ARM     := -marm -mthumb-interwork
LDFLAGS := $(ARCH) -mthumb -mthumb-interwork -nostartfiles -specs=nosys.specs \
           -T src/main/platform/gba_cart.ld -Wl,--gc-sections -Wl,-Map,$(BUILD)/$(ROMNAME).map
CRTI    := $(shell $(CC) $(ARCH) -mthumb -print-file-name=crti.o)
CRTN    := $(shell $(CC) $(ARCH) -mthumb -print-file-name=crtn.o)
CRTB    := $(shell $(CC) $(ARCH) -mthumb -print-file-name=crtbegin.o)
CRTE    := $(shell $(CC) $(ARCH) -mthumb -print-file-name=crtend.o)

GENSRC  := $(GEN)/tables.c $(GEN)/gfx.c $(GEN)/album.c
SRCS    := $(sort $(wildcard src/*/*.c src/*/*/*.c src/*/*/*/*.c))
ASMS    := $(sort $(wildcard src/*/*/*.s src/*/*/*.S))
OBJS    := $(patsubst %.c,$(BUILD)/obj/%.o,$(SRCS) $(GENSRC)) $(patsubst %.s,$(BUILD)/obj/%.o,$(patsubst %.S,%.s,$(ASMS)))
HDRS    := $(wildcard include/*.h src/corrosion/legacy/*.h) $(GEN)/tables.h $(GEN)/gfx.h

.PHONY: all rom clean test hosttest compat emutest release pdf harness
all: rom
rom: $(BUILD)/$(ROMNAME).gba

$(GEN)/tables.c $(GEN)/tables.h: tools/gen_tables.py
	@mkdir -p $(GEN)
	$(PYTHON) tools/gen_tables.py $(SPF) $(BLOCK) $(GEN)/tables.c $(GEN)/tables.h

$(GEN)/gfx.c $(GEN)/gfx.h: tools/gen_gfx.py
	@mkdir -p $(GEN)
	$(PYTHON) tools/gen_gfx.py $(GEN)/gfx.c $(GEN)/gfx.h

$(BUILD)/compose_album: tools/compose_album.c src/corrosion/legacy/corrosion.c src/corrosion/legacy/pattern.c src/corrosion/legacy/names.c src/expansion/expand.c src/instruments/instruments.c include/etch2.h include/expand.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c99 -O1 -Wall -DHOST_TOOL -Iinclude -Isrc/corrosion/legacy -o $@ tools/compose_album.c \
	  src/corrosion/legacy/corrosion.c src/corrosion/legacy/pattern.c src/corrosion/legacy/names.c \
	  src/expansion/expand.c src/instruments/instruments.c

$(GEN)/album.c: $(BUILD)/compose_album
	@mkdir -p $(GEN)
	$(BUILD)/compose_album $(GEN)/album.c docs/generated_album_recipe.txt

$(BUILD)/obj/%.iwram.o: %.iwram.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ARM) -O2 -c -o $@ $<

$(BUILD)/obj/%.o: %.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(THUMB) -c -o $@ $<

$(BUILD)/obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ARCH) -mthumb-interwork -x assembler-with-cpp -c -o $@ $<

$(BUILD)/obj/%.o: %.S $(GEN)/tables.h
	@mkdir -p $(dir $@)
	$(CC) $(ARCH) -mthumb-interwork -DMIX_BLOCK_N=$(BLOCK) -x assembler-with-cpp -c -o $@ $<

$(BUILD)/gbafix: tools/third_party/gbafix/gbafix.c
	@mkdir -p $(BUILD)
	$(HOSTCC) -O2 -w -o $@ $<

$(BUILD)/$(ROMNAME).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(CRTI) $(CRTB) $(OBJS) -lc -lgcc $(CRTE) $(CRTN)

$(BUILD)/$(ROMNAME).gba: $(BUILD)/$(ROMNAME).elf $(BUILD)/gbafix
	$(OBJCOPY) -O binary $< $@
	$(BUILD)/gbafix $@ -t"ETCH II" -cAE2E -mSC -r0
	$(PYTHON) tools/check_rom.py $@

# ---- tests ---------------------------------------------------------------
harness: $(BUILD)/mgba_harness
$(BUILD)/mgba_harness: tests/harness/mgba_harness.c
	@mkdir -p $(BUILD)
	$(HOSTCC) -O2 -Wall -o $@ $< -lmgba

hosttest:
	$(PYTHON) tests/run_host_tests.py

compat: harness
	$(PYTHON) tests/compat/run_compat.py

emutest: rom harness
	$(PYTHON) tests/emu_test.py $(BUILD)/$(ROMNAME).gba

test: hosttest compat emutest

release: rom
	$(PYTHON) tools/make_release.py

clean:
	rm -rf $(BUILD)
