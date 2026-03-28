#!/usr/bin/env python3
"""
Create a proper N64 .z64 ROM from a raw binary.

Uses libdragon's open-source IPL3 (production, CIC-6102 signed)
for real hardware compatibility. The IPL3 initializes RDRAM and
copies the ROM payload to the entry point.

IPL3 source: https://github.com/DragonMinded/libdragon/tree/trunk/boot
"""

import struct
import sys
import os


def make_z64(bin_path, z64_path, rom_name="N64 GAME"):
    with open(bin_path, "rb") as f:
        rom_data = f.read()

    # Pad to 4-byte alignment
    while len(rom_data) % 4:
        rom_data += b'\x00'

    # Try to find libdragon's prebuilt IPL3
    ipl3_paths = [
        os.path.join(os.path.dirname(__file__), "ipl3_prod.z64"),
        "/tmp/ipl3_prod.z64",
        os.path.join(os.path.dirname(__file__), "..", "boot", "ipl3_prod.z64"),
    ]

    ipl3_rom = None
    for p in ipl3_paths:
        if os.path.exists(p) and os.path.getsize(p) > 4000:
            with open(p, "rb") as f:
                ipl3_rom = f.read()
            print(f"  Using IPL3 from: {p}")
            break

    if ipl3_rom and len(ipl3_rom) >= 0x1000:
        # The ipl3_prod.z64 is a complete ROM header (64B + 4032B IPL3 + padding)
        # Extract the header and IPL3 from it
        header = bytearray(ipl3_rom[:64])
        ipl3_code = ipl3_rom[64:0x1000]

        # Patch the header with our ROM name and entry point
        struct.pack_into(">I", header, 0x08, 0x80000400)  # Entry point

        # ROM name (20 bytes, space-padded)
        name = rom_name.encode('ascii')[:20].ljust(20)
        header[0x20:0x34] = name

    else:
        print("  WARNING: No libdragon IPL3 found, using minimal stub")
        print("  ROM may not boot on real hardware!")
        print("  Download: curl -L -o tools/ipl3_prod.z64 \\")
        print("    https://raw.githubusercontent.com/DragonMinded/libdragon/trunk/boot/bin/ipl3_prod.z64")

        # Fallback: minimal header + stub IPL3
        header = bytearray(64)
        struct.pack_into(">I", header, 0x00, 0x80371240)
        struct.pack_into(">I", header, 0x04, 0x0000000F)
        struct.pack_into(">I", header, 0x08, 0x80000400)
        struct.pack_into(">I", header, 0x0C, 0x00001444)
        name = rom_name.encode('ascii')[:20].ljust(20)
        header[0x20:0x34] = name
        header[0x38] = 0x4E  # 'N' = cartridge
        header[0x3C] = ord('E')  # USA

        # Minimal IPL3 stub
        ipl3_code = bytearray(0x1000 - 64)

    # Assemble ROM: header (64B) + IPL3 (to 0x1000) + ROM data
    rom = bytearray(header)
    rom += ipl3_code
    while len(rom) < 0x1000:
        rom += b'\x00'
    rom += rom_data

    # Pad total ROM to at least 1 MB
    min_size = 1024 * 1024
    while len(rom) < min_size:
        rom += b'\xFF'

    # The libdragon IPL3 handles CRC itself — but we should update
    # the CRC in the header for compatibility with other loaders
    # For now, the IPL3's embedded CRC validation handles this

    with open(z64_path, "wb") as f:
        f.write(rom)

    print(f"Created {z64_path}")
    print(f"  ROM data: {len(rom_data):,d} bytes ({len(rom_data)/1024:.1f} KB)")
    print(f"  Total Z64: {len(rom):,d} bytes ({len(rom)/1024:.1f} KB)")
    print(f"  Entry: 0x80000400")
    print(f"  Name: {rom_name}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.z64> [rom_name]")
        sys.exit(1)
    rom_name = sys.argv[3] if len(sys.argv) > 3 else "N64 GAME"
    make_z64(sys.argv[1], sys.argv[2], rom_name)
