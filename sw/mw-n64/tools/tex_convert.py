#!/usr/bin/env python3
"""
MW-N64: Texture pre-converter

Converts Morrowind textures (DDS, BMP, TGA) to N64-ready CI8 format.
Output: 128×128 8-bit indexed with 256-color RGBA5551 palette.

The N64's 4 KB TMEM limits us to 64×64 at CI8 per tile load.
We store at 128×128 and the terrain renderer loads 64×64 sub-tiles.

Usage:
    python3 tex_convert.py <input_dir> <output_dir>
"""

import os
import sys
import struct

try:
    from PIL import Image
except ImportError:
    print("WARNING: Pillow not installed. Texture conversion disabled.")
    print("Install with: pip install Pillow")
    sys.exit(0)


def rgb_to_rgba5551(r, g, b, a=255):
    """Convert 8-bit RGBA to N64 RGBA5551."""
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    a1 = 1 if a > 127 else 0
    return (r5 << 11) | (g5 << 6) | (b5 << 1) | a1


def quantize_to_ci8(img, target_size=(128, 128)):
    """Quantize an image to 256-color CI8 format."""
    # Resize to target
    img = img.resize(target_size, Image.LANCZOS)

    # Convert to RGBA
    if img.mode != 'RGBA':
        img = img.convert('RGBA')

    # Quantize to 256 colors using PIL's built-in quantizer
    img_rgb = img.convert('RGB')
    img_quant = img_rgb.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    palette = img_quant.getpalette()  # Flat list: [R,G,B,R,G,B,...]

    # Build CI8 pixel data
    pixels = list(img_quant.getdata())
    ci8_data = bytes(pixels)

    # Build RGBA5551 palette
    palette_5551 = []
    for i in range(256):
        if palette and i * 3 + 2 < len(palette):
            r, g, b = palette[i*3], palette[i*3+1], palette[i*3+2]
        else:
            r, g, b = 0, 0, 0
        palette_5551.append(rgb_to_rgba5551(r, g, b))

    return ci8_data, palette_5551


def generate_mipmaps(img, base_size=128):
    """
    Generate mipmap chain (Lambert LOD technique).
    Returns list of (size, ci8_data, palette) from highest to lowest detail.
    Sizes: 128, 64, 32, 16, 8 — each half the previous.
    Lowest 2 LODs (16, 8) always resident in RDRAM.
    """
    mips = []
    size = base_size
    while size >= 8:
        ci8_data, palette = quantize_to_ci8(img, (size, size))
        mips.append((size, ci8_data, palette))
        size //= 2
    return mips


def convert_texture(inpath, outpath, target_size=(128, 128)):
    """Convert a single texture file to CI8 binary with mipmap chain."""
    try:
        img = Image.open(inpath)
    except Exception as e:
        print(f"  SKIP {os.path.basename(inpath)}: {e}")
        return False

    mips = generate_mipmaps(img, target_size[0])

    # Write binary with mipmap chain:
    # [4B magic "MIP8"][4B mip_count][4B base_width][4B base_height]
    # For each mip level:
    #   [4B width][4B height][512B palette][W×H bytes pixels]
    with open(outpath, 'wb') as f:
        f.write(b'MIP8')
        f.write(struct.pack('<I', len(mips)))
        f.write(struct.pack('<II', target_size[0], target_size[1]))

        for size, ci8_data, palette in mips:
            f.write(struct.pack('<II', size, size))
            for c in palette:
                f.write(struct.pack('<H', c))
            f.write(ci8_data)

    return True


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input_dir> <output_dir>")
        sys.exit(1)

    in_dir = sys.argv[1]
    out_dir = sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    extensions = ('.dds', '.bmp', '.tga', '.png', '.jpg', '.jpeg')
    count = 0

    for root, dirs, files in os.walk(in_dir):
        for fn in files:
            if fn.lower().endswith(extensions):
                inpath = os.path.join(root, fn)
                rel = os.path.relpath(inpath, in_dir)
                outname = rel.rsplit('.', 1)[0] + '.ci8'
                outpath = os.path.join(out_dir, outname)
                os.makedirs(os.path.dirname(outpath), exist_ok=True)

                if convert_texture(inpath, outpath):
                    count += 1

    print(f"\nConverted {count} textures to CI8 format in {out_dir}/")


if __name__ == "__main__":
    main()
