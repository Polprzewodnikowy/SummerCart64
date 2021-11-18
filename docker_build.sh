#!/bin/bash

docker run \
    --rm \
    --user $(id -u):$(id -g) \
    --mount type=bind,src="$(pwd)",target="/workdir" \
    ghcr.io/polprzewodnikowy/sc64env:v1.2 \
    ./build.sh $@
