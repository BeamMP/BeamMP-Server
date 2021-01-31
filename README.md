# BeamMP-Server

[![CMake Windows Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Windows%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Windows+Build%22)
[![CMake Linux Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Linux%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Linux+Build%22)

The Server is the way we link client to each other and handle authentication, compression, and data management. It also allows lua plugins, that system is always being reviewed and improved with detailed instructions on wiki.beammp.com.

## Supported Operating Systems

The code itself supports (latest stable) Linux and Windows. In terms of actual build support, for now we only distribute windows binaries and instructions to build on Debian 10 (stable). For any other distro or OS, you just have to find the same libraries listed in the Linux Build [Prerequisites](#prerequisites) further down the page, and it should build fine. We don't currently support ARM or any big-endian architectures. 

Recommended compilers: MSVC, GCC. 

You can find precompiled binaries under [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

## Linux / POSIX specific build instructions

Currently only linux and windows are supported (generally). On systems to which we do not provide binaries (so anything but windows), you are allowed to compile the program and use it. Other restrictions, such as not being allowed to distribute those binaries, still apply (see [copyright notice](#copyright)).

### Prerequisites

All package names are ones found in debian's (debian 10 stable) repositories, but will exist under similar names in other distros. Feel free to PR your own guide for a different distro.

- `git`
- `make`
- `cmake`
- `g++` (must support ISO C++17)
- `liblua5.3`
- `libz-dev`
- `rapidjson-dev`
- `libcurl4-openssl-dev` (or other `libcurl4-*-dev`)

### How to build

1. Make sure you have all [prerequisites](#prerequisites) installed
2. Clone the repository in a location of your choice with `git clone --recursive https://github.com/BeamMP/BeamMP-Server`
3. Checkout the branch of the release you want to compile (`master` is often unstable), for example `git checkout tags/v1.20` for version 1.20.
4. `cd` into it with `cd BeamMP-Server`
5. Run `cmake .` (with `.`)
6. Run `make`
7. You will now have a `BeamMP-Server` file in your directory, which is executable with `./BeamMP-Server`. Follow the (windows or linux, doesnt matter) instructions on the [wiki](https://wiki.beammp.com/en/home/Server_Mod) for further setup after installation (which we just did), such as port-forwarding and getting a key to actually run the server.

*tip: to run the server in the background, simply (in bash, zsh, etc) run:* `nohup ./BeamMP-Server &`*.*

## Copyright

Copyright (c) 2019-present Anonymous275. BeamMP Server code is not in the public domain and is not free software. One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries, the only permission that has been granted is to use the software in its compiled form as distributed from the BeamMP.com website. Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
