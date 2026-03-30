#!/bin/bash
# n64_input.sh — Send N64 controller inputs to ares via xdotool
#
# Usage: ./n64_input.sh <button> [hold_ms]
#   button  — one of: start, a, b, z, l, r, cup, cdown, cleft, cright,
#                       dup, ddown, dleft, dright, up, down, left, right
#   hold_ms — how long to hold the button (default: 100ms)
#
# Ares default keyboard mapping (N64):
#   A=X, B=Z/S, Start=Enter, Z-trigger=A, L=Q, R=W
#   D-pad=Arrow keys, C-buttons=I/K/J/L, Analog=Arrow keys (same)
#
# Requires DISPLAY to be set (e.g., DISPLAY=:99 for Xvfb)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XDOTOOL="$SCRIPT_DIR/bin/xdotool"
[ -x "$XDOTOOL" ] || XDOTOOL="$(command -v xdotool 2>/dev/null)"

BUTTON="${1:?Usage: $0 <button> [hold_ms]}"
HOLD_MS="${2:-100}"

# Map N64 buttons to ares default keyboard keys
case "$BUTTON" in
    start)   KEY="Return" ;;
    a)       KEY="x" ;;
    b)       KEY="z" ;;
    z)       KEY="a" ;;
    l)       KEY="q" ;;
    r)       KEY="w" ;;
    # C-buttons
    cup)     KEY="i" ;;
    cdown)   KEY="k" ;;
    cleft)   KEY="j" ;;
    cright)  KEY="l" ;;
    # D-pad
    dup)     KEY="Up" ;;
    ddown)   KEY="Down" ;;
    dleft)   KEY="Left" ;;
    dright)  KEY="Right" ;;
    # Analog stick (same as d-pad in default mapping)
    up)      KEY="Up" ;;
    down)    KEY="Down" ;;
    left)    KEY="Left" ;;
    right)   KEY="Right" ;;
    *)
        echo "Unknown button: $BUTTON"
        echo "Valid: start a b z l r cup cdown cleft cright dup ddown dleft dright up down left right"
        exit 1
        ;;
esac

# Send keypress
HOLD_SEC=$(echo "scale=3; $HOLD_MS / 1000" | bc)
"$XDOTOOL" keydown "$KEY"
sleep "$HOLD_SEC"
"$XDOTOOL" keyup "$KEY"
