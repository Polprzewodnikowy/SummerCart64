#!/bin/bash

docker run \
    --rm \
    --mount type=bind,src="$(pwd)",target="/workdir" \
    ghcr.io/polprzewodnikowy/sc64env:v1.2 \
    ./build.sh $@
