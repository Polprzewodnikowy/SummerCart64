#!/bin/bash
# capture_perf.sh — Boot ROM, open pause menu, capture all diagnostic pages
#
# Usage: ./capture_perf.sh <rom.z64> [output_dir]
#
# Captures screenshots of each pause menu page:
#   page1 — Main diagnostics (FPS, frame time, tri counts)
#   page2 — Global budgets (tex face/tri, baked tri)
#   page3 — Per-room budgets
#   page4 — View distances
#   page5 — Room visibility dump
#
# Also captures a gameplay screenshot before opening the menu.
#
# All screenshots saved to output_dir (default: /tmp/perf_capture/)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/bin"
INPUT="$SCRIPT_DIR/n64_input.sh"
ROM="${1:?Usage: $0 <rom.z64> [output_dir]}"
OUTDIR="${2:-/tmp/perf_capture}"
DISPLAY_NUM=":99"
LOG="/tmp/ares_perf.log"

ARES="$BIN/ares"
XVFB="$BIN/Xvfb"
[ -x "$XVFB" ] || XVFB="$(command -v Xvfb 2>/dev/null || true)"
IMPORT="$BIN/import"
[ -x "$IMPORT" ] || IMPORT="$(command -v import 2>/dev/null || true)"

if [ ! -x "$ARES" ]; then echo "ERROR: ares not found at $ARES"; exit 1; fi
if [ -z "$XVFB" ]; then echo "ERROR: Xvfb not found"; exit 1; fi
if [ -z "$IMPORT" ]; then echo "ERROR: import (imagemagick) not found"; exit 1; fi

mkdir -p "$OUTDIR"

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
export DISPLAY=$DISPLAY_NUM
sleep 1

# Start ares
echo "Booting ROM: $ROM"
"$ARES" --setting "Audio/Driver=None" --system "Nintendo 64" --fullscreen "$ROM" > "$LOG" 2>&1 &
ARES_PID=$!

# Wait for ROM to boot and render a few frames
echo "Waiting for boot (5 seconds)..."
sleep 5

# Capture gameplay screenshot
echo "Capturing gameplay..."
"$IMPORT" -window root "$OUTDIR/gameplay.png" 2>/dev/null

# Open pause menu (Start button)
echo "Opening pause menu..."
"$INPUT" start 200
sleep 1

# Capture page 1 (main diagnostics)
echo "Capturing page 1: Main diagnostics"
"$IMPORT" -window root "$OUTDIR/page1_diagnostics.png" 2>/dev/null

# Navigate to page 2 (R button cycles pages in our pause menu)
"$INPUT" r 100
sleep 0.5
echo "Capturing page 2: Global budgets"
"$IMPORT" -window root "$OUTDIR/page2_budgets.png" 2>/dev/null

# Page 3
"$INPUT" r 100
sleep 0.5
echo "Capturing page 3: Per-room budgets"
"$IMPORT" -window root "$OUTDIR/page3_room_budgets.png" 2>/dev/null

# Page 4
"$INPUT" r 100
sleep 0.5
echo "Capturing page 4: View distances"
"$IMPORT" -window root "$OUTDIR/page4_view_dist.png" 2>/dev/null

# Page 5
"$INPUT" r 100
sleep 0.5
echo "Capturing page 5: Room visibility"
"$IMPORT" -window root "$OUTDIR/page5_visibility.png" 2>/dev/null

# Page 6 (Claude: T3D Pipeline Status)
"$INPUT" r 100
sleep 0.5
echo "Capturing page 6: T3D pipeline"
"$IMPORT" -window root "$OUTDIR/page6_t3d.png" 2>/dev/null

# Page 7 (Claude: Timing Waterfall)
"$INPUT" r 100
sleep 0.5
echo "Capturing page 7: Timing waterfall"
"$IMPORT" -window root "$OUTDIR/page7_waterfall.png" 2>/dev/null

# Page 8 (Claude: Performance Targets)
"$INPUT" r 100
sleep 0.5
echo "Capturing page 8: Performance targets"
"$IMPORT" -window root "$OUTDIR/page8_targets.png" 2>/dev/null

echo ""
echo "=== Capture Complete ==="
echo "Screenshots: $OUTDIR/"
ls -la "$OUTDIR/"
echo ""

# Print debug log
if [ -s "$LOG" ]; then
    echo "=== Debug Output ==="
    head -100 "$LOG"
fi
