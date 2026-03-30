#!/bin/bash
# run_test.sh — Boot a .z64 ROM in ares under Xvfb, capture screenshot + debug output
#
# Usage: ./run_test.sh <rom.z64> [frames] [screenshot.png]
#   rom.z64      — path to N64 ROM
#   frames       — how many frames to run (default: 120 = ~2 seconds)
#   screenshot   — output screenshot path (default: /tmp/ares_capture.png)
#
# Requires: ares + Xvfb (stored in tools/emulator/bin/)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/bin"
ROM="${1:?Usage: $0 <rom.z64> [frames] [screenshot.png]}"
FRAMES="${2:-120}"
SCREENSHOT="${3:-/tmp/ares_capture.png}"
DISPLAY_NUM=":99"
LOG="/tmp/ares_test.log"

# Resolve tool paths: prefer local bin/, fall back to system
ARES="$BIN/ares"
XVFB="$BIN/Xvfb"
[ -x "$XVFB" ] || XVFB="$(command -v Xvfb 2>/dev/null || true)"
IMPORT="$BIN/import"
[ -x "$IMPORT" ] || IMPORT="$(command -v import 2>/dev/null || true)"

if [ ! -x "$ARES" ]; then
    echo "ERROR: ares not found at $BIN/ares"
    echo "Run setup.sh or build ares and copy to $BIN/"
    exit 1
fi

if [ -z "$XVFB" ] || [ ! -x "$XVFB" ]; then
    echo "ERROR: Xvfb not found. Install with: apt-get install -y xvfb"
    exit 1
fi

# Clean up on exit
cleanup() {
    [ -n "${ARES_PID:-}" ] && kill "$ARES_PID" 2>/dev/null; wait "$ARES_PID" 2>/dev/null
    [ -n "${XVFB_PID:-}" ] && kill "$XVFB_PID" 2>/dev/null; wait "$XVFB_PID" 2>/dev/null
    true
}
trap cleanup EXIT

# Start virtual framebuffer
"$XVFB" $DISPLAY_NUM -screen 0 640x480x24 +extension GLX &
XVFB_PID=$!
sleep 1

export DISPLAY=$DISPLAY_NUM

# Run ares with audio disabled (no PulseAudio in headless)
echo "Booting ROM: $ROM"
"$ARES" --setting "Audio/Driver=None" --system "Nintendo 64" --fullscreen "$ROM" > "$LOG" 2>&1 &
ARES_PID=$!

# Wait for frames to render
WAIT_SEC=$(echo "scale=1; $FRAMES / 60" | bc)
echo "Running $FRAMES frames (~${WAIT_SEC}s)..."
sleep "$WAIT_SEC"

# Capture screenshot if imagemagick is available
if [ -n "$IMPORT" ] && [ -x "$IMPORT" ]; then
    "$IMPORT" -window root "$SCREENSHOT" 2>/dev/null
    echo "Screenshot: $SCREENSHOT"
else
    echo "(Skipping screenshot — install imagemagick for capture)"
fi

echo "Debug log: $LOG"

# Print debug output from the ROM (ares forwards N64 printf to stderr/stdout)
if [ -s "$LOG" ]; then
    echo "=== Debug Output ==="
    head -100 "$LOG"
fi
