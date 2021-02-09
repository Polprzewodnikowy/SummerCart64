#!/bin/bash

LIBDRAGON_DOCKER_VERSION=latest

docker \
    run \
    -t \
    --mount type=bind,src=`realpath "$(dirname $0)"`,target="/libdragon" \
    anacierdem/libdragon:$LIBDRAGON_DOCKER_VERSION \
    /bin/bash -c "make clean && make all"
