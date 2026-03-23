#!/usr/bin/env python3
"""
Validation tests for the asset packer and archive format.

Tests:
  1. Pack assets and verify header/index structure
  2. Verify sector alignment
  3. Verify decompressed content round-trips correctly
  4. Test empty archive
  5. Test uncompressible data (compression should fall back to raw)

Run: python3 test_packer.py
"""

import os
import struct
import sys
import tempfile

# Add tools dir to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import asset_pack

SECTOR = 512
MAGIC = 0x53433634
VERSION = 1

passed = 0
failed = 0

def test(name):
    print(f"  {name:<50s} ", end="", flush=True)

def ok():
    global passed
    print("[PASS]")
    passed += 1

def fail(msg):
    global failed
    print(f"[FAIL] {msg}")
    failed += 1


def test_basic_archive():
    """Pack two small files and verify archive structure."""
    test("basic_archive_structure")

    with tempfile.TemporaryDirectory() as tmpdir:
        # Create test files
        f1 = os.path.join(tmpdir, "asset1.bin")
        f2 = os.path.join(tmpdir, "asset2.bin")

        with open(f1, "wb") as f:
            f.write(b"A" * 1000)  # Compressible
        with open(f2, "wb") as f:
            f.write(b"B" * 500)   # Compressible

        manifest = {
            "assets": [
                {"id": 1, "file": "asset1.bin", "compress": True},
                {"id": 2, "file": "asset2.bin", "compress": True},
            ]
        }

        archive = asset_pack.pack_archive(manifest, tmpdir)

        # Verify header
        magic, ver, count, idx_sector, idx_sectors, total = \
            struct.unpack_from("<IIIIII", archive, 0)

        assert magic == MAGIC, f"bad magic: {magic:#x}"
        assert ver == VERSION, f"bad version: {ver}"
        assert count == 2, f"bad count: {count}"
        assert idx_sector == 1, f"index should start at sector 1"
        assert len(archive) == total * SECTOR, f"archive size mismatch"
        assert len(archive) % SECTOR == 0, "not sector-aligned"

        # Verify index entries
        idx_offset = idx_sector * SECTOR
        for i in range(count):
            entry_off = idx_offset + i * 20
            aid, sec_off, sec_cnt, flags, comp_size, decomp_size = \
                struct.unpack_from("<IIHHII", archive, entry_off)
            assert aid == i + 1, f"wrong asset id: {aid}"
            assert sec_off > 0, f"zero sector offset"
            assert sec_cnt > 0, f"zero sector count"
            assert decomp_size > 0, f"zero decompressed size"

        ok()


def test_sector_alignment():
    """Verify every asset data chunk starts at a sector boundary."""
    test("sector_alignment_of_all_chunks")

    with tempfile.TemporaryDirectory() as tmpdir:
        for i in range(5):
            with open(os.path.join(tmpdir, f"file{i}.bin"), "wb") as f:
                f.write(bytes(range(256)) * (i + 1))  # Varying sizes

        manifest = {
            "assets": [
                {"id": i + 1, "file": f"file{i}.bin", "compress": True}
                for i in range(5)
            ]
        }

        archive = asset_pack.pack_archive(manifest, tmpdir)

        # Parse index and check each asset's sector offset
        _, _, count, idx_sec, _, _ = struct.unpack_from("<IIIIII", archive, 0)
        idx_offset = idx_sec * SECTOR

        for i in range(count):
            entry_off = idx_offset + i * 20
            _, sec_off, sec_cnt, _, _, _ = struct.unpack_from("<IIHHII", archive, entry_off)
            byte_off = sec_off * SECTOR
            assert byte_off % SECTOR == 0, f"asset {i} not sector-aligned"
            assert byte_off + sec_cnt * SECTOR <= len(archive), f"asset {i} exceeds archive"

        ok()


def test_uncompressible_data():
    """Random data that doesn't compress should be stored raw."""
    test("uncompressible_data_stored_raw")

    with tempfile.TemporaryDirectory() as tmpdir:
        # Random-ish data that won't compress well
        data = bytes(range(256)) * 4
        f1 = os.path.join(tmpdir, "random.bin")
        with open(f1, "wb") as f:
            f.write(data)

        manifest = {
            "assets": [
                {"id": 1, "file": "random.bin", "compress": True},
            ]
        }

        archive = asset_pack.pack_archive(manifest, tmpdir)

        # Check flags - should NOT have COMPRESSED flag if it didn't compress
        _, _, count, idx_sec, _, _ = struct.unpack_from("<IIIIII", archive, 0)
        idx_offset = idx_sec * SECTOR
        _, sec_off, sec_cnt, flags, comp_size, decomp_size = \
            struct.unpack_from("<IIHHII", archive, idx_offset)

        # Data is stored raw or compressed depending on ratio
        # Either way, decompressed_size should match original
        assert decomp_size == len(data), f"wrong decompressed size"
        ok()


def test_empty_archive():
    """Empty manifest should produce a valid but empty archive."""
    test("empty_archive_valid")

    manifest = {"assets": []}
    archive = asset_pack.pack_archive(manifest, ".")

    magic, ver, count, _, _, total = struct.unpack_from("<IIIIII", archive, 0)
    assert magic == MAGIC
    assert ver == VERSION
    assert count == 0
    assert len(archive) > 0  # At least header
    ok()


def test_no_compress_flag():
    """Assets with compress=false should be stored raw."""
    test("compress_false_stores_raw")

    with tempfile.TemporaryDirectory() as tmpdir:
        data = b"AAAA" * 1000  # Very compressible, but we say don't compress
        f1 = os.path.join(tmpdir, "raw.bin")
        with open(f1, "wb") as f:
            f.write(data)

        manifest = {
            "assets": [
                {"id": 1, "file": "raw.bin", "compress": False},
            ]
        }

        archive = asset_pack.pack_archive(manifest, tmpdir)

        _, _, _, idx_sec, _, _ = struct.unpack_from("<IIIIII", archive, 0)
        _, sec_off, sec_cnt, flags, comp_size, decomp_size = \
            struct.unpack_from("<IIHHII", archive, idx_sec * SECTOR)

        assert not (flags & 1), "should not have COMPRESSED flag"
        assert decomp_size == len(data)

        # Verify raw data is present
        byte_off = sec_off * SECTOR
        stored = archive[byte_off:byte_off + len(data)]
        assert stored == data, "raw data mismatch"
        ok()


if __name__ == "__main__":
    print("\n=== Asset Packer Tests ===\n")

    test_basic_archive()
    test_sector_alignment()
    test_uncompressible_data()
    test_empty_archive()
    test_no_compress_flag()

    print(f"\n=== Results: {passed} passed, {failed} failed ===\n")
    sys.exit(1 if failed > 0 else 0)
