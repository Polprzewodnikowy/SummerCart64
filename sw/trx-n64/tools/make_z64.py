#!/usr/bin/env python3
"""
Create an N64 .z64 ROM using libdragon's CIC-6102 signed IPL3.

The libdragon IPL3 (ipl3_prod.z64) is a CIC-6102 compatible boot code
that has been bruteforce-signed to pass the CIC checksum verification.
It copies data from ROM to RDRAM and jumps to the entry point.

Key: libdragon's IPL3 does a bulk DMA of ROM 0x1000+ to entry point.
It handles PIF boot termination internally. It works with raw binary
payloads — does NOT require ELF format despite what the docs suggest
for the "dev" variant. The "prod" variant just copies and jumps.
"""

import struct
import sys
import os


def n64_crc(rom_data, seed=0xF8CA4DDC):
    """Compute N64 CRC1/CRC2 for CIC-6102."""
    data = bytearray(rom_data)
    while len(data) < 0x101000:
        data += b'\x00'

    t1 = t2 = t3 = t4 = t5 = t6 = seed

    for i in range(0x1000, 0x101000, 4):
        d = struct.unpack(">I", data[i:i+4])[0]
        if ((t6 + d) & 0xFFFFFFFF) < t6:
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

    return (t6 ^ t4 ^ t3) & 0xFFFFFFFF, (t5 ^ t2 ^ t1) & 0xFFFFFFFF


def make_z64(bin_path, z64_path, rom_name="N64 GAME"):
    with open(bin_path, "rb") as f:
        rom_data = f.read()

    while len(rom_data) % 4:
        rom_data += b'\x00'

    # Find libdragon's CIC-6102 signed IPL3
    ipl3_path = os.path.join(os.path.dirname(__file__), "ipl3_prod.z64")
    if not os.path.exists(ipl3_path) or os.path.getsize(ipl3_path) < 4000:
        print("  ERROR: tools/ipl3_prod.z64 not found!")
        print("  Download: curl -L -o tools/ipl3_prod.z64 \\")
        print("    https://raw.githubusercontent.com/DragonMinded/libdragon/trunk/boot/bin/ipl3_prod.z64")
        sys.exit(1)

    with open(ipl3_path, "rb") as f:
        ipl3_rom = f.read()

    print(f"  Using CIC-6102 signed IPL3 from: {ipl3_path}")

    # The ipl3_prod.z64 contains: 64B header + IPL3 code + padding to 0x1000
    # Use the ENTIRE first 0x1000 bytes as our ROM header+IPL3
    # (the header has the correct PI config, CIC seed, etc.)
    rom_header_and_ipl3 = bytearray(ipl3_rom[:0x1000])

    # Pad if the IPL3 file is shorter than 0x1000
    while len(rom_header_and_ipl3) < 0x1000:
        rom_header_and_ipl3 += b'\x00'

    # Patch the header fields we need to customize
    # Entry point
    struct.pack_into(">I", rom_header_and_ipl3, 0x08, 0x80000400)

    # ROM name (20 bytes at offset 0x20)
    name = rom_name.encode('ascii')[:20].ljust(20)
    rom_header_and_ipl3[0x20:0x34] = name

    # CRC placeholders (computed below)
    struct.pack_into(">I", rom_header_and_ipl3, 0x10, 0)
    struct.pack_into(">I", rom_header_and_ipl3, 0x14, 0)

    # Assemble ROM: header+IPL3 (0x1000) + raw binary payload
    rom = bytearray(rom_header_and_ipl3) + bytearray(rom_data)

    # Pad to 1 MB minimum
    while len(rom) < 1024 * 1024:
        rom += b'\x00'

    # Compute and patch CRC (uses CIC-6102 seed)
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
