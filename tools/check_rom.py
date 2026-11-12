#!/usr/bin/env python3
"""ETCH II -- by Synth Centric.  Verify a GBA ROM header (logo, fixed byte,
complement checksum, title, game code, maker) and print its SHA-256."""
import hashlib
import sys

# CRC32 of the 156-byte Nintendo logo block every bootable GBA ROM carries
LOGO_CRC = 0xD0BEB55E


def check(path, quiet=False):
    import zlib
    rom = open(path, "rb").read()
    hdr = rom[:0xC0]
    info = {
        "size": len(rom),
        "title": hdr[0xA0:0xAC].rstrip(b"\0").decode("ascii", "replace"),
        "game_code": hdr[0xAC:0xB0].decode("ascii", "replace"),
        "maker": hdr[0xB0:0xB2].decode("ascii", "replace"),
        "fixed_96": hdr[0xB2] == 0x96,
        "logo_ok": zlib.crc32(hdr[0x04:0xA0]) == LOGO_CRC,
        "branch_ok": hdr[3] == 0xEA,
        "sha256": hashlib.sha256(rom).hexdigest(),
    }
    chk = 0
    for b in hdr[0xA0:0xBD]:
        chk = (chk - b) & 0xFF
    chk = (chk - 0x19) & 0xFF
    info["complement"] = "%02X" % hdr[0xBD]
    info["complement_ok"] = chk == hdr[0xBD]
    ok = info["logo_ok"] and info["fixed_96"] and info["complement_ok"] and info["branch_ok"] and "SRAM_V" in rom.decode("latin1")
    info["save_type"] = "SRAM_V" if b"SRAM_V" in rom else "none"
    if not quiet:
        print("ROM %s: %s" % (path, info))
        print("ROM header OK" if ok else "ROM header BAD")
    return ok, info


if __name__ == "__main__":
    ok, _ = check(sys.argv[1])
    sys.exit(0 if ok else 1)
