#!/usr/bin/env python3

import sys



if __name__ == "__main__":
    if (len(sys.argv) != 2):
        print(f"Usage: python {sys.argv[0]} file_path")
        sys.exit(1)

    file = sys.argv[1]

    try:
        data = b''
        with open(file, 'rb') as f:
            data = f.read()
        with open(file, 'wb') as f:
            f.write(data.strip(b'\xFF'))
    except FileNotFoundError:
        print(f"Couldn't open file \"{file}\"")
        sys.exit(2)
    except Exception as e:
        print(e)
        sys.exit(3)
