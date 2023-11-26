#!/usr/bin/env python3

import sys



if __name__ == '__main__':
    if (len(sys.argv) != 3):
        print(f'Usage: python {sys.argv[0]} in_file out_file')
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        output = ''
        while True:
            file_bytes = f.read(4)
            if len(file_bytes) != 4:
                break
            output += f"{int.from_bytes(file_bytes, 'little'):08X}\n"

    with open(sys.argv[2], 'w') as f:
        f.write(output)
