#!/bin/bash

set -e

TOOLCHAIN="riscv32-unknown-elf-"
CFLAGS="-ffreestanding -nostartfiles -Os"

case "$1" in
    all)
        mkdir -p ./build
        ${TOOLCHAIN}gcc $CFLAGS -T cic.ld -o ./build/cic.elf startup.S cic.c
        echo "Size of cic:"
        ${TOOLCHAIN}size -B -d ./build/cic.elf
        ${TOOLCHAIN}objdump -S -D ./build/cic.elf > ./build/cic.lst
        ${TOOLCHAIN}objcopy -O binary ./build/cic.elf ./build/cic.bin
        python3 ./convert.py ./build/cic.bin ./build/cic.mem
        ;;
    clean)
        rm -rf ./build/*
        ;;
esac
