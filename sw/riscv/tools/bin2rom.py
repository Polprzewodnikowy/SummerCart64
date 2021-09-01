#!/usr/bin/env python3
import os
import sys

rom = None

rom_name = sys.argv[1] or 'rom.bin'

try:
    binary_data = sys.stdin.buffer.read()
    if (os.path.exists(rom_name)):
        os.remove(rom_name)
    rom = open(rom_name, mode='wb')
    rom.write(len(binary_data).to_bytes(4, byteorder='little'))
    rom.write(binary_data)

except Exception as e:
    print(f'Unable to convert the rom: {e}', file=sys.stderr)
    sys.exit(-1)

finally:
    if (rom): rom.close()
