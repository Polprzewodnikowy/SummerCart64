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

The Lattice Diamond software is used to build the FPGA bitstream.
A free 1 year license is necessary to run the build process.
You can request personal license from the [Lattice website](https://www.latticesemi.com/Support/Licensing).
The Build script expects a license file to be present in the path: `fw/project/lcmxo2/license.dat`.
Since the build is done inside docker container, it is required to parse the MAC address linked with the license file to the build container.
The Build script expects it in the `MAC_ADDRESS` environment variable.
For example, run `MAC_ADDRESS=AB:00:00:00:00:00 ./docker_build.sh release` command to start the building process if your license is attached to `AB:00:00:00:00:00` MAC address.

For Github CICD: 
* Add a Repository secrets variable called `LATTICE_DIAMOND_LICENSE_BASE64` with the license file as base64 encoded string.
* Add a Repository secrets variable called `LATTICE_DIAMOND_MAC_ADDRESS` containing the expected MAC address.
