#!/bin/bash

docker run --mount type=bind,src="$(pwd)",target="/workdir" ghcr.io/polprzewodnikowy/sc64env /bin/bash ./docker/build.sh
