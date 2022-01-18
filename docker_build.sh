#!/bin/bash

CONTAINER_NAME="sc64builder"

docker ps | grep $CONTAINER_NAME > /dev/null

if [ $? -eq 1 ]; then
    docker run \
        -dt --rm \
        --name $CONTAINER_NAME \
        --user $(id -u):$(id -g) \
        --mount type=bind,src="$(pwd)",target="/workdir" \
        ghcr.io/polprzewodnikowy/sc64env:v1.2
fi

GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_TAG=$(git describe --tags 2> /dev/null)
GIT_SHA=$(git rev-parse HEAD)

if [ -t 1 ]; then
    DOCKER_OPTIONS="-it"
fi

docker exec \
    $DOCKER_OPTIONS \
    -e GIT_BRANCH="$GIT_BRANCH" \
    -e GIT_TAG="$GIT_TAG" \
    -e GIT_SHA="$GIT_SHA" \
    $CONTAINER_NAME \
    ./build.sh $@
