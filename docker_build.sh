#!/bin/bash

GIT_SHA=$(git rev-parse --short HEAD)
GIT_TAG=$(git describe --tags --exact-match 2> /dev/null)

if [ ! -z "$GIT_TAG" ]; then
    __SC64_VERSION="git tag: $GIT_TAG"
else
    __SC64_VERSION="git sha: $GIT_SHA"
fi

docker run \
    --rm \
    --user $(id -u):$(id -g) \
    --mount type=bind,src="$(pwd)",target="/workdir" \
    -e __SC64_VERSION="$__SC64_VERSION" \
    ghcr.io/polprzewodnikowy/sc64env:v1.2 \
    ./build.sh $@
