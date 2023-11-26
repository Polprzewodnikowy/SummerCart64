#!/bin/bash

set -e

TOOLCHAIN="riscv32-unknown-elf-"
CFLAGS=" \
    -march=rv32i \
    -mabi=ilp32 \
    -Os \
    -Wl,--gc-sections \
    -ffunction-sections \
    -fdata-sections \
    -ffreestanding \
    -nostartfiles \
    -nostdlib \
    -nodefaultlibs \
    -fno-builtin \
    -mcmodel=medany \
"

case "$1" in
    all)
        ${TOOLCHAIN}gcc $CFLAGS -T cic.ld -o cic.elf startup.S cic.c
        echo "Size of cic:"
        ${TOOLCHAIN}size -B -d cic.elf
        ${TOOLCHAIN}objdump -S -D cic.elf > cic.lst
        ${TOOLCHAIN}objcopy -O binary cic.elf cic.bin
        python3 ./convert.py cic.bin cic.mem
        ;;
    clean)
        rm -f cic.elf cic.lst cic.bin cic.mem
        ;;
esac
