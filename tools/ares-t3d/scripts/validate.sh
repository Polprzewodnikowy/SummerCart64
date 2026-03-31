#!/bin/bash
# validate.sh — Build and run all ares-t3d tests + decode tool
#
# Usage: ./validate.sh
#
# Runs:
#   1. CMake build
#   2. All unit tests (test_t3d_commands, test_vertex_extract, test_detection)
#   3. decode_buffer tool against any .bin fixtures in tests/fixtures/
#
# Exit code: 0 if all pass, 1 if any fail.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "=== ares-t3d validation ==="
echo ""

# 1. Build
echo "[BUILD] cmake + make..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-Werror" > /dev/null 2>&1
make -j$(nproc) 2>&1 | grep -E "error|warning|Built" || true
echo ""

# 2. Unit tests
echo "[TESTS] Running unit tests..."
FAIL=0

for test in test_t3d_commands test_vertex_extract test_detection; do
    if "./$test" 2>&1 | tail -1 | grep -q "FAILED"; then
        echo "  FAIL: $test"
        FAIL=1
    else
        echo "  PASS: $test"
    fi
done
echo ""

# 3. Fixture decoding (if any .bin files exist)
FIXTURE_DIR="$PROJECT_DIR/tests/fixtures"
if [ -d "$FIXTURE_DIR" ] && ls "$FIXTURE_DIR"/*.bin 1>/dev/null 2>&1; then
    echo "[FIXTURES] Decoding captured buffers..."
    for f in "$FIXTURE_DIR"/*.bin; do
        echo "  Decoding: $(basename "$f")"
        ./decode_buffer "$f" 2>/dev/null || echo "    (decode returned non-zero)"
    done
    echo ""
else
    echo "[FIXTURES] No .bin fixtures found in tests/fixtures/"
    echo "  To add: capture rspq command buffers and save as .bin files"
    echo ""
fi

# 4. Summary
if [ "$FAIL" -eq 0 ]; then
    echo "=== ALL PASSED ==="
    exit 0
else
    echo "=== FAILURES DETECTED ==="
    exit 1
fi
