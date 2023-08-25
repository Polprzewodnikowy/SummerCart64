#!/bin/bash

BUILDER_IMAGE="ghcr.io/polprzewodnikowy/sc64env:v1.5"

pushd $(dirname $0) > /dev/null

GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_TAG=$(git describe --tags 2> /dev/null)
GIT_SHA=$(git rev-parse HEAD)
GIT_MESSAGE=$(git log --oneline --format=%B -n 1 HEAD | head -n 1)

if [ -t 1 ]; then
    DOCKER_OPTIONS="-it"
fi

SECONDS=0

docker run \
    $DOCKER_OPTIONS \
    --rm \
    --user $(id -u):$(id -g) \
    --mac-address ${MAC_ADDRESS:-F8:12:34:56:78:90} \
    -v "$(pwd)"/fw/project/lcmxo2/license.dat:/flexlm/license.dat \
    -v "$(pwd)":/workdir \
    -h=`hostname` \
    -e GIT_BRANCH="$GIT_BRANCH" \
    -e GIT_TAG="$GIT_TAG" \
    -e GIT_SHA="$GIT_SHA" \
    -e GIT_MESSAGE="$GIT_MESSAGE" \
    -e SC64_VERSION=${SC64_VERSION:-""} \
    $BUILDER_IMAGE \
    ./build.sh $@

BUILD_ERROR=$?

echo "Build took $SECONDS seconds"

popd > /dev/null

if [ $BUILD_ERROR -ne 0 ]; then
    exit -1
fi
