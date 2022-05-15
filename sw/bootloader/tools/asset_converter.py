#!/usr/bin/env python3

import sys
from PIL import Image



if (len(sys.argv) != 3):
    print(f"Usage: python {sys.argv[0]} input_path output_path")
    exit(-1)

asset_input = sys.argv[1]
asset_output = sys.argv[2]

source_asset = None
final_asset = None

try:
    source_asset = Image.open(asset_input)
    converted_asset = source_asset.convert("RGBA")
    final_asset = open(asset_output, "wb")
    final_asset.write(converted_asset.tobytes())
except FileNotFoundError:
    print(f"Couldn't open file \"{asset_input}\"")
except Exception as e:
    print(e)
finally:
    if (source_asset):
        source_asset.close()
    if (final_asset):
        final_asset.close()
