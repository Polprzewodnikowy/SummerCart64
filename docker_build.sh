#!/bin/bash

set -e

BUILDER_IMAGE="ghcr.io/polprzewodnikowy/sc64env:v1.5"

pushd $(dirname $0) > /dev/null

GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_TAG=$(git describe --tags 2> /dev/null)
GIT_SHA=$(git rev-parse HEAD)

if [ -t 1 ]; then
    DOCKER_OPTIONS="-it"
fi

docker run \
    $DOCKER_OPTIONS \
    --rm \
    --user $(id -u):$(id -g) \
    --mac-address ${MAC_ADDRESS:-F8:12:34:56:78:90} \
    --mount type=bind,src="$(pwd)/flexlm",target="/flexlm" \
    --mount type=bind,src="$(pwd)",target="/workdir" \
    -e GIT_BRANCH="$GIT_BRANCH" \
    -e GIT_TAG="$GIT_TAG" \
    -e GIT_SHA="$GIT_SHA" \
    $BUILDER_IMAGE \
    ./build.sh $@

popd > /dev/null
