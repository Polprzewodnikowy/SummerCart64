#!/bin/bash

docker run --mount type=bind,src="$(pwd)",target="/workdir" ghcr.io/polprzewodnikowy/sc64env:v0.9 /bin/bash -c "make clean all"
