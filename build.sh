#!/bin/bash

PACKAGE_FILE_NAME="SummerCart64"

if [ $1 ]; then
    PACKAGE_FILE_NAME="${1}"
fi

# Build bootloader
echo "Pulling libdragon"
docker pull anacierdem/libdragon:latest
echo "Building bootloader"
docker run -t --mount type=bind,src="$(pwd)/sw/bootloader",target="/libdragon" anacierdem/libdragon:latest /usr/bin/make N64_BYTE_SWAP=false

# Build UltraCIC-III
pushd sw/cic
echo "Building UltraCIC-III"
avra UltraCIC-III.asm -D attiny45
popd

# Build FPGA firmware
echo "Pulling Quartus Lite"
docker pull chriz2600/quartus-lite:20.1.0
echo "Building FPGA firmware"
docker run -t --mount type=bind,src="$(pwd)",target="/build" chriz2600/quartus-lite:20.1.0 /usr/local/bin/quartus_wrapper quartus_sh --flow compile /build/fw/SummerCart64.qpf

FILES=(
    "./fw/output_files/SummerCart64.pof"
    "./fw/output_files/SummerCart64.sof"
    "./fw/output_files/SummerCart64_pof.svf"
    "./fw/output_files/SummerCart64.svf"
    "./hw/ftdi-template-release.xml"
    "./sw/cic/UltraCIC-III.hex"
)

# Generate Gerber files (only local, no docker image for Eagle exists)
if [ !$CI ]; then
    pushd hw
    if [ -e CAMOutputs ]; then
        rm -r CAMOutputs
    fi
    eaglecon.exe -X -dCAMJOB -jSummerCart64.cam SummerCart64.brd
    FILES+=("./hw/CAMOutputs")
    popd
fi

# ZIP files for release
zip -r ${PACKAGE_FILE_NAME}.zip ${FILES[@]}
