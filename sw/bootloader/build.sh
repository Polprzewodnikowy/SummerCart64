#!/bin/bash

build_in_docker() {
    docker run -t \
        --mount type=bind,src=`realpath $(pwd)`,target="/src" \
        --mount type=bind,src=`realpath "$(pwd)/../libsc64"`,target="/src/libsc64" \
        $1 /bin/bash -c "cd /src && make clean && make -f $2 all"
}

build_in_docker "anacierdem/libdragon:6.0.2" "Makefile"
