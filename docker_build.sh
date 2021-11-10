#!/bin/bash

docker run \
    --rm \
    -it \
    --mount type=bind,src="$(pwd)",target="/workdir" \
    ghcr.io/polprzewodnikowy/sc64env:v1.2 \
    ./build.sh $@
