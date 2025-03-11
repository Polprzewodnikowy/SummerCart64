#!/bin/bash

set -e

SC64_VERSION=${SC64_VERSION:-"none"}

PACKAGE_FILE_NAME="sc64-extra"

TOP_FILES=(
    "./fw/ftdi/ft232h_config.xml"
    "./fw/project/lcmxo2/impl1/fpga_max_frequency.txt"
    "./sw/tools/primer.py"
    "./sw/tools/requirements.txt"
    "./sc64-firmware-${SC64_VERSION}.bin"
)

FILES=(
    "./assets/*"
    "./docs/*"
    "./hw/pcb/LICENSE"
    "./hw/pcb/sc64v2_bom.html"
    "./hw/pcb/sc64v2.kicad_pcb"
    "./hw/pcb/sc64v2.kicad_pro"
    "./hw/pcb/sc64v2.kicad_sch"
    "./hw/shell/sc64_shell_back.stl"
    "./hw/shell/sc64_shell_front.stl"
    "./LICENSE"
    "./README.md"
)

HAVE_COMMIT_INFO=false

BUILT_BOOTLOADER=false
BUILT_CONTROLLER=false
BUILT_CIC=false
BUILT_FPGA=false
BUILT_UPDATE=false
BUILT_RELEASE=false

FORCE_CLEAN=false

get_last_commit_info () {
    if [ "$HAVE_COMMIT_INFO" = true ]; then return; fi

    SAFE_DIRECTORY="-c safe.directory=$(pwd)"

    GIT_BRANCH=$(git $SAFE_DIRECTORY rev-parse --abbrev-ref HEAD)
    GIT_TAG=$(git $SAFE_DIRECTORY describe --tags 2> /dev/null)
    GIT_SHA=$(git $SAFE_DIRECTORY rev-parse HEAD)
    GIT_MESSAGE=$(git $SAFE_DIRECTORY log --oneline --format=%B -n 1 HEAD | head -n 1)

    HAVE_COMMIT_INFO=true
}

build_bootloader () {
    if [ "$BUILT_BOOTLOADER" = true ]; then return; fi

    get_last_commit_info

    pushd sw/bootloader > /dev/null
    if [ "$FORCE_CLEAN" = true ]; then
        make clean
    fi
    VERSION=""
    if [ ! -z "${GIT_BRANCH+x}" ]; then VERSION+=" -DGIT_BRANCH='\"$GIT_BRANCH\"'"; fi
    if [ ! -z "${GIT_TAG+x}" ]; then VERSION+=" -DGIT_TAG='\"$GIT_TAG\"'"; fi
    if [ ! -z "${GIT_SHA+x}" ]; then VERSION+=" -DGIT_SHA='\"$GIT_SHA\"'"; fi
    if [ ! -z "${GIT_MESSAGE+x}" ]; then VERSION+=" -DGIT_MESSAGE='\"$GIT_MESSAGE\"'"; fi
    make all -j VERSION="$VERSION" USER_FLAGS="$USER_FLAGS"
    popd > /dev/null

    BUILT_BOOTLOADER=true
}

build_controller () {
    if [ "$BUILT_CONTROLLER" = true ]; then return; fi

    pushd sw/controller > /dev/null
    if [ "$FORCE_CLEAN" = true ]; then
        ./build.sh clean
    fi
    USER_FLAGS="$USER_FLAGS" ./build.sh all
    popd > /dev/null

    BUILT_CONTROLLER=true
}

build_cic () {
    if [ "$BUILT_CIC" = true ]; then return; fi

    pushd sw/cic > /dev/null
    if [ "$FORCE_CLEAN" = true ]; then
        ./build.sh clean
    fi
    ./build.sh all
    popd > /dev/null

    BUILT_CIC=true
}

build_fpga () {
    if [ "$BUILT_FPGA" = true ]; then return; fi

    build_cic

    pushd fw/project/lcmxo2 > /dev/null
    if [ "$FORCE_CLEAN" = true ]; then
        rm -rf ./impl1/
    fi
    ./build.sh
    popd > /dev/null

    BUILT_FPGA=true
}

build_update () {
    if [ "$BUILT_UPDATE" = true ]; then return; fi

    get_last_commit_info

    build_bootloader
    build_controller
    build_cic
    build_fpga

    pushd sw/tools > /dev/null
    if [ "$FORCE_CLEAN" = true ]; then
        rm -f ../../sc64-firmware-*.bin
    fi
    GIT_INFO=""
    GIT_INFO+=$'\n'"freq: $(cat ../../fw/project/lcmxo2/impl1/fpga_max_frequency.txt)"
    if [ ! -z "${SC64_VERSION}" ]; then GIT_INFO+=$'\n'"ver: $SC64_VERSION"; fi
    if [ ! -z "${GIT_BRANCH}" ]; then GIT_INFO+=$'\n'"branch: $GIT_BRANCH"; fi
    if [ ! -z "${GIT_TAG}" ]; then GIT_INFO+=$'\n'"tag: $GIT_TAG"; fi
    if [ ! -z "${GIT_SHA}" ]; then GIT_INFO+=$'\n'"sha: $GIT_SHA"; fi
    if [ ! -z "${GIT_MESSAGE}" ]; then GIT_INFO+=$'\n'"msg: $GIT_MESSAGE"; fi
    python3 update.py \
        --git "$GIT_INFO" \
        --mcu ../controller/build/app/app.bin \
        --fpga ../../fw/project/lcmxo2/impl1/sc64_impl1.jed \
        --boot ../bootloader/build/bootloader.bin \
        --primer ../controller/build/primer/primer.bin \
        ../../sc64-firmware-${SC64_VERSION}.bin
    popd > /dev/null

    BUILT_UPDATE=true
}

build_release () {
    if [ "$BUILT_RELEASE" = true ]; then return; fi

    build_update

    if [ -e "./${PACKAGE_FILE_NAME}-${SC64_VERSION}.zip" ]; then
        rm -f ./${PACKAGE_FILE_NAME}-${SC64_VERSION}.zip
    fi

    PACKAGE="./${PACKAGE_FILE_NAME}-${SC64_VERSION}.zip"
    zip -j -r $PACKAGE ${TOP_FILES[@]}
    zip -r $PACKAGE ${FILES[@]}

    BUILT_RELEASE=true
}

print_usage () {
    echo "builder script for SC64"
    echo "usage: ./build.sh [bootloader] [controller] [cic] [fpga] [update] [release] [-c] [--help]"
    echo "parameters:"
    echo "  bootloader  - compile N64 bootloader software"
    echo "  controller  - compile MCU controller software"
    echo "  cic         - compile CIC emulation software"
    echo "  fpga        - compile FPGA design"
    echo "  update      - compile all software and designs"
    echo "  release     - collect and zip files for release (triggers 'update' build)"
    echo "  -c | --force-clean"
    echo "              - clean compilation result directories before build"
    echo "  --help      - print this guide"
}

if test $# -eq 0; then
    echo "error: no parameters provided"
    echo " "
    print_usage
    exit 1
fi

print_time () {
    echo "Build took $SECONDS seconds"
}

trap "echo \"Build failed\"; print_time" ERR

SECONDS=0

TRIGGER_BOOTLOADER=false
TRIGGER_CONTROLLER=false
TRIGGER_CIC=false
TRIGGER_FPGA=false
TRIGGER_UPDATE=false
TRIGGER_RELEASE=false

while test $# -gt 0; do
    case "$1" in
        bootloader)
            TRIGGER_BOOTLOADER=true
            ;;
        controller)
            TRIGGER_CONTROLLER=true
            ;;
        cic)
            TRIGGER_CIC=true
            ;;
        fpga)
            TRIGGER_FPGA=true
            ;;
        update)
            TRIGGER_UPDATE=true
            ;;
        release)
            TRIGGER_RELEASE=true
            ;;
        -c|--force-clean)
            FORCE_CLEAN=true
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "error: unknown parameter \"$1\""
            echo " "
            print_usage
            exit 1
            ;;
    esac
    shift
done

if [ "$TRIGGER_BOOTLOADER" = true ]; then build_bootloader; fi
if [ "$TRIGGER_CONTROLLER" = true ]; then build_controller; fi
if [ "$TRIGGER_CIC" = true ]; then build_cic; fi
if [ "$TRIGGER_FPGA" = true ]; then build_fpga; fi
if [ "$TRIGGER_UPDATE" = true ]; then build_update; fi
if [ "$TRIGGER_RELEASE" = true ]; then build_release; fi

print_time
