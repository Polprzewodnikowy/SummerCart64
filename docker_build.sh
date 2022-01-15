#!/bin/bash

GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_SHA=$(git rev-parse HEAD)
GIT_TAG=$(git describe --tags --exact-match 2> /dev/null)

if [ -z $GIT_TAG ]; then
    GIT_TAG="develop"
fi

__SC64_VERSION=$(printf "[ %q | %q | %q ]" $GIT_BRANCH $GIT_TAG $GIT_SHA)

if [ -t 1 ]; then
    DOCKER_OPTIONS="-it"
fi

docker run \
    --rm $DOCKER_OPTIONS \
    --user $(id -u):$(id -g) \
    --mount type=bind,src="$(pwd)",target="/workdir" \
    -e __SC64_VERSION="$__SC64_VERSION" \
    ghcr.io/polprzewodnikowy/sc64env:v1.2 \
    ./build.sh $@
