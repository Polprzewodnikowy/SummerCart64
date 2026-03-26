# Building TRX

## Build workflow

Initial build:

- Compile the project.
- Copy all executable files from `build/` to your game directory.
- Copy the contents of `data/*/ship/` to your game directory.

Subsequent builds:

- Compile the project.
- Copy all executable files from `build/` to your game directory.
  We recommend making a script file to do this.

## Compiling

### Compiling on Linux

Follow [BUILDING_ON_LINUX.md](BUILDING_ON_LINUX.md).

### Compiling on Windows

Follow [BUILDING_ON_WINDOWS.md](BUILDING_ON_WINDOWS.md).

### Compiling on MacOS

Follow [BUILDING_ON_MACOS.md](BUILDING_ON_MACOS.md).

### Supported compilers

Please be advised that any build systems that are not the one we use for
automating releases (= mingw-w64) come at user's own risk. They might crash or
even refuse to compile.
