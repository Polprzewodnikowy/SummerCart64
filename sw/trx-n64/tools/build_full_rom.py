#!/usr/bin/env python3
"""
Build a combined ROM with assets embedded.

Layout:
  0x000000 - 0x000FFF   Header + IPL3 (from trx-n64.z64)
  0x001000 - 0x0FFFFF   Game code (padded to 1 MB)
  0x100000 - end         Asset archive (tr1_assets.bin)

The game reads assets via PI DMA from ROM offset 0x100000.
"""

import sys
import os

def build_full_rom(rom_path, assets_path, output_path):
    with open(rom_path, "rb") as f:
        rom = f.read()

    with open(assets_path, "rb") as f:
        assets = f.read()

    # Pad ROM to exactly 1 MB (0x100000)
    ASSET_OFFSET = 0x100000
    if len(rom) < ASSET_OFFSET:
        rom = rom + b'\x00' * (ASSET_OFFSET - len(rom))
    elif len(rom) > ASSET_OFFSET:
        print(f"WARNING: ROM is {len(rom)} bytes, larger than {ASSET_OFFSET}!")
        print(f"  Assets will overlap with code!")

    # Combine
    full = bytearray(rom[:ASSET_OFFSET]) + bytearray(assets)

    # Pad to 4-byte alignment
    while len(full) % 4:
        full += b'\x00'

    with open(output_path, "wb") as f:
        f.write(full)

    print(f"Combined ROM: {output_path}")
    print(f"  Code:   {ASSET_OFFSET/1024:.0f} KB (padded)")
    print(f"  Assets: {len(assets)/1024/1024:.1f} MB at offset 0x{ASSET_OFFSET:X}")
    print(f"  Total:  {len(full)/1024/1024:.1f} MB")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <rom.z64> <assets.bin> <output.z64>")
        sys.exit(1)
    build_full_rom(sys.argv[1], sys.argv[2], sys.argv[3])
