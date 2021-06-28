# BeamMP-Server

[![CMake Windows Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Windows%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Windows+Build%22)
[![CMake Linux Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Linux%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Linux+Build%22)

This is the server for the multiplayer mod **[BeamMP](https://beammp.com/)** for the game [BeamNG.drive](https://www.beamng.com/).
The server is the point throug which all clients communicate. You can write lua mods for the server, detailed instructions on the [BeamMP Wiki](https://wiki.beammp.com).

## Minimum Requirements

These values are guesstimated and are subject to change with each release.

* RAM: 50+ MiB usable (not counting OS overhead)
* CPU: Any Hz, preferably multicore
* OS: Windows, Linux (theoretically any POSIX)
* GPU: None
* HDD: 10 MiB + Mods/Plugins
* Bandwidth: 5-10 Mb/s upload

## Contributing

TLDR; [Issues](https://github.com/BeamMP/BeamMP-Server/issues) with the "help wanted" label or with nobody assigned, any [trello](https://trello.com/b/Kw75j3zZ/beamngdrive-multiplayer) cards in the "To-Do" column.

To contribute, look at the active [issues](https://github.com/BeamMP/BeamMP-Server/issues) and at the [trello](https://trello.com/b/Kw75j3zZ/beamngdrive-multiplayer). Any issues that have the "help wanted" label or don't have anyone assigned and any trello cards that aren't assigned or in the "In-Progress" section are good tasks to take on. You can either contribute by programming or by testing and adding more info and ideas.

Fork this repository, make a new branch for your feature, implement your feature or fix, and then create a pull-request here. Even incomplete features and fixes can be pull-requested.

If you need support with understanding the codebase, please write us in the discord. You'll need to be proficient in modern C++.

## About Building from Source

We only allow building unmodified (original) source code for public use. `master` is considered **unstable** and we will not provide technical support if such a build doesn't work, so always build from a tag. You can checkout a tag with `git checkout tags/TAGNAME`, where `TAGNAME` is the tag, for example `v1.20`. 

## Supported Operating Systems

The code itself supports (latest stable) Linux and Windows. In terms of actual build support, for now we usually only distribute windows binaries and sometimes linux. For any other distro or OS, you just have to find the same libraries listed in the Linux Build [Prerequisites](#prerequisites) further down the page, and it should build fine. We don't currently support any big-endian architectures.

Recommended compilers: MSVC, GCC, CLANG. 

You can find precompiled binaries under [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

## Build Instructions

**__Do not compile from `master`. Always build from a release tag, i.e. `tags/v2.0`!__**

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
- `libopenssl-dev`

**If** you're building it from source, you'll need `libboost1.70-all-dev` or `libboost1.71-all-dev` or higher as well.

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

Copyright (c) 2019-present Anonymous275 (@Anonymous-275), Lion Kortlepel (@lionkor).
BeamMP-Server code is not in the public domain and is not free software. One must be granted explicit permission by the copyright holder(s) in order to modify or distribute any part of the source or binaries. Special permission to modify the source-code is implicitly granted only for the purpose of upstreaming those changes directly to github.com/BeamMP/BeamMP-Server via a GitHub pull-request.
Commercial usage is prohibited, unless explicit permission has been granted prior to usage.
