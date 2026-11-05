#!/usr/bin/env python3
"""Minimal ELF -> UF2 converter for RP2350 (replaces `picotool uf2 convert` in the build).
Usage: elf2uf2.py input.elf output.uf2 [family_id]
Flash image is taken from `objcopy -O binary` (LMA-based, starts at XIP base 0x10000000).
UF2 format: https://github.com/microsoft/uf2 (512-byte blocks, 256-byte payload)."""
import struct, subprocess, sys, tempfile, os
FLASH_BASE = 0x10000000
FAMILY_RP2350_ARM_S = 0xE48BFF59
def main():
    elf, out = sys.argv[1], sys.argv[2]
    family = int(sys.argv[3], 0) if len(sys.argv) > 3 else FAMILY_RP2350_ARM_S
    with tempfile.TemporaryDirectory() as d:
        b = os.path.join(d, "img.bin")
        subprocess.check_call(["arm-none-eabi-objcopy", "-O", "binary", elf, b])
        data = open(b, "rb").read()
    # base address = lowest LMA of loadable sections
    hdr = subprocess.check_output(["arm-none-eabi-objdump", "-h", elf], text=True).split("\n")
    lmas = []
    for i, l in enumerate(hdr):
        p = l.split()
        if len(p) >= 7 and p[0].isdigit() and i + 1 < len(hdr) and "LOAD" in hdr[i + 1] and "CONTENTS" in hdr[i + 1]:
            lmas.append(int(p[4], 16))
    base = min(lmas)
    if base != FLASH_BASE:
        sys.exit(f"unexpected image base 0x{base:08X}")
    n = (len(data) + 255) // 256
    with open(out, "wb") as f:
        for i in range(n):
            chunk = data[i * 256:(i + 1) * 256].ljust(256, b"\0")
            blk = struct.pack("<IIIIIIII", 0x0A324655, 0x9E5D5157, 0x00002000, base + i * 256, 256, i, n, family)
            blk += chunk + b"\0" * (476 - 256) + struct.pack("<I", 0x0AB16F30)
            assert len(blk) == 512
            f.write(blk)
    print(f"UF2: {n} blocks, {len(data)} bytes at 0x{base:08X}, family 0x{family:08X} -> {out}")
if __name__ == "__main__":
    main()
