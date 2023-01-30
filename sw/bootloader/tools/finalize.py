#!/usr/bin/env python3

import subprocess
import sys



if __name__ == '__main__':
    if (len(sys.argv) != 2):
        print(f'Usage: python {sys.argv[0]} file_path')
        sys.exit(1)

    ALIGN = 1024
    CHECKSUM_SIZE = 0x101000

    bin_file = sys.argv[1]

    try:
        bin_data = b''

        with open(bin_file, 'rb') as f:
            bin_data = f.read()

        pad_size = CHECKSUM_SIZE - len(bin_data)

        if (pad_size > 0):
            bin_data += b'\xFF' * pad_size
            with open(bin_file, 'wb') as f:
                f.write(bin_data)

        subprocess.run(['chksum64', bin_file])

        with open(bin_file, 'rb') as f:
            bin_data = f.read()

        bin_data = bin_data.strip(b'\xFF')
        modulo = len(bin_data) % ALIGN
        if (modulo > 0):
            bin_data += b'\xFF' * (ALIGN - modulo)

        with open(bin_file, 'wb') as f:
            f.write(bin_data)

    except FileNotFoundError as e:
        print(f'Couldn\'t open file "{bin_file}" {e}')
        sys.exit(2)

    except Exception as e:
        print(e)
        sys.exit(3)
