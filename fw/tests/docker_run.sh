#!/bin/bash

pushd $(dirname $0) > /dev/null

docker run \
    -it \
    --rm \
    --user $(id -u):$(id -g) \
    -v "$(pwd)":/work \
    -v "$(pwd)/../rtl":/rtl \
    -e CCACHE_DIR=/tmp/ccache \
    --entrypoint /bin/bash \
    verilator/verilator:latest \
    -c "make -j"

BUILD_ERROR=$?

popd > /dev/null

if [ $BUILD_ERROR -ne 0 ]; then
    exit -1
fi
