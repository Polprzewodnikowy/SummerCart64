- [Building FW/SW](#building-fwsw)
  - [Docker method](#docker-method)
    - [Lattice Diamond license](#lattice-diamond-license)

---

## Building FW/SW

### Docker method

Docker method is a preferred option.
Run `./docker_build.sh release` to build all firmware/software and generate release package.
For other options run script without any command to print help.

#### Lattice Diamond license

Lattice Diamond software is used to build FPGA bitstream, a free 1 year license is necessary to run the build process.
Repository already contains license attached to fake MAC address `F8:12:34:56:78:90` (path to the license: `fw/project/lcmxo2/license.dat`).
If the license expires, it is required to request new license from Lattice webpage.
New license can be attached to aforementioned MAC address or any other address.
In case of non default MAC address it is possible to provide it via `MAC_ADDRESS` environment variable.
For example: `MAC_ADDRESS=AB:00:00:00:00:00 ./docker_build.sh release`.
