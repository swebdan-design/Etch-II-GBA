# Third-Party Notices

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

## Linked into the ROM

| Component | Files | Licence |
|---|---|---|
| devkitPro devkitarm-crtls (commit in `src/main/platform/UPSTREAM.txt`) | `src/main/platform/gba_crt0.s`, `gba_cart.ld` — unmodified | **Mozilla Public License 2.0** (file-level; the source of these two files is distributed with ETCH II, unmodified) |
| newlib C library (Ubuntu `libnewlib-arm-none-eabi` 4.4.0) | `memcpy`, `memset`, `memcmp` | BSD-style newlib licences |
| libgcc (GCC 13.2) | integer division helpers | GPL v3 with the GCC Runtime Library Exception |
| CORROSION from ETCH 2.5 | `src/corrosion/legacy/` | MIT (Synth Centric) |

## Build tools (not distributed in the ROM)

| Tool | Licence | Use |
|---|---|---|
| gbafix from devkitPro gba-tools (`tools/third_party/gbafix/`, unmodified source + COPYING) | GNU LGPL 2+ | fixes the ROM header (logo, checksum) |
| GCC arm-none-eabi 13.2 | GPL v3 | compiler |
| Python 3, NumPy, Pillow, SciPy | PSF / BSD / HPND / BSD | tables, graphics, artwork, audio |
| pandoc, Playwright/Chromium | GPL / Apache 2.0 / BSD | PDF manual |

## Tests only

| Tool | Licence | Use |
|---|---|---|
| mGBA 0.10.2 (`libmgba`) by endrift | MPL 2.0 | headless emulator harness (`tests/harness/mgba_harness.c` links it) |
| GBDK-2020 4.3.0 / SDCC 4.4.1 | GPL with linking exception | builds ETCH's original CORROSION for the Game Boy reference test |

No Nintendo BIOS, no commercial game data and no third-party samples or artwork are used or required. The 156-byte boot logo block in the ROM header is written by gbafix, as for every GBA homebrew ROM, so that the console boots it.
