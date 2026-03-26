#!/bin/bash
set -x
set -e

if [[ -z "$1" || ( "$1" != "1" && "$1" != "2" ) ]]; then
    echo "Error: You must supply '1' or '2' as an argument to decide which game to build."
    exit 1
fi
TR_VERSION=$1

cd /app/tools/installer/

export DOTNET_CLI_HOME="/tmp/DOTNET_CLI_HOME"

shopt -s globstar
rm -rf **/bin **/obj **/out/*
dotnet publish TR${TR_VERSION}X_Installer -c Release -o out
