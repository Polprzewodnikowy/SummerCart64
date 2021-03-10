#!/bin/bash

PACKAGES_FOLDER_NAME="packages"
PACKAGE_FILE_NAME="SummerCart64"
FILES=(
    "./fw/output_files/SummerCart64.pof"
    "./hw/v1/ftdi-template.xml"
    "./sw/cic/UltraCIC-III.hex"
    "./sw/cic/UltraCIC-III.eep.hex"
)


# Add version to zip file name if provided
if [[ $1 ]]; then
    PACKAGE_FILE_NAME="${PACKAGE_FILE_NAME}-${1}"
fi


# Build libsc64
echo "Building libsc64"
pushd sw/libsc64
./build.sh
popd


# Build bootloader
echo "Building bootloader"
pushd sw/bootloader
./build.sh
popd


# Build UltraCIC-III
pushd sw/cic
echo "Building UltraCIC-III"
avra UltraCIC-III.asm -D attiny45
popd


# Build FPGA firmware
echo "Building FPGA firmware"
docker run -t --mount type=bind,src="$(pwd)",target="/build" chriz2600/quartus-lite:20.1.0 /usr/local/bin/quartus_wrapper quartus_sh --flow compile /build/fw/SummerCart64.qpf


# Create packages directory
echo "Creating ${PACKAGES_FOLDER_NAME} directory"
mkdir -p "${PACKAGES_FOLDER_NAME}"


# ZIP files for release
echo "Zipping files"
if [[ -e "${PACKAGES_FOLDER_NAME}/${PACKAGE_FILE_NAME}.zip" ]]; then
    rm -f "${PACKAGES_FOLDER_NAME}/${PACKAGE_FILE_NAME}.zip"
fi
zip -r "${PACKAGES_FOLDER_NAME}/${PACKAGE_FILE_NAME}.zip" ${FILES[@]}
