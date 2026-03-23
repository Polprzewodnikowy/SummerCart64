#!/usr/bin/env python3
"""
End-to-end round-trip test: pack with Python LZ4 -> decompress with C LZ4.

Verifies that the Python packer's compressed output is correctly
decompressed by our C lz4_decode_block implementation.

Run: python3 test_roundtrip.py
"""

import ctypes
import os
import struct
import subprocess
import sys
import tempfile

SECTOR = 512

# Build a shared library from lz4_decode.c for testing
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(SCRIPT_DIR, '..', 'src')
BUILD_DIR = tempfile.mkdtemp()

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


def build_decoder_lib():
    """Compile lz4_decode.c into a shared library for ctypes."""
    lib_path = os.path.join(BUILD_DIR, 'liblz4_decode.so')
    src_path = os.path.join(SRC_DIR, 'lz4_decode.c')

    result = subprocess.run(
        ['cc', '-shared', '-fPIC', '-o', lib_path, src_path, f'-I{SRC_DIR}'],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"Build failed: {result.stderr}")
        sys.exit(1)

    return ctypes.CDLL(lib_path)


def c_decompress(lib, compressed_data, max_output):
    """Call our C lz4_decode_block via ctypes."""
    func = lib.lz4_decode_block
    func.restype = ctypes.c_int
    func.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),  # src
        ctypes.POINTER(ctypes.c_uint8),  # dst
        ctypes.c_int,                     # compressed_size
        ctypes.c_int,                     # max_output_size
    ]

    src_buf = (ctypes.c_uint8 * len(compressed_data))(*compressed_data)
    dst_buf = (ctypes.c_uint8 * max_output)()

    decoded = func(src_buf, dst_buf, len(compressed_data), max_output)
    if decoded < 0:
        return None
    return bytes(dst_buf[:decoded])


def test_roundtrip_compressible(lib):
    """Compress with Python LZ4, decompress with C. Verify match."""
    test("roundtrip_compressible_data")

    try:
        import lz4.block
    except ImportError:
        print("[SKIP] lz4 not installed")
        return

    # Create a compressible pattern
    original = b"Hello World! " * 100 + b"ABCDEFG" * 50
    compressed = lz4.block.compress(original, mode='high_compression', store_size=False)

    # Decompress with our C implementation
    result = c_decompress(lib, compressed, len(original) + 1024)
    if result is None:
        fail("C decompressor returned error")
        return
    if result != original:
        fail(f"data mismatch: got {len(result)} bytes, expected {len(original)}")
        return
    ok()


def test_roundtrip_repetitive(lib):
    """Test with highly repetitive data (exercises match copying)."""
    test("roundtrip_highly_repetitive")

    try:
        import lz4.block
    except ImportError:
        print("[SKIP] lz4 not installed")
        return

    original = b"A" * 10000
    compressed = lz4.block.compress(original, mode='high_compression', store_size=False)

    result = c_decompress(lib, compressed, len(original) + 1024)
    if result is None:
        fail("C decompressor returned error")
        return
    if result != original:
        fail("repetitive data mismatch")
        return
    ok()


def test_roundtrip_binary(lib):
    """Test with binary data (all byte values)."""
    test("roundtrip_binary_data")

    try:
        import lz4.block
    except ImportError:
        print("[SKIP] lz4 not installed")
        return

    original = bytes(range(256)) * 20
    compressed = lz4.block.compress(original, mode='high_compression', store_size=False)

    result = c_decompress(lib, compressed, len(original) + 1024)
    if result is None:
        fail("C decompressor returned error")
        return
    if result != original:
        fail("binary data mismatch")
        return
    ok()


def test_roundtrip_large(lib):
    """Test with a larger payload (~100KB)."""
    test("roundtrip_100kb_mixed_data")

    try:
        import lz4.block
    except ImportError:
        print("[SKIP] lz4 not installed")
        return

    # Mix of compressible and semi-random data
    original = b""
    for i in range(100):
        original += bytes([i & 0xFF]) * 1000  # Compressible runs
        original += bytes(range(256))[:min(i + 1, 256)]  # Varying entropy

    compressed = lz4.block.compress(original, mode='high_compression', store_size=False)

    result = c_decompress(lib, compressed, len(original) + 4096)
    if result is None:
        fail("C decompressor returned error")
        return
    if result != original:
        fail(f"large data mismatch: {len(result)} vs {len(original)}")
        return

    ratio = len(compressed) / len(original)
    print(f"[PASS] ({len(original)} -> {len(compressed)}, {ratio:.1%})")
    global passed
    passed += 1


def test_full_pipeline(lib):
    """Full pipeline: create files -> pack archive -> read back -> decompress -> verify."""
    test("full_pipeline_pack_and_decompress")

    try:
        import lz4.block
    except ImportError:
        print("[SKIP] lz4 not installed")
        return

    sys.path.insert(0, os.path.join(SCRIPT_DIR, '..', 'tools'))
    import asset_pack

    with tempfile.TemporaryDirectory() as tmpdir:
        # Create test assets
        test_data = {}
        for i in range(3):
            data = f"Asset {i} data: ".encode() * (100 * (i + 1))
            fname = f"asset_{i}.bin"
            path = os.path.join(tmpdir, fname)
            with open(path, "wb") as f:
                f.write(data)
            test_data[i + 1] = data

        manifest = {
            "assets": [
                {"id": i + 1, "file": f"asset_{i}.bin", "compress": True}
                for i in range(3)
            ]
        }

        archive = asset_pack.pack_archive(manifest, tmpdir)

        # Parse header
        _, _, count, idx_sec, _, _ = struct.unpack_from("<IIIIII", archive, 0)
        idx_offset = idx_sec * SECTOR

        # For each asset, extract compressed data and decompress
        for i in range(count):
            entry_off = idx_offset + i * 20
            aid, sec_off, sec_cnt, flags, comp_size, decomp_size = \
                struct.unpack_from("<IIHHII", archive, entry_off)

            byte_off = sec_off * SECTOR
            # Use exact compressed_size, not sector-padded size
            chunk = archive[byte_off:byte_off + comp_size]

            if flags & 1:  # COMPRESSED
                result = c_decompress(lib, chunk, decomp_size + 4096)
                if result is None:
                    fail(f"decompression failed for asset {aid}")
                    return
                if result[:decomp_size] != test_data[aid]:
                    fail(f"data mismatch for asset {aid}")
                    return
            else:
                if chunk[:decomp_size] != test_data[aid]:
                    fail(f"raw data mismatch for asset {aid}")
                    return

        ok()


if __name__ == "__main__":
    print("\n=== Round-Trip Tests (Python LZ4 -> C LZ4 Decoder) ===\n")

    print("Building C decoder library...")
    lib = build_decoder_lib()
    print(f"Built: {BUILD_DIR}/liblz4_decode.so\n")

    test_roundtrip_compressible(lib)
    test_roundtrip_repetitive(lib)
    test_roundtrip_binary(lib)
    test_roundtrip_large(lib)
    test_full_pipeline(lib)

    print(f"\n=== Results: {passed} passed, {failed} failed ===\n")
    sys.exit(1 if failed > 0 else 0)
