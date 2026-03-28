#!/usr/bin/env python3
"""
Convert a big-endian MIPS ELF to a flat binary image.
Replacement for `objcopy -O binary` when binutils aren't installed.

Reads LOAD segments from the ELF, places them at their correct
virtual addresses relative to the lowest load address, outputs
a flat binary starting from the lowest address.
"""

import struct
import sys


def elf2bin(elf_path, bin_path):
    with open(elf_path, "rb") as f:
        elf = f.read()

    # Parse ELF header (big-endian 32-bit)
    if elf[:4] != b'\x7fELF':
        print(f"Error: {elf_path} is not an ELF file")
        sys.exit(1)

    ei_class = elf[4]  # 1=32-bit, 2=64-bit
    ei_data = elf[5]   # 1=LE, 2=BE
    if ei_data != 2:
        print(f"Error: expected big-endian ELF")
        sys.exit(1)

    e_entry = struct.unpack(">I", elf[0x18:0x1C])[0]
    e_phoff = struct.unpack(">I", elf[0x1C:0x20])[0]
    e_phentsize = struct.unpack(">H", elf[0x2A:0x2C])[0]
    e_phnum = struct.unpack(">H", elf[0x2C:0x2E])[0]

    # Collect LOAD segments
    segments = []
    for i in range(e_phnum):
        ph = e_phoff + i * e_phentsize
        p_type = struct.unpack(">I", elf[ph:ph+4])[0]
        if p_type != 1:  # PT_LOAD
            continue
        p_offset = struct.unpack(">I", elf[ph+4:ph+8])[0]
        p_vaddr = struct.unpack(">I", elf[ph+8:ph+12])[0]
        p_filesz = struct.unpack(">I", elf[ph+16:ph+20])[0]
        p_memsz = struct.unpack(">I", elf[ph+20:ph+24])[0]
        segments.append((p_vaddr, p_offset, p_filesz, p_memsz))

    if not segments:
        print("Error: no LOAD segments found")
        sys.exit(1)

    # Find base address (lowest vaddr)
    base = min(s[0] for s in segments)
    # Find total file-backed size (exclude BSS — filesz only, not memsz)
    end = max(s[0] + s[2] for s in segments)
    total = end - base

    # Skip empty trailing segments (BSS has filesz=0)
    # Only include segments with actual data
    data_segments = [(v, o, f, m) for v, o, f, m in segments if f > 0]
    if data_segments:
        end = max(s[0] + s[2] for s in data_segments)
        total = end - base

    # Build flat binary
    binary = bytearray(total)
    for vaddr, offset, filesz, memsz in segments:
        dest = vaddr - base
        binary[dest:dest+filesz] = elf[offset:offset+filesz]

    with open(bin_path, "wb") as f:
        f.write(binary)

    print(f"  ELF entry: 0x{e_entry:08X}, base: 0x{base:08X}")
    print(f"  {len(segments)} LOAD segments, {total:,d} bytes flat binary")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.elf> <output.bin>")
        sys.exit(1)
    elf2bin(sys.argv[1], sys.argv[2])
