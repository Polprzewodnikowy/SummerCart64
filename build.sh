#!/bin/bash

PACKAGE_FILE_NAME="SummerCart64"

if [ $1 ]; then
    PACKAGE_FILE_NAME="${PACKAGE_FILE_NAME}_${1}"
fi

# Build bootloader
docker pull anacierdem/libdragon:latest
docker run -t --mount src="$(pwd)/sw/bootloader",target="/libdragon",type=bind anacierdem/libdragon:latest /usr/bin/make N64_BYTE_SWAP=false

# Build FPGA firmware
docker pull chriz2600/quartus-lite:20.1.0
docker run -t --mount src="$(pwd)",target="/build",type=bind chriz2600/quartus-lite quartus_wrapper quartus_sh --flow compile /build/fw/SummerCart64.qpf

# ZIP files for release
zip -j ${PACKAGE_FILE_NAME}.zip ./fw/output_files/SummerCart64.pof ./fw/output_files/SummerCart64.sof ./fw/output_files/SummerCart64.svf
