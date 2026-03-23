#!/usr/bin/env python3
"""
Generate test assets for the debug ROM.

Creates 24 assets of varying sizes with recognizable patterns,
packs them into an archive, and generates an asset_ids.h header.

Asset design:
  - Assets 1-8:   Small (1-8 KB) - textures/sprites
  - Assets 9-16:  Medium (16-128 KB) - models/level chunks
  - Assets 17-24: Large (128-256 KB) - music/large data

Each asset contains a header with its ID and size, then a repeating
pattern so we can verify data integrity visually on the N64.
"""

import os
import struct
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import asset_pack

ASSETS = []

def make_asset(asset_id, size, label):
    """Create an asset with a recognizable pattern."""
    # 16-byte header: magic + id + size + label hash
    header = struct.pack("<IIII",
                         0xA55E7000 | asset_id,  # Magic with embedded ID
                         asset_id,
                         size,
                         hash(label) & 0xFFFFFFFF)

    # Fill remainder with repeating pattern based on asset ID
    pattern = f"[{label}:{asset_id}]".encode()
    body_size = size - len(header)
    repeats = body_size // len(pattern) + 1
    body = (pattern * repeats)[:body_size]

    return header + body


def main():
    build_dir = sys.argv[1] if len(sys.argv) > 1 else "build"
    os.makedirs(build_dir, exist_ok=True)

    asset_dir = os.path.join(build_dir, "test_assets")
    os.makedirs(asset_dir, exist_ok=True)

    manifest_assets = []

    # Small assets (1-8 KB)
    small_labels = ["sprite_coin", "sprite_heart", "tex_grass", "tex_brick",
                    "sfx_jump", "sfx_coin", "sfx_hurt", "sfx_powerup"]
    for i, label in enumerate(small_labels):
        asset_id = i + 1
        size = (i + 1) * 1024  # 1KB to 8KB
        data = make_asset(asset_id, size, label)
        fname = f"{label}.bin"
        with open(os.path.join(asset_dir, fname), "wb") as f:
            f.write(data)
        manifest_assets.append({"id": asset_id, "file": fname, "compress": True})

    # Medium assets (16-128 KB)
    medium_labels = ["mdl_player", "mdl_enemy", "mdl_tree", "mdl_house",
                     "level1_geo", "level1_col", "level2_geo", "level2_col"]
    for i, label in enumerate(medium_labels):
        asset_id = i + 9
        size = (i + 1) * 16 * 1024  # 16KB to 128KB
        data = make_asset(asset_id, size, label)
        fname = f"{label}.bin"
        with open(os.path.join(asset_dir, fname), "wb") as f:
            f.write(data)
        manifest_assets.append({"id": asset_id, "file": fname, "compress": True})

    # Large assets (128-256 KB)
    large_labels = ["music_title", "music_level1", "music_level2", "music_boss",
                    "cutscene_intro", "cutscene_ending", "level3_geo", "level3_tex"]
    for i, label in enumerate(large_labels):
        asset_id = i + 17
        size = 128 * 1024 + i * 18 * 1024  # 128KB to ~254KB
        data = make_asset(asset_id, size, label)
        fname = f"{label}.bin"
        with open(os.path.join(asset_dir, fname), "wb") as f:
            f.write(data)
        manifest_assets.append({"id": asset_id, "file": fname, "compress": True})

    manifest = {"assets": manifest_assets}

    # Pack archive
    print(f"Packing {len(manifest_assets)} test assets...")
    archive = asset_pack.pack_archive(manifest, asset_dir)

    archive_path = os.path.join(build_dir, "debug_assets.bin")
    with open(archive_path, "wb") as f:
        f.write(archive)
    print(f"\nArchive: {archive_path} ({len(archive):,d} bytes)")

    # Generate C header
    header_path = os.path.join(build_dir, "debug_asset_ids.h")
    asset_pack.emit_c_header(manifest, header_path, asset_dir)

    # Also print a summary
    print(f"\n{'ID':>4s}  {'Label':<20s}  {'Size':>8s}")
    print(f"{'--':>4s}  {'-----':<20s}  {'----':>8s}")
    all_labels = small_labels + medium_labels + large_labels
    all_sizes = (
        [(i + 1) * 1024 for i in range(8)] +
        [(i + 1) * 16 * 1024 for i in range(8)] +
        [128 * 1024 + i * 18 * 1024 for i in range(8)]
    )
    for i, (label, size) in enumerate(zip(all_labels, all_sizes)):
        print(f"{i+1:4d}  {label:<20s}  {size:7d}B")


if __name__ == "__main__":
    main()
