#!/usr/bin/env python3
import os
import sys

binary = None
rom = None

binary_name = sys.argv[1] or 'binary.bin'
rom_name = sys.argv[2] or 'rom.bin'

try:
    binary = open(binary_name, mode='rb')
    rom = open(rom_name, mode='wb')

    length = os.path.getsize(binary_name)
    rom.write(length.to_bytes(4, byteorder='little'))
    rom.write(binary.read())

except Exception as e:
    print(f'Unable to convert the rom: {e}', file=sys.stderr)
    sys.exit(-1)

finally:
    if (binary): binary.close()
    if (rom): rom.close()
