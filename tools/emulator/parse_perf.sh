#!/bin/bash
# parse_perf.sh — Parse PERF|, ADVICE|, T3D|, TARGETS| from ares debug log
#
# Usage: ./parse_perf.sh [logfile] [--json]
#   Default logfile: /tmp/ares_test.log
#   --json: also write /tmp/rom_test/perf.json structured summary

LOG="${1:-/tmp/ares_test.log}"
JSON_OUT=false
[ "$2" = "--json" ] && JSON_OUT=true

if [ ! -f "$LOG" ]; then
    echo "No log file: $LOG"
    exit 1
fi

PERF_LINES=$(grep "^PERF|" "$LOG" 2>/dev/null)
ADVICE_LINES=$(grep "^ADVICE|" "$LOG" 2>/dev/null)

if [ -z "$PERF_LINES" ]; then
    echo "No PERF| lines found in $LOG"
    echo "(Game needs to run long enough to emit at least 60 frames)"
    exit 1
fi

echo "=== Performance Report ==="
echo ""

# Parse last PERF line
LAST=$(echo "$PERF_LINES" | tail -1)
pf() { echo "$LAST" | tr '|' '\n' | grep "^$1=" | cut -d= -f2; }

FPS=$(pf fps); FRAME=$(pf frame); TEX=$(pf tex); BKD=$(pf bkd); RSP=$(pf rsp)
RM=$(pf rm); SM=$(pf sm); LA=$(pf la); IT=$(pf it)
TTRI=$(pf ttri); TFACE=$(pf tface); BTRI=$(pf btri)
ROOMS=$(pf rooms); BKDROOMS=$(pf bkdrooms); T3D=$(pf t3d); HEAP=$(pf heap_free)

# 3-tier check helper
tier() {
    local val=$1 green=$2 yellow=$3 higher=$4 label=$5
    if [ "$higher" = "1" ]; then
        [ "$val" -ge "$green" ] 2>/dev/null && echo "  $label: PASS ($val)" && return
        [ "$val" -ge "$yellow" ] 2>/dev/null && echo "  $label: WARN ($val)" && return
        echo "  $label: FAIL ($val)"
    else
        [ "$val" -le "$green" ] 2>/dev/null && echo "  $label: PASS ($val)" && return
        [ "$val" -le "$yellow" ] 2>/dev/null && echo "  $label: WARN ($val)" && return
        echo "  $label: FAIL ($val)"
    fi
}

echo "--- Targets (Green/Yellow/Red) ---"
tier "${FRAME:-0}" 33333 45000 0 "Frame(us)"
tier "${FPS:-0}" 20 15 1 "FPS"
tier "${LA:-0}" 5000 10000 0 "Lara(us)"
tier "${T3D:-0}" 1 1 1 "T3D ready"
tier "${RM:-0}" 15000 25000 0 "Rooms(us)"
tier "${SM:-0}" 5000 8000 0 "StMesh(us)"
tier "${IT:-0}" 3000 6000 0 "Items(us)"
tier "${BKD:-0}" 10000 18000 0 "Baked(us)"
tier "${RSP:-0}" 4000 8000 0 "RSP(us)"
echo ""

echo "--- Timing Breakdown ---"
echo "  Frame:    ${FRAME}us"
echo "  Rooms:    ${RM}us"
echo "  StMesh:   ${SM}us"
echo "  Lara:     ${LA}us"
echo "  Items:    ${IT}us"
echo "  Baked:    ${BKD}us"
echo "  RSP:      ${RSP}us"
echo ""

echo "--- Budget Usage ---"
echo "  Tex tris:   $TTRI"
echo "  Tex faces:  $TFACE"
echo "  Baked tris: $BTRI"
echo "  Rooms:      tex=$ROOMS bkd=$BKDROOMS"
echo "  Heap free:  ${HEAP} bytes"
echo ""

# Show ADVICE if present
if [ -n "$ADVICE_LINES" ]; then
    LAST_ADVICE=$(echo "$ADVICE_LINES" | tail -1)
    BN=$(echo "$LAST_ADVICE" | tr '|' '\n' | grep "^bottleneck=" | cut -d= -f2)
    SG=$(echo "$LAST_ADVICE" | tr '|' '\n' | grep "^suggest=" | cut -d= -f2)
    echo "--- Bottleneck ---"
    echo "  Primary:  $BN"
    echo "  Suggest:  $SG"
    echo ""
fi

# History
COUNT=$(echo "$PERF_LINES" | wc -l)
if [ "$COUNT" -gt 1 ]; then
    echo "--- History ($COUNT samples) ---"
    echo "$PERF_LINES" | while read -r line; do
        fps=$(echo "$line" | tr '|' '\n' | grep "^fps=" | cut -d= -f2)
        frame=$(echo "$line" | tr '|' '\n' | grep "^frame=" | cut -d= -f2)
        la=$(echo "$line" | tr '|' '\n' | grep "^la=" | cut -d= -f2)
        echo "  fps=$fps frame=${frame}us la=${la}us"
    done
fi

# JSON output
if $JSON_OUT; then
    mkdir -p /tmp/rom_test
    BN_JSON="${BN:-none}"
    SG_JSON="${SG:-none}"
    cat > /tmp/rom_test/perf.json << ENDJSON
{
  "fps": ${FPS:-0},
  "frame_us": ${FRAME:-0},
  "lara_us": ${LA:-0},
  "rooms_us": ${RM:-0},
  "stmesh_us": ${SM:-0},
  "items_us": ${IT:-0},
  "baked_us": ${BKD:-0},
  "rsp_us": ${RSP:-0},
  "tex_tris": "${TTRI}",
  "bkd_tris": "${BTRI}",
  "t3d_ready": ${T3D:-0},
  "heap_free": ${HEAP:-0},
  "bottleneck": "${BN_JSON}",
  "suggest": "${SG_JSON}",
  "samples": ${COUNT}
}
ENDJSON
    echo ""
    echo "JSON written to /tmp/rom_test/perf.json"
fi
