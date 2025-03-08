- [Building FW/SW](#building-fwsw)
  - [Docker method](#docker-method)
    - [Lattice Diamond license](#lattice-diamond-license)

---

## Building FW/SW

### Docker method

Docker method is a preferred option.
Run `./docker_build.sh release` to build all firmware/software and generate release package.
For other options run script without any command to print help about available options.

#### Lattice Diamond license

Lattice Diamond software is used to build the FPGA bitstream.
A free 1 year license is necessary to run the build process.
You can request personal license from the [Lattice website](https://www.latticesemi.com/Support/Licensing).
Build script expects license file to be present in this path: `fw/project/lcmxo2/license.dat`.
Since build is done inside docker container it is required to pass the MAC address, linked with the license file, to a container.
Build script expects it in the `MAC_ADDRESS` environment variable.
For example, run `MAC_ADDRESS=AB:00:00:00:00:00 ./docker_build.sh release` command to start the building process if your license is attached to `AB:00:00:00:00:00` MAC address.
