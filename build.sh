#!/bin/bash

PACKAGE_FILE_NAME="SummerCart64"

if [ $1 ]; then
    PACKAGE_FILE_NAME="${1}"
fi

# Build bootloader
echo "Pulling libdragon"
docker pull anacierdem/libdragon:latest
echo "Building bootloader"
docker run -t --mount src="$(pwd)/sw/bootloader",target="/libdragon",type=bind anacierdem/libdragon:latest /usr/bin/make N64_BYTE_SWAP=false

# Build FPGA firmware
echo "Pulling Quartus Lite"
docker pull chriz2600/quartus-lite:20.1.0
echo "Building FPGA firmware"
docker run -t --mount src="$(pwd)",target="/build",type=bind chriz2600/quartus-lite quartus_wrapper quartus_sh --flow compile /build/fw/SummerCart64.qpf

# Build UltraCIC-III
pushd UltraCIC-III
echo "Building UltraCIC-III"
avra UltraCIC-III.asm -D attiny45
popd

# ZIP files for release
zip -j ${PACKAGE_FILE_NAME}.zip \
    ./fw/output_files/SummerCart64.sof \
    ./fw/output_files/SummerCart64.svf \
    ./fw/output_files/SummerCart64.pof \
    ./fw/output_files/SummerCart64_pof.svf \
    ./UltraCIC-III/UltraCIC-III.hex
