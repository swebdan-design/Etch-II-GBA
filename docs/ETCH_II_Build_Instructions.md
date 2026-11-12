# ETCH II — Build Instructions

**ETCH II — A Generative Synthesis Machine** · by Synth Centric

## Requirements

| Tool | Used for | Tested version |
|---|---|---|
| An `arm-none-eabi` GCC with newlib: **devkitARM** (set `DEVKITARM`) or the distribution package (Ubuntu/Debian: `gcc-arm-none-eabi libnewlib-arm-none-eabi`) | the ROM | GCC 13.2.1 (Ubuntu 24.04 package) |
| A host C compiler (`cc`) | gbafix, the album composer, host tests | GCC 13 |
| Python 3 with NumPy and Pillow | tables, graphics, artwork, tests | Python 3.13, NumPy 2.5, Pillow 12 |
| *Tests only*: `libmgba-dev` (mGBA 0.10) | the headless emulator harness | 0.10.2 |
| *Compatibility certification only*: GBDK-2020 4.3.0 (`GBDK_HOME`) and the ETCH source (`ETCH_REPO`) | building ETCH's original CORROSION with ETCH's compiler | 4.3.0 / SDCC 4.4.1 |
| *Release only*: SciPy (audio resampling), ReportLab (PDF) | previews, manual PDF | |

The startup code and linker script (`src/main/platform/gba_crt0.s`, `gba_cart.ld`) are devkitPro's, included unmodified, so devkitARM is not required. `gbafix` is built from devkitPro's source in `tools/third_party/gbafix`.

On Ubuntu 24.04:

```bash
sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi libmgba-dev python3-numpy python3-pil
```

## Build the ROM

```bash
make
```

Output: `build/ETCH_II_by_Synth_Centric.gba` (plus `.elf` and `.map`). The build generates `build/gen/tables.c`, `gfx.c` and `album.c`, fixes the header with gbafix and checks it with `tools/check_rom.py`.

Mixer rate is a build option (samples per frame and per block):

```bash
make clean
```

```bash
make SPF=304 BLOCK=16
```

`264/24` (15 768 Hz) is the release setting; `304/16` (18 157 Hz) and `224/16` (13 379 Hz) also build — see the Sound Engine notes for why 15 768 Hz was chosen.

## Tests

```bash
make hosttest
```

```bash
make compat ETCH_REPO=/path/to/Etch---GB GBDK_HOME=/opt/gbdk
```

```bash
make emutest
```

`make test` runs all three. `make compat` also needs ETCH rebuilt from source next to the ETCH repository (`../etch-rebuild`) for the end-to-end check against the released ROM's symbol map; without it that check reports FAIL with an explanation rather than passing silently.

## Release package

```bash
make release
```

Builds the ROM, records audio previews from it in the emulator, renders the artwork, writes the PDF manual, copies the documentation, creates `release/ETCH_II_source.zip` and writes `release/SHA256SUMS.txt`.

## Running

Any accurate GBA emulator (mGBA recommended) or a flash cartridge. Set the cartridge's save type to **SRAM** (32 KB) if it doesn't detect it from the ROM's `SRAM_V113` marker.
