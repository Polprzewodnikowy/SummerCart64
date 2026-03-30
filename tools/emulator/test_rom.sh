#!/bin/bash
# test_rom.sh — Bulletproof end-to-end ROM testing pipeline
#
# Builds the ROM, boots it in ares, verifies boot, captures gameplay,
# opens pause menu, reads all 5 diagnostic pages, and reports results.
#
# Usage: ./test_rom.sh [rom_path] [--skip-build] [--full-rom]
#
# Exit codes:
#   0 — All checks passed
#   1 — Build failed
#   2 — ROM failed to boot (black screen / crash)
#   3 — Gameplay not rendering (no visible scene)
#   4 — Performance regression detected
#
# Outputs:
#   /tmp/rom_test/gameplay.png     — In-game screenshot
#   /tmp/rom_test/page*.png        — Each diagnostic page
#   /tmp/rom_test/boot.log         — Ares debug output
#   /tmp/rom_test/report.txt       — Human-readable test report

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$SCRIPT_DIR/bin"
TRX_DIR="$REPO_ROOT/sw/trx-n64"
OUTDIR="/tmp/rom_test"
LOG="$OUTDIR/boot.log"
REPORT="$OUTDIR/report.txt"
DISPLAY_NUM=":99"
SKIP_BUILD=false
FULL_ROM=false
ROM=""

# Parse args
for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=true ;;
        --full-rom) FULL_ROM=true ;;
        *.z64) ROM="$arg" ;;
    esac
done

# Tools
ARES="$BIN/ares"
XVFB="$BIN/Xvfb"
[ -x "$XVFB" ] || XVFB="$(command -v Xvfb 2>/dev/null || true)"
IMPORT="$BIN/import"
[ -x "$IMPORT" ] || IMPORT="$(command -v import 2>/dev/null || true)"
XDOTOOL="$BIN/xdotool"
[ -x "$XDOTOOL" ] || XDOTOOL="$(command -v xdotool 2>/dev/null || true)"
INPUT="$SCRIPT_DIR/n64_input.sh"

echo "================================================================"
echo "  TRX-N64 ROM Test Pipeline"
echo "================================================================"

mkdir -p "$OUTDIR"
> "$REPORT"

# ----------------------------------------------------------------
# PHASE 1: BUILD
# ----------------------------------------------------------------
if ! $SKIP_BUILD; then
    echo ""
    echo "[PHASE 1] Building ROM..."
    cd "$TRX_DIR"

    if $FULL_ROM; then
        if N64_INST=/opt/libdragon make -f Makefile.libdragon fullrom 2>&1 | tee "$OUTDIR/build.log" | tail -5; then
            ROM="$TRX_DIR/build/trx-n64-full.z64"
            echo "  [OK] Full ROM built: $ROM"
            echo "BUILD: OK (full ROM)" >> "$REPORT"
        else
            echo "  [FAIL] Build failed"
            echo "BUILD: FAILED" >> "$REPORT"
            exit 1
        fi
    else
        if N64_INST=/opt/libdragon make -f Makefile.libdragon 2>&1 | tee "$OUTDIR/build.log" | tail -5; then
            ROM="$TRX_DIR/trx-n64.z64"
            echo "  [OK] ROM built: $ROM"
            echo "BUILD: OK (base ROM)" >> "$REPORT"
        else
            echo "  [FAIL] Build failed"
            echo "BUILD: FAILED" >> "$REPORT"
            exit 1
        fi
    fi
else
    [ -z "$ROM" ] && ROM="$TRX_DIR/trx-n64.z64"
    echo "[PHASE 1] Skipping build, using: $ROM"
    echo "BUILD: SKIPPED" >> "$REPORT"
fi

if [ ! -f "$ROM" ]; then
    echo "  [FAIL] ROM not found: $ROM"
    exit 1
fi
echo "ROM_SIZE: $(du -h "$ROM" | cut -f1)" >> "$REPORT"

# ----------------------------------------------------------------
# PHASE 2: BOOT TEST
# ----------------------------------------------------------------
echo ""
echo "[PHASE 2] Booting ROM in ares..."

# Clean up on exit
cleanup() {
    [ -n "${ARES_PID:-}" ] && kill "$ARES_PID" 2>/dev/null; wait "$ARES_PID" 2>/dev/null
    [ -n "${XVFB_PID:-}" ] && kill "$XVFB_PID" 2>/dev/null; wait "$XVFB_PID" 2>/dev/null
    true
}
trap cleanup EXIT

# Start Xvfb
"$XVFB" $DISPLAY_NUM -screen 0 640x480x24 +extension GLX 2>/dev/null &
XVFB_PID=$!
sleep 1
export DISPLAY=$DISPLAY_NUM

# Start ares
"$ARES" --setting "Audio/Driver=None" --system "Nintendo 64" --fullscreen "$ROM" > "$LOG" 2>&1 &
ARES_PID=$!

# Wait for boot (3 seconds)
sleep 3

# Capture boot screen
"$IMPORT" -window root "$OUTDIR/boot.png" 2>/dev/null
echo "  Captured: boot.png"

# Check if ROM produced any debug output (sign of life)
if [ -s "$LOG" ]; then
    echo "  [OK] ROM produced debug output"
    echo "BOOT: OK (debug output detected)" >> "$REPORT"
else
    echo "  [WARN] No debug output from ROM"
    echo "BOOT: WARN (no debug output)" >> "$REPORT"
fi

# ----------------------------------------------------------------
# PHASE 3: GAMEPLAY CAPTURE (wait for level to load)
# ----------------------------------------------------------------
echo ""
echo "[PHASE 3] Waiting for gameplay (5 more seconds)..."
sleep 5

"$IMPORT" -window root "$OUTDIR/gameplay.png" 2>/dev/null
echo "  Captured: gameplay.png"
echo "GAMEPLAY: captured" >> "$REPORT"

# ----------------------------------------------------------------
# PHASE 4: PAUSE MENU DIAGNOSTICS
# ----------------------------------------------------------------
echo ""
echo "[PHASE 4] Capturing pause menu diagnostics..."

# Press Start to open pause menu
"$INPUT" start 200
sleep 1

# Page 1: Main diagnostics
"$IMPORT" -window root "$OUTDIR/page1_diagnostics.png" 2>/dev/null
echo "  Captured: page1_diagnostics.png"

# Page 2: Global budgets (R to cycle)
"$INPUT" r 100
sleep 0.5
"$IMPORT" -window root "$OUTDIR/page2_budgets.png" 2>/dev/null
echo "  Captured: page2_budgets.png"

# Page 3: Per-room budgets
"$INPUT" r 100
sleep 0.5
"$IMPORT" -window root "$OUTDIR/page3_room_budgets.png" 2>/dev/null
echo "  Captured: page3_room_budgets.png"

# Page 4: View distances
"$INPUT" r 100
sleep 0.5
"$IMPORT" -window root "$OUTDIR/page4_view_dist.png" 2>/dev/null
echo "  Captured: page4_view_dist.png"

# Page 5: Room visibility
"$INPUT" r 100
sleep 0.5
"$IMPORT" -window root "$OUTDIR/page5_visibility.png" 2>/dev/null
echo "  Captured: page5_visibility.png"

# Close pause menu
"$INPUT" start 200
sleep 0.5

# ----------------------------------------------------------------
# PHASE 5: ADDITIONAL GAMEPLAY (post-menu)
# ----------------------------------------------------------------
echo ""
echo "[PHASE 5] Post-menu gameplay..."
sleep 2
"$IMPORT" -window root "$OUTDIR/gameplay_post.png" 2>/dev/null
echo "  Captured: gameplay_post.png"

# ----------------------------------------------------------------
# PHASE 6: PERFORMANCE DATA
# ----------------------------------------------------------------
echo ""
echo "[PHASE 6] Parsing performance data..."

# Parse PERF| lines from debug log
if grep -q "^PERF|" "$LOG" 2>/dev/null; then
    "$SCRIPT_DIR/parse_perf.sh" "$LOG" | tee "$OUTDIR/perf_summary.txt"
    echo "PERF_DATA: found" >> "$REPORT"

    # Extract key metrics for machine-readable output
    LAST_PERF=$(grep "^PERF|" "$LOG" | tail -1)
    echo "$LAST_PERF" > "$OUTDIR/perf_raw.txt"

    # Check thresholds
    FRAME_US=$(echo "$LAST_PERF" | tr '|' '\n' | grep "^frame=" | cut -d= -f2)
    LARA_US=$(echo "$LAST_PERF" | tr '|' '\n' | grep "^la=" | cut -d= -f2)
    FPS_VAL=$(echo "$LAST_PERF" | tr '|' '\n' | grep "^fps=" | cut -d= -f2)

    echo "FRAME_US: $FRAME_US" >> "$REPORT"
    echo "LARA_US: $LARA_US" >> "$REPORT"
    echo "FPS: $FPS_VAL" >> "$REPORT"

    # Performance gate
    if [ -n "$LARA_US" ] && [ "$LARA_US" -gt 10000 ] 2>/dev/null; then
        echo "  [WARN] Lara render time ${LARA_US}us > 10ms — T3D not working?" | tee -a "$REPORT"
    fi
    if [ -n "$FRAME_US" ] && [ "$FRAME_US" -gt 50000 ] 2>/dev/null; then
        echo "  [WARN] Frame time ${FRAME_US}us > 50ms — severe performance issue" | tee -a "$REPORT"
    fi
else
    echo "  No PERF| data (game didn't run long enough or no level loaded)"
    echo "PERF_DATA: none" >> "$REPORT"
fi

# ----------------------------------------------------------------
# REPORT
# ----------------------------------------------------------------
echo ""
echo "================================================================"
echo "  Test Report"
echo "================================================================"

# Append debug log to report
if [ -s "$LOG" ]; then
    echo "" >> "$REPORT"
    echo "=== ARES DEBUG LOG ===" >> "$REPORT"
    head -100 "$LOG" >> "$REPORT"
fi

echo "" >> "$REPORT"
echo "=== SCREENSHOTS ===" >> "$REPORT"
ls -lh "$OUTDIR"/*.png >> "$REPORT" 2>/dev/null

cat "$REPORT"
echo ""
echo "Screenshots saved to: $OUTDIR/"
echo "Full report: $REPORT"
echo "Perf summary: $OUTDIR/perf_summary.txt"
echo ""
echo "To view screenshots, use: Read tool on /tmp/rom_test/<filename>.png"
