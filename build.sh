#!/bin/bash

set -e

PACKAGE_FILE_NAME="SummerCart64"

FILES=(
    "./fw/output_files/SC64_firmware.pof"
    "./fw/output_files/SC64_update.bin"
    "./hw/ftdi-template.xml"
    "./sw/cic/UltraCIC-III.hex"
    "./LICENSE"
)

BUILT_CIC=false
BUILT_N64=false
BUILT_RISCV=false
BUILT_SW=false
BUILT_FPGA=false
BUILT_UPDATE=false
BUILT_RELEASE=false

FORCE_CLEAN=false
SKIP_FPGA_REBUILD=false
DEBUG_ENABLED=false

build_cic () {
    if [ "$BUILT_CIC" = true ]; then return; fi

    pushd sw/cic > /dev/null
    avra UltraCIC-III.asm -D attiny45
    popd > /dev/null

    BUILT_CIC=true
}

build_n64 () {
    if [ "$BUILT_N64" = true ]; then return; fi

    pushd sw/n64 > /dev/null
    if [ "$FORCE_CLEAN" = true ]; then
        make clean
    fi
    N64_FLAGS="$USER_FLAGS"
    if [ ! -z "${GIT_BRANCH+x}" ]; then N64_FLAGS+=" -DGIT_BRANCH='\"$GIT_BRANCH\"'"; fi
    if [ ! -z "${GIT_TAG+x}" ]; then N64_FLAGS+=" -DGIT_TAG='\"$GIT_TAG\"'"; fi
    if [ ! -z "${GIT_SHA+x}" ]; then N64_FLAGS+=" -DGIT_SHA='\"$GIT_SHA\"'"; fi
    make all -j USER_FLAGS="$N64_FLAGS"
    popd > /dev/null

    BUILT_N64=true
}

build_riscv () {
    if [ "$BUILT_RISCV" = true ]; then return; fi

    pushd sw/riscv > /dev/null
    if [ "$FORCE_CLEAN" = true ]; then
        make clean
    fi
    make all -j USER_FLAGS="$USER_FLAGS"
    popd > /dev/null

    BUILT_RISCV=true
}

build_sw () {
    if [ "$BUILT_SW" = true ]; then return; fi

    build_n64
    build_riscv

    pushd fw > /dev/null
    mkdir -p output_files > /dev/null
    cat ../sw/n64/build/n64boot.bin ../sw/riscv/build/governor.bin > output_files/SC64_software.bin
    objcopy -I binary -O ihex output_files/SC64_software.bin output_files/SC64_software.hex
    popd

    BUILT_SW=true
}

build_fpga () {
    if [ "$BUILT_FPGA" = true ]; then return; fi

    build_sw

    pushd fw > /dev/null
    if [ "$SKIP_FPGA_REBUILD" = true ] && [ -f output_files/SummerCart64.sof ]; then
        quartus_cpf -c SummerCart64.cof
    else
        if [ "$DEBUG_ENABLED" = true ]; then
            quartus_sh --set VERILOG_MACRO="DEBUG" ./SummerCart64.qpf
        else
            quartus_sh --set -remove VERILOG_MACRO="DEBUG" ./SummerCart64.qpf
        fi
        quartus_sh --flow compile ./SummerCart64.qpf
    fi
    popd > /dev/null

    BUILT_FPGA=true
}

build_update () {
    if [ "$BUILT_UPDATE" = true ]; then return; fi

    build_fpga

    pushd fw/output_files > /dev/null
    objcopy -I binary -O binary --reverse-bytes=4 sc64_firmware_cfm0_auto.rpd SC64_firmware.bin
    cat SC64_software.bin SC64_firmware.bin > SC64_update.bin
    popd > /dev/null

    BUILT_UPDATE=true
}

build_release () {
    if [ "$BUILT_RELEASE" = true ]; then return; fi

    build_cic
    build_update

    if [ -e "./${PACKAGE_FILE_NAME}.zip" ]; then
        rm -f "./${PACKAGE_FILE_NAME}.zip"
    fi
    zip -j -r "./${PACKAGE_FILE_NAME}.zip" ${FILES[@]}

    BUILT_RELEASE=true
}

print_usage () {
    echo "builder script for SummerCart64"
    echo "usage: ./build.sh [cic] [n64] [riscv] [fpga] [update] [release] [-c] [-s] [-d] [--help]"
    echo "parameters:"
    echo "  cic       - assemble UltraCIC-III software"
    echo "  n64       - compile N64 bootloader software"
    echo "  riscv     - compile cart governor software"
    echo "  sw        - compile all software (triggers 'n64' and 'riscv' build)"
    echo "  fpga      - compile FPGA design (triggers 'sw' build)"
    echo "  update    - convert programming .pof file to raw binary for self-upgrade (triggers 'fpga' build)"
    echo "  release   - collect and zip files for release (triggers 'cic' and 'update' build)"
    echo "  -c | --force-clean"
    echo "            - clean software compilation result directories before build"
    echo "  -s | --skip-fpga-rebuild"
    echo "            - do not recompile whole FPGA design if it's already done, just update software binaries"
    echo "  -d | --debug"
    echo "            - enable debug features"
    echo "  --help    - print this guide"
}

if test $# -eq 0; then
    echo "error: no parameters provided"
    echo " "
    print_usage
    exit 1
fi

TRIGGER_CIC=false
TRIGGER_N64=false
TRIGGER_RISCV=false
TRIGGER_SW=false
TRIGGER_FPGA=false
TRIGGER_UPDATE=false
TRIGGER_RELEASE=false

while test $# -gt 0; do
    case "$1" in
        cic)
            TRIGGER_CIC=true
            ;;
        n64)
            TRIGGER_N64=true
            ;;
        riscv)
            TRIGGER_RISCV=true
            ;;
        sw)
            TRIGGER_SW=true
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
        -s|--skip-fpga-rebuild)
            SKIP_FPGA_REBUILD=true
            ;;
        -d|--debug)
            DEBUG_ENABLED=true
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

if [ "$DEBUG_ENABLED" = true ]; then USER_FLAGS+=" -DDEBUG"; fi
if [ "$TRIGGER_CIC" = true ]; then build_cic; fi
if [ "$TRIGGER_N64" = true ]; then build_n64; fi
if [ "$TRIGGER_RISCV" = true ]; then build_riscv; fi
if [ "$TRIGGER_SW" = true ]; then build_sw; fi
if [ "$TRIGGER_FPGA" = true ]; then build_fpga; fi
if [ "$TRIGGER_UPDATE" = true ]; then build_update; fi
if [ "$TRIGGER_RELEASE" = true ]; then build_release; fi
