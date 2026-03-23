#!/usr/bin/env python3
"""
Generate test archive for integration tests.
Creates test_archive.bin with 5 known assets.
"""

import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import asset_pack

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Define test assets - must match test_integration.c expectations
ASSETS = [
    (1, b"ASSET_ONE_" * 100),     # 1000 bytes
    (2, b"ASSET_TWO_" * 200),     # 2000 bytes
    (3, b"ASSET_THREE_" * 300),   # 3600 bytes
    (4, b"FOUR" * 500),           # 2000 bytes
    (5, b"FIVER" * 400),          # 2000 bytes
]

def main():
    with tempfile.TemporaryDirectory() as tmpdir:
        manifest_assets = []

        for asset_id, data in ASSETS:
            fname = f"asset_{asset_id}.bin"
            path = os.path.join(tmpdir, fname)
            with open(path, "wb") as f:
                f.write(data)
            manifest_assets.append({
                "id": asset_id,
                "file": fname,
                "compress": True,
            })

        manifest = {"assets": manifest_assets}

        print("Generating test archive...")
        archive = asset_pack.pack_archive(manifest, tmpdir)

        output = os.path.join(SCRIPT_DIR, "test_archive.bin")
        with open(output, "wb") as f:
            f.write(archive)
        print(f"Written: {output} ({len(archive)} bytes)")


if __name__ == "__main__":
    main()
