#!/bin/bash

build_in_docker() {
    docker run -t \
        --mount type=bind,src=`realpath $(pwd)`,target="/src" \
        $1 /bin/bash -c "cd /src && make -f $2 all"
}

build_in_docker "anacierdem/libdragon:6.0.2" "Makefile.libdragon"

build_in_docker "polprzewodnikowy/n64sdkmod:latest" "Makefile.libultra"
