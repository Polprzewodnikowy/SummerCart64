#!/usr/bin/env python3
import struct
import sys

sv_template = None
sv_code = None

template_name = sys.argv[1] or 'template.sv'
code_name = sys.argv[2] or 'result.sv'

try:
    sv_template = open(template_name, mode='r')
    sv_code = open(code_name, mode='w')

    var_name = sv_template.readline().strip()

    rom_formatted = ''
    index = 0
    for line in iter(lambda: sys.stdin.buffer.read(4), ''):
        if (not line):
            break
        value = format(struct.unpack('<I', line)[0], '08x')
        rom_formatted += f'\n                {index}: {var_name} = 32\'h{value};'
        index += 1

    sv_code.write(sv_template.read().format(rom_formatted=rom_formatted))

except Exception as e:
    print(f'Unable to convert the code: {e}', file=sys.stderr)
    sys.exit(-1)

finally:
    if (sv_template): sv_template.close()
    if (sv_code): sv_code.close()
