#!/bin/bash

docker run -t --mount type=bind,src="$(pwd)",target="/libdragon" anacierdem/libdragon:4.1.1 /bin/bash -c "/usr/bin/make clean; /usr/bin/make all N64_BYTE_SWAP=false"
