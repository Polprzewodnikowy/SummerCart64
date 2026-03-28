#!/usr/bin/env python3
"""
Create a proper N64 .z64 ROM with valid CRC checksums.

Uses libdragon's open-source IPL3 (CIC-6102 signed).
Computes correct CRC1/CRC2 using the standard N64 checksum algorithm
so the boot chip validates the ROM and allows execution.

CRC algorithm: https://n64brew.dev/wiki/ROM_Header#Checksum
"""

import struct
import sys
import os


def n64_crc(rom_data):
    """
    Compute N64 ROM CRC1 and CRC2.

    The CRC is computed over bytes 0x1000 to 0x101000 (the first 1MB
    after the IPL3). The algorithm depends on the CIC seed byte at
    ROM offset 0x3F.

    For CIC-6102 (most common, used by libdragon IPL3):
      seed = 0xF8CA4DDC
    """
    CIC_6102_SEED = 0xF8CA4DDC

    # Ensure we have at least 1MB + 0x1000 of data
    data = rom_data
    while len(data) < 0x101000:
        data += b'\x00'

    t1 = t2 = t3 = t4 = t5 = t6 = CIC_6102_SEED

    for i in range(0x1000, 0x101000, 4):
        d = struct.unpack(">I", data[i:i+4])[0]

        if (t6 + d) & 0xFFFFFFFF < t6:
            t4 = (t4 + 1) & 0xFFFFFFFF

        t6 = (t6 + d) & 0xFFFFFFFF
        t3 ^= d
        shift = d & 0x1F
        r = ((d << shift) | (d >> (32 - shift))) & 0xFFFFFFFF
        t5 = (t5 + r) & 0xFFFFFFFF

        if t2 > d:
            t2 ^= r
        else:
            t2 ^= t6 ^ d

        t1 = (t1 + (d ^ t5)) & 0xFFFFFFFF

    crc1 = (t6 ^ t4 ^ t3) & 0xFFFFFFFF
    crc2 = (t5 ^ t2 ^ t1) & 0xFFFFFFFF
    return crc1, crc2


def make_z64(bin_path, z64_path, rom_name="N64 GAME"):
    with open(bin_path, "rb") as f:
        rom_data = f.read()

    # Pad to 4-byte alignment
    while len(rom_data) % 4:
        rom_data += b'\x00'

    # Find libdragon's prebuilt IPL3
    ipl3_paths = [
        os.path.join(os.path.dirname(__file__), "ipl3_prod.z64"),
        "/tmp/ipl3_prod.z64",
    ]

    ipl3_rom = None
    for p in ipl3_paths:
        if os.path.exists(p) and os.path.getsize(p) > 4000:
            with open(p, "rb") as f:
                ipl3_rom = f.read()
            print(f"  Using IPL3 from: {p}")
            break

    if not ipl3_rom or len(ipl3_rom) < 0x1000:
        print("  ERROR: No valid IPL3 found!")
        print("  Download: curl -L -o tools/ipl3_prod.z64 \\")
        print("    https://raw.githubusercontent.com/DragonMinded/libdragon/trunk/boot/bin/ipl3_prod.z64")
        sys.exit(1)

    # Use IPL3's header as base (has correct PI config, CIC seed, etc.)
    header = bytearray(ipl3_rom[:64])

    # Patch entry point
    struct.pack_into(">I", header, 0x08, 0x80000400)

    # Patch ROM name
    name = rom_name.encode('ascii')[:20].ljust(20)
    header[0x20:0x34] = name

    # CRC placeholders (will compute below)
    struct.pack_into(">I", header, 0x10, 0)
    struct.pack_into(">I", header, 0x14, 0)

    # Extract IPL3 boot code (bytes 64 to 0x1000)
    ipl3_code = ipl3_rom[64:0x1000]
    # Pad if needed
    while len(ipl3_code) < (0x1000 - 64):
        ipl3_code += b'\x00'

    # Assemble ROM: header (64B) + IPL3 (to 0x1000) + ROM data
    rom = bytearray(header) + bytearray(ipl3_code) + bytearray(rom_data)

    # Pad to at least 1 MB
    while len(rom) < 1024 * 1024:
        rom += b'\xFF'

    # Compute and patch CRC
    crc1, crc2 = n64_crc(bytes(rom))
    struct.pack_into(">I", rom, 0x10, crc1)
    struct.pack_into(">I", rom, 0x14, crc2)

    with open(z64_path, "wb") as f:
        f.write(rom)

    print(f"Created {z64_path}")
    print(f"  ROM data: {len(rom_data):,d} bytes ({len(rom_data)/1024:.1f} KB)")
    print(f"  Total Z64: {len(rom):,d} bytes ({len(rom)/1024:.1f} KB)")
    print(f"  CRC1: {crc1:08X}  CRC2: {crc2:08X}")
    print(f"  Entry: 0x80000400")
    print(f"  Name: {rom_name}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.z64> [rom_name]")
        sys.exit(1)
    rom_name = sys.argv[3] if len(sys.argv) > 3 else "N64 GAME"
    make_z64(sys.argv[1], sys.argv[2], rom_name)
