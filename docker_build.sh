#!/bin/bash

BUILDER_IMAGE="ghcr.io/polprzewodnikowy/sc64env:v1.10"
BUILDER_PLATFORM="linux/x86_64"

pushd $(dirname $0) > /dev/null

if [ -t 1 ]; then
    DOCKER_OPTIONS="-it"
fi
echo " < 1"
docker run \
    $DOCKER_OPTIONS \
    --rm \
    --user $(id -u):$(id -g) \
    --mac-address ${MAC_ADDRESS:-F8:12:34:56:78:90} \
    -v "$(pwd)"/fw/project/lcmxo2/license.dat:/flexlm/license.dat \
    -v "$(pwd)":/workdir \
    -h=`hostname` \
    -e SC64_VERSION=${SC64_VERSION:-""} \
    --platform $BUILDER_PLATFORM \
    $BUILDER_IMAGE \
    ./build.sh $@
BUILD_RESULT=$?
echo " < 2"
echo " error: $BUILD_RESULT"
popd > /dev/null
echo " < 3"
exit $BUILD_RESULT
