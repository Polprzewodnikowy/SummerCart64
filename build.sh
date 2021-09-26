#!/bin/bash


PACKAGE_FILE_NAME="SummerCart64"
FILES=(
    "./fw/output_files/SummerCart64.pof"
    "./hw/v1/ftdi-template.xml"
    "./sw/cic/UltraCIC-III.hex"
    "./sw/riscv/build/controller.rom"
    "./LICENSE"
)


set -e


pushd sw/cic
echo "Building UltraCIC-III software"
avra UltraCIC-III.asm -D attiny45
popd


pushd sw/n64
echo "Building N64 bootloader software"
make clean all
popd


pushd sw/riscv
echo "Building RISC-V controller software"
make clean all
popd


pushd fw
echo "Building FPGA firmware"
quartus_sh --flow compile ./SummerCart64.qpf
popd


echo "Zipping files"
if [[ -e "./${PACKAGE_FILE_NAME}.zip" ]]; then
    rm -f "./${PACKAGE_FILE_NAME}.zip"
fi
zip -r "./${PACKAGE_FILE_NAME}.zip" ${FILES[@]}
