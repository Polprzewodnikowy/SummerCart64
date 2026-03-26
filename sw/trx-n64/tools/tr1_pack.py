#!/usr/bin/env python3
"""
TR1 Asset Packer for SC64

Converts original Tomb Raider 1 game data (.PHD level files, audio,
music) into the SC64 asset streaming archive format.

Usage:
    python3 tr1_pack.py <game_data_dir> <output_dir>

Input structure (game_data_dir/):
    levels/
        LEVEL1.PHD
        LEVEL2.PHD
        ...
        LEVEL15.PHD
        TITLE.PHD
        GYM.PHD
    music/
        track02.ogg    (or .wav, .raw)
        ...
    fmv/
        (optional FMV data)

Output:
    <output_dir>/tr1_assets.bin     - SC64 asset archive
    <output_dir>/tr1_asset_ids.h    - C header with asset ID defines

Asset ID layout:
    1-15     Level geometry
    16       Title screen level
    17       Gym level
    100-115  Level texture pages (base + page_num)
    200-215  Level audio samples
    300+     Music tracks
"""

import os
import struct
import sys
import glob

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'asset_streaming', 'tools'))
import asset_pack

# TR1 level names in order
TR1_LEVELS = [
    "LEVEL1",   # 0: Caves
    "LEVEL2",   # 1: City of Vilcabamba
    "LEVEL3A",  # 2: Lost Valley
    "LEVEL3B",  # 3: Tomb of Qualopec
    "LEVEL4",   # 4: St. Francis' Folly
    "LEVEL5",   # 5: Colosseum
    "LEVEL6",   # 6: Palace Midas
    "LEVEL7A",  # 7: The Cistern
    "LEVEL7B",  # 8: Tomb of Tihocan
    "LEVEL8A",  # 9: City of Khamoon
    "LEVEL8B",  # 10: Obelisk of Khamoon
    "LEVEL8C",  # 11: Sanctuary of the Scion
    "LEVEL10A", # 12: Natla's Mines
    "LEVEL10B", # 13: Atlantis
    "LEVEL10C", # 14: The Great Pyramid
]

ASSET_LEVEL_BASE = 1
ASSET_TITLE = 16
ASSET_GYM = 17
ASSET_TEXPAGE_BASE = 100
ASSET_TEXPAGE_STRIDE = 16
ASSET_SAMPLES_BASE = 200
ASSET_MUSIC_BASE = 300


def find_level_file(data_dir, name):
    """Find a level file case-insensitively."""
    patterns = [
        os.path.join(data_dir, "levels", f"{name}.PHD"),
        os.path.join(data_dir, "levels", f"{name}.phd"),
        os.path.join(data_dir, f"{name}.PHD"),
        os.path.join(data_dir, f"{name}.phd"),
    ]
    for p in patterns:
        matches = glob.glob(p)
        if matches:
            return matches[0]
    return None


def pack_tr1(data_dir, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    asset_dir = os.path.join(output_dir, "tr1_assets_staging")
    os.makedirs(asset_dir, exist_ok=True)

    manifest_assets = []
    found_count = 0

    # Pack level files
    for i, name in enumerate(TR1_LEVELS):
        path = find_level_file(data_dir, name)
        if not path:
            print(f"  Warning: {name}.PHD not found, skipping")
            continue

        asset_id = ASSET_LEVEL_BASE + i
        fname = f"level_{i:02d}.phd"

        # Copy level file to staging
        import shutil
        shutil.copy2(path, os.path.join(asset_dir, fname))

        manifest_assets.append({
            "id": asset_id,
            "file": fname,
            "compress": True,
        })
        size = os.path.getsize(path)
        print(f"  Level {i:2d}: {name:12s}  {size:>10,d} bytes  -> asset {asset_id}")
        found_count += 1

    # Title screen
    title_path = find_level_file(data_dir, "TITLE")
    if title_path:
        import shutil
        shutil.copy2(title_path, os.path.join(asset_dir, "title.phd"))
        manifest_assets.append({
            "id": ASSET_TITLE,
            "file": "title.phd",
            "compress": True,
        })
        print(f"  Title:   TITLE         {os.path.getsize(title_path):>10,d} bytes  -> asset {ASSET_TITLE}")
        found_count += 1

    # Gym
    gym_path = find_level_file(data_dir, "GYM")
    if gym_path:
        import shutil
        shutil.copy2(gym_path, os.path.join(asset_dir, "gym.phd"))
        manifest_assets.append({
            "id": ASSET_GYM,
            "file": "gym.phd",
            "compress": True,
        })
        print(f"  Gym:     GYM           {os.path.getsize(gym_path):>10,d} bytes  -> asset {ASSET_GYM}")
        found_count += 1

    # Music tracks
    music_dir = os.path.join(data_dir, "music")
    if os.path.isdir(music_dir):
        for track_file in sorted(os.listdir(music_dir)):
            # Extract track number from filename
            name_part = os.path.splitext(track_file)[0]
            try:
                track_num = int(''.join(c for c in name_part if c.isdigit()))
            except ValueError:
                continue

            asset_id = ASSET_MUSIC_BASE + track_num
            import shutil
            shutil.copy2(
                os.path.join(music_dir, track_file),
                os.path.join(asset_dir, track_file))
            manifest_assets.append({
                "id": asset_id,
                "file": track_file,
                "compress": True,
            })
            size = os.path.getsize(os.path.join(music_dir, track_file))
            print(f"  Music:   track {track_num:2d}      {size:>10,d} bytes  -> asset {asset_id}")

    if found_count == 0:
        print("\nERROR: No game data found!")
        print(f"  Looked in: {data_dir}")
        print(f"  Expected:  levels/LEVEL1.PHD, etc.")
        sys.exit(1)

    # Pack archive
    print(f"\nPacking {len(manifest_assets)} assets...")
    manifest = {"assets": manifest_assets}
    archive = asset_pack.pack_archive(manifest, asset_dir)

    archive_path = os.path.join(output_dir, "tr1_assets.bin")
    with open(archive_path, "wb") as f:
        f.write(archive)
    print(f"\nArchive: {archive_path} ({len(archive):,d} bytes)")

    # Generate C header
    header_path = os.path.join(output_dir, "tr1_asset_ids.h")
    with open(header_path, "w") as f:
        f.write("/* Auto-generated by tr1_pack.py -- do not edit */\n")
        f.write("#ifndef TR1_ASSET_IDS_H__\n")
        f.write("#define TR1_ASSET_IDS_H__\n\n")

        f.write("/* Level geometry */\n")
        for i, name in enumerate(TR1_LEVELS):
            f.write(f"#define ASSET_LEVEL_{name:<12s}  ({ASSET_LEVEL_BASE + i})\n")
        f.write(f"#define ASSET_TITLE                  ({ASSET_TITLE})\n")
        f.write(f"#define ASSET_GYM                    ({ASSET_GYM})\n")
        f.write(f"\n/* Texture pages: TEXPAGE(level, page) */\n")
        f.write(f"#define ASSET_TEXPAGE(lev, pg)       ({ASSET_TEXPAGE_BASE} + (lev) * {ASSET_TEXPAGE_STRIDE} + (pg))\n")
        f.write(f"\n/* Audio samples per level */\n")
        f.write(f"#define ASSET_SAMPLES(lev)           ({ASSET_SAMPLES_BASE} + (lev))\n")
        f.write(f"\n/* Music tracks */\n")
        f.write(f"#define ASSET_MUSIC(track)           ({ASSET_MUSIC_BASE} + (track))\n")
        f.write(f"\n#define TR1_LEVEL_COUNT              ({len(TR1_LEVELS)})\n")
        f.write(f"#define TR1_ASSET_COUNT              ({len(manifest_assets)})\n")
        f.write(f"\n#endif\n")

    print(f"Header: {header_path}")

    # Summary
    print(f"\n{'='*50}")
    print(f"TR1 Asset Pack Complete")
    print(f"  Levels found:  {found_count}")
    print(f"  Total assets:  {len(manifest_assets)}")
    print(f"  Archive size:  {len(archive):,d} bytes ({len(archive)/1024/1024:.1f} MB)")
    print(f"\nWrite to SD card at sector 0x10000:")
    print(f"  dd if={archive_path} of=/dev/sdX bs=512 seek=65536")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <game_data_dir> <output_dir>")
        print(f"\nExpects TR1 game data at <game_data_dir>/levels/LEVEL1.PHD etc.")
        sys.exit(1)

    pack_tr1(sys.argv[1], sys.argv[2])
