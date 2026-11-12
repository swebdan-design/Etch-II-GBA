"""ETCH II -- by Synth Centric.  Python driver for the headless mGBA harness."""
import os
import subprocess
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
HARNESS = os.path.join(ROOT, "build", "mgba_harness")

KEYS = {"a": 1, "b": 2, "select": 4, "start": 8, "right": 16, "left": 32,
        "up": 64, "down": 128, "r": 256, "l": 512}


def load_symbols(path):
    """Symbols from a GBDK .noi file or from `arm-none-eabi-nm` of an .elf."""
    syms = {}
    if path.endswith(".noi"):
        for line in open(path):
            p = line.split()
            if len(p) == 3 and p[0] == "DEF":
                syms[p[1].lstrip("_")] = int(p[2], 16)
    else:
        out = subprocess.run(["arm-none-eabi-nm", path], capture_output=True, text=True).stdout
        for line in out.splitlines():
            p = line.split()
            if len(p) == 3:
                syms[p[2]] = int(p[0], 16)
    return syms


class Emu:
    def __init__(self, rom, symfile=None, save=None):
        args = [HARNESS, rom] + ([save] if save else [])
        self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.syms = load_symbols(symfile) if symfile else {}
        self.held = 0

    def cmd(self, s):
        self.p.stdin.write(s + "\n")
        self.p.stdin.flush()
        r = self.p.stdout.readline().strip()
        if r.startswith("err"):
            raise RuntimeError(r)
        return r

    def run(self, frames=1):
        return int(self.cmd("run %d" % frames).split()[1])

    def sym(self, name):
        return self.syms[name]

    def peek(self, addr, n=1):
        if isinstance(addr, str):
            addr = self.sym(addr)
        return bytes.fromhex(self.cmd("peek %x %d" % (addr, n)))

    def u8(self, addr):
        return self.peek(addr, 1)[0]

    def u16(self, addr):
        return struct.unpack("<H", self.peek(addr, 2))[0]

    def u32(self, addr):
        return struct.unpack("<I", self.peek(addr, 4))[0]

    def poke(self, addr, data):
        if isinstance(addr, str):
            addr = self.sym(addr)
        if isinstance(data, int):
            data = bytes([data])
        for i in range(0, len(data), 512):
            self.cmd("poke %x %s" % (addr + i, data[i:i + 512].hex()))

    def keys(self, mask):
        self.held = mask
        self.cmd("keys %d" % mask)

    def hold(self, name):
        self.keys(self.held | KEYS[name])

    def release(self, name):
        self.keys(self.held & ~KEYS[name])

    def press(self, name, frames=3, after=3):
        self.hold(name)
        self.run(frames)
        self.release(name)
        self.run(after)

    def combo(self, mod, name, frames=3, after=4):
        self.hold(mod)
        self.run(2)
        self.press(name, frames, 2)
        self.release(mod)
        self.run(after)

    def shot(self, path):
        ppm = path + ".ppm" if not path.endswith(".ppm") else path
        self.cmd("shot " + ppm)
        if path.endswith(".png"):
            from PIL import Image
            Image.open(ppm).save(path)
            os.remove(ppm)
        return path

    def wav_start(self, path):
        self.cmd("wav " + path)

    def wav_stop(self):
        return int(self.cmd("wavstop").split()[1])

    def savebat(self, path):
        return int(self.cmd("savebat " + path).split()[1])

    def close(self):
        try:
            self.p.stdin.write("quit\n")
            self.p.stdin.flush()
            self.p.wait(5)
        except Exception:
            self.p.kill()
