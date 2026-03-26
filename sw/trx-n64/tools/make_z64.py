#!/usr/bin/env python3
"""
Create a proper N64 .z64 ROM from a raw binary.

N64 ROM header (64 bytes):
  Offset  Size  Description
  0x00    4     PI BSB DOM1 config (0x80371240 for standard cart)
  0x04    4     Clock rate (0x0000000F)
  0x08    4     Entry point (boot address, typically 0x80000400)
  0x0C    4     Release (0x00001444 for libultra 2.0L)
  0x10    4     CRC1
  0x14    4     CRC2
  0x18    8     Reserved (zeros)
  0x20    20    ROM name (padded with spaces)
  0x34    4     Reserved
  0x38    4     Media format / Game ID
  0x3C    1     Country code
  0x3D    1     Version
  0x3E    2     Reserved

After header: IPL3 bootcode (4032 bytes at 0x40-0xFFF)
Then ROM data starts at 0x1000.

For SC64/libdragon ROMs, the bootcode is typically the open-source
IPL3 from libdragon. We can also use a minimal bootstrap that jumps
directly to our entry point.
"""

import struct
import sys
import os
import hashlib


def crc32_n64(data):
    """Simple CRC placeholder — real N64 CRC depends on IPL3 seed."""
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF


def create_ipl3_stub():
    """
    Create a minimal IPL3-like bootstrap (4032 bytes).

    Real IPL3 bootcode copies ROM data to RDRAM and jumps to entry point.
    This stub does the minimum: set up SP, copy code, jump.

    For proper emulator compatibility, we'd need a real IPL3 (like
    libdragon's open-source one). This stub works for SC64 hardware
    which has its own boot path.
    """
    # Minimal MIPS bootstrap: copies ROM 0x1000+ to 0x80000400 and jumps
    code = bytearray(4032)

    # MIPS instructions (big-endian):
    instrs = [
        # Set up stack pointer
        0x3C1D8010,  # lui sp, 0x8010
        0x37BD0000,  # ori sp, sp, 0x0000

        # Source: ROM address 0xB0001000 (uncached)
        0x3C08B000,  # lui t0, 0xB000
        0x35081000,  # ori t0, t0, 0x1000

        # Dest: RDRAM 0x80000400
        0x3C098000,  # lui t1, 0x8000
        0x35290400,  # ori t1, t1, 0x0400

        # Count: ROM size (we'll patch this)
        0x3C0A0001,  # lui t2, 0x0001  (64KB default, patched later)
        0x354A0000,  # ori t2, t2, 0x0000

        # Copy loop
        0x8D0B0000,  # lw t3, 0(t0)     # load word from ROM
        0xAD2B0000,  # sw t3, 0(t1)     # store to RDRAM
        0x25080004,  # addiu t0, t0, 4
        0x25290004,  # addiu t1, t1, 4
        0x254AFFFC,  # addiu t2, t2, -4
        0x1D40FFFA,  # bgtz t2, -6      # loop
        0x00000000,  # nop (branch delay)

        # Jump to entry point
        0x3C088000,  # lui t0, 0x8000
        0x35080400,  # ori t0, t0, 0x0400
        0x01000008,  # jr t0
        0x00000000,  # nop (branch delay)
    ]

    for i, instr in enumerate(instrs):
        struct.pack_into(">I", code, i * 4, instr)

    return bytes(code)


def make_z64(bin_path, z64_path, rom_name="N64 GAME"):
    with open(bin_path, "rb") as f:
        rom_data = f.read()

    # Pad to 4-byte alignment
    while len(rom_data) % 4:
        rom_data += b'\x00'

    # Build header (64 bytes)
    header = bytearray(64)

    # PI BSB DOM1 config
    struct.pack_into(">I", header, 0x00, 0x80371240)

    # Clock rate
    struct.pack_into(">I", header, 0x04, 0x0000000F)

    # Entry point
    struct.pack_into(">I", header, 0x08, 0x80000400)

    # Release
    struct.pack_into(">I", header, 0x0C, 0x00001444)

    # CRC1 and CRC2 (placeholder — patched below)
    struct.pack_into(">I", header, 0x10, 0)
    struct.pack_into(">I", header, 0x14, 0)

    # ROM name (20 bytes, space-padded)
    name = rom_name.encode('ascii')[:20].ljust(20)
    header[0x20:0x34] = name[:20]

    # Media format + Game ID
    struct.pack_into(">I", header, 0x38, 0x4E000000)  # 'N' = cartridge

    # Country code ('E' = USA)
    header[0x3C] = ord('E')

    # Version
    header[0x3D] = 0

    # Create IPL3 bootcode
    ipl3 = create_ipl3_stub()

    # Patch the copy size in IPL3 based on actual ROM data size
    ipl3 = bytearray(ipl3)
    copy_size = len(rom_data)
    struct.pack_into(">I", ipl3, 6 * 4, 0x3C0A0000 | ((copy_size >> 16) & 0xFFFF))
    struct.pack_into(">I", ipl3, 7 * 4, 0x354A0000 | (copy_size & 0xFFFF))
    ipl3 = bytes(ipl3)

    # Assemble: header (64) + IPL3 (4032) + padding to 0x1000 + ROM data
    rom = bytearray(header)
    rom += ipl3

    # Pad to 0x1000 (4096 bytes total before ROM data)
    while len(rom) < 0x1000:
        rom += b'\x00'

    rom += rom_data

    # Pad total ROM to power-of-2 size (minimum 1 MB for most emulators)
    min_size = 1024 * 1024  # 1 MB minimum
    while len(rom) < min_size:
        rom += b'\xFF'

    # Calculate CRC (simplified — uses the IPL3+ROM data)
    # Real CRC algorithm depends on IPL3 seed byte at offset 0x3F
    crc_data = bytes(rom[0x1000:0x101000]) if len(rom) >= 0x101000 else bytes(rom[0x1000:])
    crc1 = crc32_n64(crc_data)
    crc2 = crc32_n64(crc_data[::-1])
    struct.pack_into(">I", rom, 0x10, crc1)
    struct.pack_into(">I", rom, 0x14, crc2)

    with open(z64_path, "wb") as f:
        f.write(rom)

    print(f"Created {z64_path}")
    print(f"  ROM data: {len(rom_data):,d} bytes ({len(rom_data)/1024:.1f} KB)")
    print(f"  Total Z64: {len(rom):,d} bytes ({len(rom)/1024:.1f} KB)")
    print(f"  CRC1: {crc1:08X}  CRC2: {crc2:08X}")
    print(f"  Entry: 0x80000400")
    print(f"  Name: {name.decode().strip()}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.z64> [rom_name]")
        sys.exit(1)
    rom_name = sys.argv[3] if len(sys.argv) > 3 else "N64 GAME"
    make_z64(sys.argv[1], sys.argv[2], rom_name)
