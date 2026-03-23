#!/usr/bin/env python3
"""
SC64 Asset Archive Packer

Creates a sector-aligned, LZ4-compressed asset archive for use with
the SC64 asset streaming library.

Usage:
    python3 asset_pack.py -o archive.bin asset_manifest.json

    # Or write directly to an SD card image at a specific sector offset:
    python3 asset_pack.py -o sdcard.img --sd-offset 0x10000 asset_manifest.json

Manifest format (JSON):
    {
        "assets": [
            {
                "id": 1,
                "file": "textures/level1.bin",
                "compress": true,
                "flags": ["resident", "prefetch"]
            },
            {
                "id": 2,
                "file": "models/player.bin",
                "compress": true
            }
        ]
    }

Or use directory mode:
    python3 asset_pack.py -o archive.bin --dir assets/ --auto-id

    This assigns sequential IDs to all files found in the directory.
"""

import argparse
import json
import math
import os
import struct
import sys

try:
    import lz4.block
    HAS_LZ4 = True
except ImportError:
    HAS_LZ4 = False

SECTOR_SIZE = 512
MAGIC = 0x53433634  # "SC64"
VERSION = 1
ENTRY_SIZE = 16

FLAG_COMPRESSED = (1 << 0)
FLAG_RESIDENT   = (1 << 1)
FLAG_PREFETCH   = (1 << 2)

FLAG_MAP = {
    "compressed": FLAG_COMPRESSED,
    "resident": FLAG_RESIDENT,
    "prefetch": FLAG_PREFETCH,
}


def align_up(value, alignment):
    return (value + alignment - 1) & ~(alignment - 1)


def sectors_for_bytes(byte_count):
    return (byte_count + SECTOR_SIZE - 1) // SECTOR_SIZE


def compress_data(data):
    """Compress data using LZ4 block format."""
    if not HAS_LZ4:
        print("ERROR: lz4 Python package required for compression.", file=sys.stderr)
        print("  Install with: pip install lz4", file=sys.stderr)
        sys.exit(1)
    return lz4.block.compress(data, mode='high_compression', store_size=False)


def pack_archive(manifest, base_dir=""):
    """Build an archive from a manifest dict. Returns the complete archive bytes."""

    assets = manifest["assets"]
    asset_count = len(assets)

    if asset_count == 0:
        print("WARNING: No assets in manifest.", file=sys.stderr)

    # Phase 1: Read and compress all asset data
    entries = []
    asset_blobs = []

    for asset_def in assets:
        asset_id = asset_def["id"]
        filepath = os.path.join(base_dir, asset_def["file"])
        should_compress = asset_def.get("compress", True)
        flag_names = asset_def.get("flags", [])

        with open(filepath, "rb") as f:
            raw_data = f.read()

        decompressed_size = len(raw_data)
        flags = 0
        for name in flag_names:
            flags |= FLAG_MAP.get(name, 0)

        if should_compress and HAS_LZ4:
            compressed = compress_data(raw_data)
            # Only use compressed version if it's actually smaller
            if len(compressed) < len(raw_data):
                blob = compressed
                flags |= FLAG_COMPRESSED
            else:
                blob = raw_data
        else:
            blob = raw_data

        # Pad blob to sector boundary
        padded_size = align_up(len(blob), SECTOR_SIZE)
        blob = blob + b'\x00' * (padded_size - len(blob))

        entries.append({
            "asset_id": asset_id,
            "flags": flags,
            "decompressed_size": decompressed_size,
            "sector_count": padded_size // SECTOR_SIZE,
        })
        asset_blobs.append(blob)

        ratio = len(blob) / decompressed_size if decompressed_size > 0 else 0
        comp_str = "compressed" if (flags & FLAG_COMPRESSED) else "raw"
        print(f"  Asset {asset_id:6d}: {os.path.basename(filepath):40s} "
              f"{decompressed_size:10d} -> {len(blob):10d} ({comp_str}, {ratio:.1%})")

    # Phase 2: Compute layout
    # Header: 1 sector
    # Index:  ceil(asset_count * ENTRY_SIZE / SECTOR_SIZE) sectors
    # Data:   variable

    index_bytes = asset_count * ENTRY_SIZE
    index_sector_count = sectors_for_bytes(index_bytes)
    header_sectors = 1
    data_start_sector = header_sectors + index_sector_count

    # Assign sector offsets to each asset
    current_sector = data_start_sector
    for i, entry in enumerate(entries):
        entry["sector_offset"] = current_sector
        current_sector += entry["sector_count"]

    total_sectors = current_sector

    # Phase 3: Build header
    header = struct.pack("<IIIIII",
                         MAGIC,
                         VERSION,
                         asset_count,
                         header_sectors,         # index starts right after header
                         index_sector_count,
                         total_sectors)
    header += b'\x00' * (SECTOR_SIZE - len(header))

    # Phase 4: Build index table
    index_data = b''
    for entry in entries:
        index_data += struct.pack("<IIHHI",
                                  entry["asset_id"],
                                  entry["sector_offset"],
                                  entry["sector_count"],
                                  entry["flags"],
                                  entry["decompressed_size"])

    # Pad index to sector boundary
    index_padded = align_up(len(index_data), SECTOR_SIZE)
    index_data += b'\x00' * (index_padded - len(index_data))

    # Phase 5: Concatenate everything
    archive = header + index_data
    for blob in asset_blobs:
        archive += blob

    assert len(archive) == total_sectors * SECTOR_SIZE

    print(f"\n  Archive: {asset_count} assets, {total_sectors} sectors "
          f"({total_sectors * SECTOR_SIZE:,d} bytes)")

    return archive


def build_manifest_from_dir(directory):
    """Auto-generate a manifest from a directory of files."""
    assets = []
    asset_id = 1

    for root, dirs, files in sorted(os.walk(directory)):
        dirs.sort()
        for fname in sorted(files):
            filepath = os.path.relpath(os.path.join(root, fname), directory)
            assets.append({
                "id": asset_id,
                "file": filepath,
                "compress": True,
            })
            asset_id += 1

    return {"assets": assets}


def main():
    parser = argparse.ArgumentParser(description="SC64 Asset Archive Packer")
    parser.add_argument("manifest", nargs="?",
                        help="JSON manifest file")
    parser.add_argument("-o", "--output", required=True,
                        help="Output archive file")
    parser.add_argument("--dir",
                        help="Pack all files from directory (auto-generates manifest)")
    parser.add_argument("--auto-id", action="store_true",
                        help="Auto-assign sequential IDs (with --dir)")
    parser.add_argument("--sd-offset", type=lambda x: int(x, 0), default=0,
                        help="Sector offset when writing into existing file (hex ok)")
    parser.add_argument("--dump-manifest", action="store_true",
                        help="Print auto-generated manifest and exit (with --dir)")

    args = parser.parse_args()

    if args.dir:
        manifest = build_manifest_from_dir(args.dir)
        if args.dump_manifest:
            print(json.dumps(manifest, indent=2))
            return
    elif args.manifest:
        with open(args.manifest) as f:
            manifest = json.load(f)
    else:
        parser.error("Must provide either a manifest file or --dir")
        return

    base_dir = os.path.dirname(args.manifest) if args.manifest else args.dir or ""

    print(f"Packing {len(manifest['assets'])} assets...")
    archive = pack_archive(manifest, base_dir)

    if args.sd_offset > 0:
        # Write at offset into existing file
        byte_offset = args.sd_offset * SECTOR_SIZE
        mode = "r+b" if os.path.exists(args.output) else "wb"
        with open(args.output, mode) as f:
            f.seek(byte_offset)
            f.write(archive)
        print(f"Written to {args.output} at sector offset 0x{args.sd_offset:X} "
              f"(byte offset 0x{byte_offset:X})")
    else:
        with open(args.output, "wb") as f:
            f.write(archive)
        print(f"Written to {args.output}")


if __name__ == "__main__":
    main()
