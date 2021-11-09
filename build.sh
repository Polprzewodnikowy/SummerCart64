#!/bin/bash


PACKAGE_FILE_NAME="SummerCart64"
FILES=(
    "./fw/output_files/SC64_firmware.pof"
    "./fw/output_files/SC64_update.bin"
    "./hw/ftdi-template.xml"
    "./sw/cic/UltraCIC-III.hex"
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
quartus_cpf -c ./SummerCart64.cof
cp output_files/SC64_firmware.pof output_files/SC64_update.pof
cat output_files/sc64_firmware_ufm_auto.rpd output_files/sc64_firmware_cfm0_auto.rpd > output_files/SC64_update_LE.bin
riscv32-unknown-elf-objcopy -I binary -O binary --reverse-bytes=4 output_files/SC64_update_LE.bin output_files/SC64_update.bin
popd


echo "Zipping files"
if [[ -e "./${PACKAGE_FILE_NAME}.zip" ]]; then
    rm -f "./${PACKAGE_FILE_NAME}.zip"
fi
zip -r "./${PACKAGE_FILE_NAME}.zip" ${FILES[@]}
