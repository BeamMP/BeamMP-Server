# BeamMP-Server

[![CMake Windows Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Windows%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Windows+Build%22)
[![CMake Linux Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Linux%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Linux+Build%22)

The Server is the way we link client to each other and handle authentication, compression, and data management. It also allows lua plugins, that system is always being reviewed and improved with detailed instructions on wiki.beammp.com.

## About Building from Source

We only allow building unmodified (original) source code. `master` is considered **unstable** and we will not provide technical support if such a build doesn't work, so always build from a tag. You can checkout a tag with `git checkout tags/v1.20`. 

## Supported Operating Systems

The code itself supports (latest stable) Linux and Windows. In terms of actual build support, for now we only distribute windows binaries and instructions to build on Debian 10 (stable). For any other distro or OS, you just have to find the same libraries listed in the Linux Build [Prerequisites](#prerequisites) further down the page, and it should build fine. We don't currently support ARM or any big-endian architectures. 

Recommended compilers: MSVC, GCC. 

You can find precompiled binaries under [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

## Build Instructions

**__Do not compile from `master`. Always build from a release tag, i.e. `tags/v1.20`!__**

Currently only linux and windows are supported (generally). See [Releases](https://github.com/BeamMP/BeamMP-Server/releases/) for official binary releases. On systems to which we do not provide binaries (so anything but windows), you are allowed to compile the program and use it. Other restrictions, such as not being allowed to distribute those binaries, still apply (see [copyright notice](#copyright)).

### Prerequisites

#### Windows

Please use the prepackaged binaries in [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

Dependencies for windows can be installed with `vcpkg`, in which case the current dependencies are the `x64-windows-static` versions of `lua`, `zlib`, `rapidjson`, `boost-beast`, `boost-asio` and `openssl`.

#### Linux / \*nix

These package names are in the debian / ubuntu style. Feel free to PR your own guide for a different distro.

- `git`
- `make`
- `cmake`
- `g++`
  
  Must support ISO C++17. If your distro's `g++` doesn't support C++17, chances are that it has a `g++-8` or `g++-10` package that does. If this is the case. you just need to run CMake with `-DCMAKE_CXX_COMPILER=g++-10` (replace `g++-10` with your compiler's name).
- `liblua5.3` 
  
  Any 5.x version should work, but 5.3 is what we officially use. Any other version might break in the future.
  You can also use any version of `libluajit`, but the same applies regarding the version.
- `libz-dev`
- `rapidjson-dev`
- `libboost1.70-dev` 
  
  If your distro doesn't have 1.7x version of libboost, you'll have to compile it from source. For Ubuntu, you can use 
  ```
  sudo add-apt-repository ppa:mhier/libboost-latest
  sudo apt-get install -y libboost1.70-dev libboost1.70
  ```
- `libopenssl-dev`

### How to build

On windows. use git-bash for these commands.

1. Make sure you have all [prerequisites](#prerequisites) installed
2. Clone the repository in a location of your choice with `git clone --recursive https://github.com/BeamMP/BeamMP-Server`
3. Checkout the branch of the release you want to compile (`master` is often unstable), for example `git checkout tags/v1.20` for version 1.20.
4. `cd` into it with `cd BeamMP-Server`
5. Run `cmake .` (with `.`)
6. Run `make`
7. You will now have a `BeamMP-Server` file in your directory, which is executable with `./BeamMP-Server` (`.\BeamMP-Server.exe` for windows). Follow the (windows or linux, doesnt matter) instructions on the [wiki](https://wiki.beammp.com/en/home/Server_Mod) for further setup after installation (which we just did), such as port-forwarding and getting a key to actually run the server.

*tip: to run the server in the background, simply (in bash, zsh, etc) run:* `nohup ./BeamMP-Server &`*.*

## Copyright

Copyright (c) 2019-present Anonymous275. BeamMP Server code is not in the public domain and is not free software. One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries, the only permission that has been granted is to use the software in its compiled form as distributed from the BeamMP.com website. Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
