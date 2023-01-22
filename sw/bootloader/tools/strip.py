#!/usr/bin/env python3

import sys



if __name__ == "__main__":
    if (len(sys.argv) != 3):
        print(f"Usage: python {sys.argv[0]} file_path align")
        sys.exit(1)

    file = sys.argv[1]
    align = int(sys.argv[2], base=0)

    try:
        data = b''
        with open(file, 'rb') as f:
            data = f.read().strip(b'\xFF')
        with open(file, 'wb') as f:
            modulo = (len(data) % align) if (align > 0) else 0
            data += b'\xFF' * ((align - modulo) if (modulo != 0) else 0)
            f.write(data)
    except FileNotFoundError:
        print(f"Couldn't open file \"{file}\"")
        sys.exit(2)
    except Exception as e:
        print(e)
        sys.exit(3)
