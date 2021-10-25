#!/bin/bash

docker run \
    --mount type=bind,src="$(pwd)",target="/workdir" \
    ghcr.io/polprzewodnikowy/sc64env:v1.0 \
    /bin/bash -c "USER_FLAGS=\"-DDEBUG\" make clean all"
