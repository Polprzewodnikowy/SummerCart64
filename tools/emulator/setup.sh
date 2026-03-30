#!/bin/bash
# setup.sh — Verify emulator tooling is installed and ready
#
# Checks for: ares, Xvfb, imagemagick (import)
# If missing, prints instructions.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/bin"
OK=true

echo "=== Emulator Setup Check ==="

# Check ares
if [ -x "$BIN/ares" ]; then
    echo "[OK] ares found at $BIN/ares"
else
    echo "[MISSING] ares not found at $BIN/ares"
    echo "  Build from source: cd /tmp/ares && cmake --preset ubuntu-ci && cmake --build build -j\$(nproc)"
    echo "  Then copy: cp /tmp/ares/build/desktop-ui/ares $BIN/"
    OK=false
fi

# Check Xvfb
if [ -x "$BIN/Xvfb" ]; then
    echo "[OK] Xvfb found at $BIN/Xvfb"
elif command -v Xvfb &>/dev/null; then
    echo "[OK] Xvfb found at $(which Xvfb) (system)"
else
    echo "[MISSING] Xvfb — install with: apt-get install -y xvfb"
    OK=false
fi

# Check imagemagick import (for screenshots)
if command -v import &>/dev/null; then
    echo "[OK] import (imagemagick) available"
else
    echo "[OPTIONAL] import not found — install imagemagick for screenshots"
    echo "  apt-get install -y imagemagick"
fi

# Check xdotool (for sending keys)
if command -v xdotool &>/dev/null; then
    echo "[OK] xdotool available"
else
    echo "[OPTIONAL] xdotool not found — install for key input automation"
    echo "  apt-get install -y xdotool"
fi

echo ""
if $OK; then
    echo "All required tools present. Ready to test ROMs."
    echo "Usage: $SCRIPT_DIR/run_test.sh <rom.z64> [frames] [screenshot.png]"
else
    echo "Some tools missing. See instructions above."
    exit 1
fi
