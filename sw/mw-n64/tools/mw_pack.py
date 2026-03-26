#!/usr/bin/env python3
"""
MW-N64: Morrowind asset packer for SC64

Converts Morrowind game data into the SC64 asset streaming archive.

Input (data_dir/):
    Morrowind.esm          — Master game data file
    Tribunal.esm           — Expansion (optional)
    Bloodmoon.esm          — Expansion (optional)
    Textures/              — .DDS/.BMP/.TGA texture files
    Meshes/                — .NIF mesh files (pre-converted to .n64mesh)
    Sound/                 — .WAV sound effects
    Music/                 — Music tracks

Output:
    <out_dir>/mw_assets.bin    — SC64 asset archive
    <out_dir>/mw_asset_ids.h   — C header with asset IDs

Asset ID layout:
    1         Morrowind.esm (the entire master file)
    2         Tribunal.esm (optional)
    3         Bloodmoon.esm (optional)
    100-4999  Pre-converted meshes (hashed from filename)
    5000-5999 Textures (pre-converted CI8 pages)
    6000-6999 Sound effects
    7000-7099 Music tracks

Usage:
    python3 mw_pack.py <data_dir> <output_dir>
"""

import os
import sys
import hashlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'asset_streaming', 'tools'))
import asset_pack

def filename_to_id(filename, base, max_range):
    """Deterministic hash of filename to asset ID."""
    h = int(hashlib.md5(filename.lower().encode()).hexdigest()[:8], 16)
    return base + (h % max_range)

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <data_dir> <output_dir>")
        sys.exit(1)

    data_dir = sys.argv[1]
    out_dir = sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    staging = os.path.join(out_dir, "mw_staging")
    os.makedirs(staging, exist_ok=True)

    manifest_assets = []

    # Pack ESM files
    for esm_id, esm_name in [(1, "Morrowind.esm"), (2, "Tribunal.esm"), (3, "Bloodmoon.esm")]:
        esm_path = os.path.join(data_dir, esm_name)
        if not os.path.exists(esm_path):
            # Try case-insensitive
            for fn in os.listdir(data_dir):
                if fn.lower() == esm_name.lower():
                    esm_path = os.path.join(data_dir, fn)
                    break
        if os.path.exists(esm_path):
            import shutil
            dst = os.path.join(staging, esm_name)
            shutil.copy2(esm_path, dst)
            size = os.path.getsize(esm_path)
            manifest_assets.append({"id": esm_id, "file": esm_name, "compress": False})
            print(f"  ESM: {esm_name} ({size/1024/1024:.1f} MB) -> asset {esm_id}")

    # Pack pre-converted meshes
    mesh_dir = os.path.join(data_dir, "Meshes_N64")
    if os.path.isdir(mesh_dir):
        for root, dirs, files in os.walk(mesh_dir):
            for fn in files:
                if fn.endswith('.n64mesh'):
                    rel = os.path.relpath(os.path.join(root, fn), mesh_dir)
                    aid = filename_to_id(rel, 100, 4900)
                    import shutil
                    dst = os.path.join(staging, f"mesh_{aid}.bin")
                    shutil.copy2(os.path.join(root, fn), dst)
                    manifest_assets.append({"id": aid, "file": f"mesh_{aid}.bin", "compress": True})

    # Pack textures (pre-converted to raw CI8)
    tex_dir = os.path.join(data_dir, "Textures_N64")
    if os.path.isdir(tex_dir):
        for fn in sorted(os.listdir(tex_dir)):
            if fn.endswith('.ci8'):
                aid = filename_to_id(fn, 5000, 1000)
                import shutil
                shutil.copy2(os.path.join(tex_dir, fn), os.path.join(staging, fn))
                manifest_assets.append({"id": aid, "file": fn, "compress": True})

    # Pack sound effects
    snd_dir = os.path.join(data_dir, "Sound")
    if os.path.isdir(snd_dir):
        for root, dirs, files in os.walk(snd_dir):
            for fn in files:
                if fn.lower().endswith('.wav'):
                    rel = os.path.relpath(os.path.join(root, fn), snd_dir)
                    aid = filename_to_id(rel, 6000, 1000)
                    import shutil
                    dst = os.path.join(staging, f"snd_{aid}.wav")
                    shutil.copy2(os.path.join(root, fn), dst)
                    manifest_assets.append({"id": aid, "file": f"snd_{aid}.wav", "compress": False})

    # Pack music
    music_dir = os.path.join(data_dir, "Music")
    if os.path.isdir(music_dir):
        track_id = 7000
        for fn in sorted(os.listdir(music_dir)):
            if fn.lower().endswith(('.mp3', '.wav', '.ogg')):
                import shutil
                dst = os.path.join(staging, f"music_{track_id}.bin")
                shutil.copy2(os.path.join(music_dir, fn), dst)
                manifest_assets.append({"id": track_id, "file": f"music_{track_id}.bin", "compress": False})
                print(f"  Music: {fn} -> asset {track_id}")
                track_id += 1

    if not manifest_assets:
        print("ERROR: No game data found!")
        print(f"  Expected: {data_dir}/Morrowind.esm")
        sys.exit(1)

    # Pack archive
    print(f"\nPacking {len(manifest_assets)} assets...")
    manifest = {"assets": manifest_assets}
    archive = asset_pack.pack_archive(manifest, staging)

    archive_path = os.path.join(out_dir, "mw_assets.bin")
    with open(archive_path, "wb") as f:
        f.write(archive)
    print(f"\nArchive: {archive_path} ({len(archive)/1024/1024:.1f} MB)")
    print(f"\nWrite to SD card:")
    print(f"  dd if={archive_path} of=/dev/sdX bs=512 seek=65536")

if __name__ == "__main__":
    main()
