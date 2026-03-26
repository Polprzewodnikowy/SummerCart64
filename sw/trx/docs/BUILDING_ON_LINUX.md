## Building on Linux

This guide describes the officially supported Linux build workflow using
Docker and [just](https://github.com/casey/just).

## Installing dependencies

Install the following dependencies using your distribution's package manager:

- `docker`
- `ffmpeg`
- `glew`
- `just`
- `meson`
- `pkgconfig`
- `python3`
- `sdl2`
- `uthash`

Depending on your system, the Docker package may be named `docker`,
`docker.io`, or similar.

## Building TRX

1. Download the shipped game assets for the game you want to build:

    ```bash
    ./tools/download_assets X
    ```

    Replace `X` with the TR version.

2. Build the Linux target:

    ```bash
    just trx-build-linux target='debug'
    ```

The built files will be placed in the `build/` directory.

## Other build methods

The Docker workflow above is the recommended way to build TRX on Linux.

If you prefer a manual, non-Docker setup, you are welcome to try it, but it is
not part of the project's official build workflow.

The best starting point is to inspect the files in `tools/*/docker/` for the
external dependencies and `meson.build` for the local files, then tailor your
system to match the release build environment as closely as possible.

## Running the game

To prepare the game directory:

1. Copy the built files from `build/`.
2. Copy the contents of `data/X/ship/`.
3. Copy the original game files from your game installation.

Replace `X` with the TR version you built.

Once the files are in place, run:

```bash
./TRX
```
