#!/bin/bash

PACKAGES_FOLDER_NAME="packages"
PACKAGE_FILE_NAME="SummerCart64_PCB"
FILES=(
    "./hw/CAMOutputs"
    # Manually created files
    "./hw/SummerCart64_sch.pdf"
    "./hw/SummerCart64_brd_top.pdf"
    "./hw/SummerCart64_brd_bot.pdf"
    "./hw/SummerCart64_brd_place_top.pdf"
)


# Add version to zip file name if provided
if [[ $1 ]]; then
    PACKAGE_FILE_NAME="${PACKAGE_FILE_NAME}-${1}"
fi


# Generate Gerbers
pushd hw
if [[ -e CAMOutputs ]]; then
    rm -rf CAMOutputs
fi
echo "Generating Gerbers"
eaglecon.exe -X -dCAMJOB -jSummerCart64.cam SummerCart64.brd
popd


# Create packages directory
echo "Creating ${PACKAGES_FOLDER_NAME} directory"
mkdir -p "${PACKAGES_FOLDER_NAME}"


# ZIP files for release
echo "Zipping PCB files"
if [[ -e "${PACKAGES_FOLDER_NAME}/${PACKAGE_FILE_NAME}.zip" ]]; then
    rm -f "${PACKAGES_FOLDER_NAME}/${PACKAGE_FILE_NAME}.zip"
fi
zip -r "${PACKAGES_FOLDER_NAME}/${PACKAGE_FILE_NAME}.zip" ${FILES[@]}
