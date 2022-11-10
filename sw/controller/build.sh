#!/bin/bash

set -e

case "$1" in
    all)
        make all -j -f loader.mk USER_FLAGS="$USER_FLAGS"
        make all -j -f app.mk USER_FLAGS="$USER_FLAGS"
        ;;
    clean)
        make clean -f loader.mk
        make clean -f app.mk
        ;;
esac
