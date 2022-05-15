#!/bin/bash

source $bindir/diamond_env

yum remove -y libusb
diamondc build.tcl
