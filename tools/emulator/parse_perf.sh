#!/bin/bash
# parse_perf.sh — Parse PERF| lines from ares debug log
#
# Usage: ./parse_perf.sh [logfile]
#   Default logfile: /tmp/ares_test.log
#
# Output: Human-readable performance summary + raw values
# The game outputs PERF| lines every 60 frames (~1 second)
#
# Format: PERF|fps=N|frame=N|tex=N|bkd=N|rsp=N|rm=N|sm=N|la=N|it=N|ttri=N/N|tface=N/N|btri=N/N|rooms=N|bkdrooms=N

LOG="${1:-/tmp/ares_test.log}"

if [ ! -f "$LOG" ]; then
    echo "No log file: $LOG"
    exit 1
fi

PERF_LINES=$(grep "^PERF|" "$LOG" 2>/dev/null)
if [ -z "$PERF_LINES" ]; then
    echo "No PERF| lines found in $LOG"
    echo "(Game needs to run long enough to emit at least 60 frames)"
    exit 1
fi

echo "=== Performance Report ==="
echo ""

# Parse last PERF line for current state
LAST=$(echo "$PERF_LINES" | tail -1)

parse_field() {
    echo "$LAST" | tr '|' '\n' | grep "^$1=" | cut -d= -f2
}

FPS=$(parse_field fps)
FRAME=$(parse_field frame)
TEX=$(parse_field tex)
BKD=$(parse_field bkd)
RSP=$(parse_field rsp)
RM=$(parse_field rm)
SM=$(parse_field sm)
LA=$(parse_field la)
IT=$(parse_field it)
TTRI=$(parse_field ttri)
TFACE=$(parse_field tface)
BTRI=$(parse_field btri)
ROOMS=$(parse_field rooms)
BKDROOMS=$(parse_field bkdrooms)

echo "FPS:              $FPS"
echo "Frame time:       ${FRAME}us"
echo ""
echo "--- CPU Timing ---"
echo "  Textured pass:  ${TEX}us"
echo "    Rooms:        ${RM}us"
echo "    Static mesh:  ${SM}us"
echo "    Lara (T3D):   ${LA}us"
echo "    Items:        ${IT}us"
echo "  Baked pass:     ${BKD}us"
echo "  RSP stall:      ${RSP}us"
echo ""
echo "--- Budget Usage ---"
echo "  Tex tris:       $TTRI"
echo "  Tex faces:      $TFACE"
echo "  Baked tris:     $BTRI"
echo ""
echo "--- Scene ---"
echo "  Tex rooms:      $ROOMS"
echo "  Baked rooms:    $BKDROOMS"
echo ""

# Check for performance issues
FRAME_NUM=$(echo "$FRAME" | tr -d ' ')
if [ -n "$FRAME_NUM" ] && [ "$FRAME_NUM" -gt 33333 ] 2>/dev/null; then
    echo "WARNING: Frame time ${FRAME}us exceeds 33ms (below 30fps)"
fi
if [ -n "$FRAME_NUM" ] && [ "$FRAME_NUM" -gt 16667 ] 2>/dev/null; then
    echo "NOTE: Frame time ${FRAME}us exceeds 16.6ms (below 60fps)"
fi

LA_NUM=$(echo "$LA" | tr -d ' ')
if [ -n "$LA_NUM" ] && [ "$LA_NUM" -gt 5000 ] 2>/dev/null; then
    echo "WARNING: Lara time ${LA}us > 5ms — T3D may not be active"
fi

# Show history (all PERF lines)
COUNT=$(echo "$PERF_LINES" | wc -l)
if [ "$COUNT" -gt 1 ]; then
    echo ""
    echo "--- History ($COUNT samples) ---"
    echo "$PERF_LINES" | while read -r line; do
        fps=$(echo "$line" | tr '|' '\n' | grep "^fps=" | cut -d= -f2)
        frame=$(echo "$line" | tr '|' '\n' | grep "^frame=" | cut -d= -f2)
        la=$(echo "$line" | tr '|' '\n' | grep "^la=" | cut -d= -f2)
        echo "  fps=$fps frame=${frame}us la=${la}us"
    done
fi
