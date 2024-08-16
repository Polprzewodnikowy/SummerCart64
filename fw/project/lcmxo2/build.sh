#!/bin/bash

set -e

source $bindir/diamond_env

printf "[$(hostname)]\nSYSTEM=Linux\nCORENUM=$(nproc --all)\n" > $(dirname $0)/multicore.txt

diamondc build.tcl

MINIMUM_FREQ=$(cat impl1/sc64_impl1.twr \
    | grep 'Preference: FREQUENCY NET "clk"' \
    | head -1 \
    | awk -F ' ' '{print $5}' \
)

DESIGN_FREQ=$(cat impl1/sc64_impl1.twr \
    | sed -n '/Preference: FREQUENCY NET "clk"/,$p' \
    | grep 'is the maximum frequency for this preference' \
    | head -1 \
    | awk -F ' ' '{print $2}' \
    | sed 's/MHz//' \
)

echo "Minimum required frequency: $MINIMUM_FREQ MHz"
echo "Maximum design frequency:   $DESIGN_FREQ MHz"

if (( $(echo "$DESIGN_FREQ < $MINIMUM_FREQ" | bc -l) )); then
    echo "Timing error detected, build failed"
    exit 1
fi

echo -n "$DESIGN_FREQ MHz" > impl1/fpga_max_frequency.txt
