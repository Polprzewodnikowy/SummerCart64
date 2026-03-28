#!/usr/bin/env python3
"""
Create an N64 .z64 ROM with classic CIC-6102 boot sequence.

The classic N64 boot (used by every commercial game):
  1. PIF sends CIC challenge/response
  2. IPL3 copies 1 MB from ROM offset 0x1000 to RDRAM at entry point
  3. Jumps to entry point

We embed a minimal IPL3 that does step 2+3 and sends the PIF
boot termination command. This is the simplest possible boot
that works on both emulators and real hardware.

The key insight: the IPL3 must write 0x08 to PIF RAM offset 0x3C
to signal "boot complete" — without this, PIF halts the CPU.
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


def build_ipl3(rom_size, entry_point=0x80000400):
    """
    Build a minimal IPL3 boot code (4032 bytes, runs from DMEM).

    This IPL3:
      1. Initializes SP
      2. DMA copies ROM data (offset 0x1000) to RDRAM entry point
      3. Sends PIF boot termination command (CRITICAL for real hw + ares)
      4. Jumps to entry point

    All instructions are big-endian MIPS.
    """
    code = bytearray(4032)
    pc = 0

    def emit(instr):
        nonlocal pc
        struct.pack_into(">I", code, pc, instr)
        pc += 4

    # Amount to copy (cap at 1 MB)
    copy_size = min(rom_size, 0x100000)
    if copy_size < 0x1000:
        copy_size = 0x1000

    # lui sp, 0x8010  (stack at top of 1 MB)
    emit(0x3C1D8010)
    # addiu sp, sp, -0x10
    emit(0x27BDFFF0)

    # ===== PI DMA: copy ROM 0x10001000 -> RDRAM entry_point =====
    # Wait for PI to be ready
    # lui t0, 0xA460 (PI_BASE)
    emit(0x3C08A460)

    # _pi_wait: lw t1, 0x10(t0)  # PI_STATUS
    emit(0x8D090010)
    # andi t1, t1, 3
    emit(0x31290003)
    # bnez t1, _pi_wait
    emit(0x1520FFFD)
    # nop
    emit(0x00000000)

    # Set RDRAM address (PI_DRAM_ADDR = entry_point & 0x1FFFFFFF)
    dram_addr = entry_point & 0x1FFFFFFF
    # lui t1, high(dram_addr)
    emit(0x3C090000 | ((dram_addr >> 16) & 0xFFFF))
    # ori t1, t1, low(dram_addr)
    emit(0x35290000 | (dram_addr & 0xFFFF))
    # sw t1, 0(t0)  # PI_DRAM_ADDR
    emit(0xAD090000)

    # Set cart address (PI_CART_ADDR = 0x10001000 for ROM offset 0x1000)
    cart_addr = 0x10001000
    # lui t1, high(cart_addr)
    emit(0x3C090000 | ((cart_addr >> 16) & 0xFFFF))
    # ori t1, t1, low(cart_addr)
    emit(0x35290000 | (cart_addr & 0xFFFF))
    # sw t1, 4(t0)  # PI_CART_ADDR
    emit(0xAD090004)

    # Set transfer length (PI_WR_LEN = copy_size - 1)
    xfer_len = copy_size - 1
    # lui t1, high(xfer_len)
    emit(0x3C090000 | ((xfer_len >> 16) & 0xFFFF))
    # ori t1, t1, low(xfer_len)
    emit(0x35290000 | (xfer_len & 0xFFFF))
    # sw t1, 0xC(t0)  # PI_WR_LEN (starts the DMA)
    emit(0xAD09000C)

    # Wait for DMA to complete
    # _dma_wait: lw t1, 0x10(t0)
    emit(0x8D090010)
    # andi t1, t1, 1  (DMA busy)
    emit(0x31290001)
    # bnez t1, _dma_wait
    emit(0x1520FFFD)
    # nop
    emit(0x00000000)

    # ===== Send PIF boot termination command =====
    # This tells the PIF that boot is complete.
    # Without this, PIF halts the CPU after ~5 seconds.
    #
    # Write 0x08 to PIF RAM last byte (0xBFC007FC)
    # lui t0, 0xBFC0
    emit(0x3C08BFC0)
    # ori t1, zero, 0x08
    emit(0x34090008)
    # sw t1, 0x07FC(t0)  # PIF_RAM + 0x3C
    emit(0xAD0907FC)

    # ===== Jump to entry point =====
    # lui t0, high(entry_point)
    emit(0x3C080000 | ((entry_point >> 16) & 0xFFFF))
    # ori t0, t0, low(entry_point)
    emit(0x35080000 | (entry_point & 0xFFFF))
    # jr t0
    emit(0x01000008)
    # nop
    emit(0x00000000)

    return bytes(code)


def make_z64(bin_path, z64_path, rom_name="N64 GAME"):
    with open(bin_path, "rb") as f:
        rom_data = f.read()

    while len(rom_data) % 4:
        rom_data += b'\x00'

    # Build header
    header = bytearray(64)
    struct.pack_into(">I", header, 0x00, 0x80371240)  # PI config
    struct.pack_into(">I", header, 0x04, 0x0000000F)  # Clock rate
    struct.pack_into(">I", header, 0x08, 0x80000400)  # Entry point
    struct.pack_into(">I", header, 0x0C, 0x00001444)  # Release (libultra 2.0L)

    # ROM name
    name = rom_name.encode('ascii')[:20].ljust(20)
    header[0x20:0x34] = name

    # Media format / country
    header[0x38] = 0x4E  # 'N' = cartridge
    header[0x3C] = ord('E')  # USA
    header[0x3F] = 0x3F  # CIC-6102 seed byte

    # Build IPL3
    ipl3 = build_ipl3(len(rom_data))

    # Assemble ROM
    rom = bytearray(header) + bytearray(ipl3)
    while len(rom) < 0x1000:
        rom += b'\x00'
    rom += bytearray(rom_data)

    # Pad to 1 MB minimum
    while len(rom) < 1024 * 1024:
        rom += b'\x00'

    # Compute CRC
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
    print(f"  IPL3: Custom (PI DMA + PIF termination)")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.z64> [rom_name]")
        sys.exit(1)
    rom_name = sys.argv[3] if len(sys.argv) > 3 else "N64 GAME"
    make_z64(sys.argv[1], sys.argv[2], rom_name)
