#!/usr/bin/env python3

import sys
from PIL import Image



def compress(data: bytes, step_size: int=4) -> bytes:
    uncompressed_length = len(data)

    if ((uncompressed_length % step_size) != 0):
        raise ValueError(f'Data length not aligned to {step_size}')

    compressed_data = bytes()
    compressed_data += uncompressed_length.to_bytes(4, byteorder='big')

    count = 0
    last_value = b''

    for offset in range(0, uncompressed_length + step_size, step_size):
        next_value = data[offset:(offset + step_size)]

        if (offset != 0):
            if ((next_value == last_value) and (count < 255)):
                count += 1
            else:
                compressed_data += count.to_bytes(1, byteorder='big')
                compressed_data += last_value
                count = 0

        last_value = next_value

    return compressed_data


if __name__ == "__main__":
    if (len(sys.argv) < 3):
        print(f"Usage: python {sys.argv[0]} input_path output_path [--compress]")
        sys.exit(1)

    asset_input = sys.argv[1]
    asset_output = sys.argv[2]
    asset_compress = len(sys.argv) > 3 and (sys.argv[3] == '--compress')

    source_asset = None
    final_asset = None

    try:
        source_asset = Image.open(asset_input)
        converted_asset = source_asset.convert("RGBA").tobytes()
        if (asset_compress):
            converted_asset = compress(converted_asset)
        final_asset = open(asset_output, "wb")
        final_asset.write(converted_asset)
    except FileNotFoundError:
        print(f"Couldn't open file \"{asset_input}\"")
        sys.exit(2)
    except Exception as e:
        print(e)
        sys.exit(3)
    finally:
        if (source_asset):
            source_asset.close()
        if (final_asset):
            final_asset.close()
