# BeamMP-Server

[![CMake Windows Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Windows%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Windows+Build%22)
[![CMake Linux Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Linux%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Linux+Build%22)

This is the server for the multiplayer mod **[BeamMP](https://beammp.com/)** for the game [BeamNG.drive](https://www.beamng.com/).
The server is the point through which all clients communicate. You can write Lua mods for the server, there are detailed instructions on the [BeamMP Wiki](https://wiki.beammp.com).

**For Linux, you __need__ the runtime dependencies, which are listed below under [Runtime Dependencies](#runtime-dependencies)**

## Support + Contact

Feel free to ask any questions via the following channels:

- **Discord**: [click for invite](https://discord.gg/beammp)
- **BeamMP Forum**: [BeamMP Forum Support](https://forum.beammp.com/c/support/33)

## Minimum Requirements

These values are guesstimated and are subject to change with each release.

* RAM: 30-100 MiB usable (not counting OS overhead)
* CPU: >1GHz, preferably multicore
* OS: Windows, Linux (theoretically any POSIX)
* GPU: None
* HDD: 10 MiB + Mods/Plugins
* Bandwidth: 5-10 Mb/s upload

## Contributing

TLDR; [Issues](https://github.com/BeamMP/BeamMP-Server/issues) with the "help wanted" or "good first issue" label or with nobody assigned.

To contribute, look at the active [issues](https://github.com/BeamMP/BeamMP-Server/issues). Any issues that have the "help wanted" label or don't have anyone assigned are good tasks to take on. You can either contribute by programming or by testing and adding more info and ideas.

Fork this repository, make a new branch for your feature, implement your feature or fix, and then create a pull-request here. Even incomplete features and fixes can be pull-requested.

If you need support with understanding the codebase, please write us in the Discord. You'll need to be proficient in modern C++.

## About Building from Source

We only allow building unmodified (original) source code for public use. `master` is considered **unstable** and we will not provide technical support if such a build doesn't work, so always build from a tag. You can checkout a tag with `git checkout tags/TAGNAME`, where `TAGNAME` is the tag, for example `v3.1.0`. 

## Supported Operating Systems

The code itself supports (latest stable) Linux, Windows and FreeBSD. In terms of actual build support, for now we usually only distribute Windows binaries and Linux. For any other distro or OS, you just have to find the same libraries listed in [Runtime Dependencies](#runtime-dependencies) further down the page, and it should build fine.

Recommended compilers: MSVC, GCC, CLANG. 

You can find precompiled binaries under [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

## Build Instructions

On Linux, you need some dependencies to **build** the server (on Windows, you don't):

```
liblua5.3-dev curl zip unzip tar cmake make git g++
```

You can install these with your distribution's package manager. You will need sudo or need root for ONLY this step.

The names of each package may change depending on your platform.

If you are building for ARM (like aarch64), you need to run `export VCPKG_FORCE_SYSTEM_BINARIES=1` before the following commands.

You can build on **Windows, Linux** or other platforms by following these steps:

1. Check out the repository with git: `git clone --recursive https://github.com/BeamMP/BeamMP-Server`.
2. Go into the directory `cd BeamMP-Server`.
3. Run CMake `cmake -S . -B bin -DCMAKE_BUILD_TYPE=Release` - this can take a few minutes and may take a lot of disk space and bandwidth.
4. Build via `cmake --build bin --parallel --config Release -t BeamMP-Server`.
5. Your executable can be found in `bin/`.

When you make changes to the code, you only have to run step 4 again.
### Building for FreeBSD
Building is only supported for major release branches of FreeBSD that are currently not EOL. The build process is the same as on Linux, although build dependencies can be universally installed from ports via pkg:
```
pkg install git cmake-core zip bash devel/ninja devel/pkgconf lua53
```
After installing the necessary build dependencies, follow the Linux build instructions beginning from step 3. Beware that running the initial cmake command will compile vcpkg from source, as vcpkg has no native FreeBSD port - this may take some time.

On systems with a single logical CPU core, `make` may fail to build the server when using the `--parallel` option when calling CMake. If you see error messages related to make, simply omit the `--parallel` from the command: `cmake --build bin --config Release -t BeamMP-Server`.

### Runtime Dependencies

These are needed to *run* the server.

Debian, Ubuntu and friends: `liblua5.3-0`

Other Linux distros: `liblua` of *some kind*.

Windows: No libraries.

## Support
The BeamMP project is supported by community donations via our [Patreon](https://www.patreon.com/BeamMP). This brings perks such as Patreon-only channels on our Discord, early access to new updates, and more server keys. 
