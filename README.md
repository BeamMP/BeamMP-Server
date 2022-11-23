# BeamMP-Server

[![CMake Windows Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Windows%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Windows+Build%22)
[![CMake Linux Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Linux%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Linux+Build%22)

This is the server for the multiplayer mod **[BeamMP](https://beammp.com/)** for the game [BeamNG.drive](https://www.beamng.com/).
The server is the point through which all clients communicate. You can write Lua mods for the server, there are detailed instructions on the [BeamMP Wiki](https://wiki.beammp.com).

**For Linux, you __need__ the runtime dependencies, listed below under "[prerequisites](#prerequisites)".**

## Support + Contact

Feel free to ask any questions via the following channels:

- **IRC**: `#beammp` on [irc.libera.chat](https://web.libera.chat/)
- **Discord**: [click for invite](https://discord.gg/beammp)
- **BeamMP Forum**: [BeamMP Forum Support](https://forum.beammp.com/c/support/33)

## Minimum Requirements

These values are guesstimated and are subject to change with each release.

* RAM: 50+ MiB usable (not counting OS overhead)
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

The code itself supports (latest stable) Linux and Windows. In terms of actual build support, for now we usually only distribute Windows binaries and sometimes Linux. For any other distro or OS, you just have to find the same libraries listed in the Linux Build [Prerequisites](#prerequisites) further down the page, and it should build fine. We don't currently support any big-endian architectures.

Recommended compilers: MSVC, GCC, CLANG. 

You can find precompiled binaries under [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

## Build Instructions

**__Do not compile from `master`. Always build from a release tag, i.e. `tags/v3.1.0`!__**

Currently only Linux and Windows are supported (generally). See [Releases](https://github.com/BeamMP/BeamMP-Server/releases/) for official binary releases. On systems to which we do not provide binaries (so anything but windows), you are allowed to compile the program and use it. Other restrictions, such as not being allowed to distribute those binaries, still apply (see [copyright notice](#copyright)).

### Prerequisites

#### Windows

There are **no runtime libraries** needed for Windows.

Please use the prepackaged binaries in [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

Dependencies for **Windows** can be installed with `vcpkg`.
These are:
```
lua zlib rapidjson openssl websocketpp curl boost
```
The triplet we use for releases is `x64-windows-static`.

#### Linux

We recommend Ubuntu 22.04 or Arch Linux. Any Linux distribution will work, but you have to figure out the package names yourself (please feel free to PR in a change to this README with that info).

##### Runtime Dependencies

These are needed to *run* the server.

<details>
<summary>
Ubuntu 22.04
</summary>

`apt-get install` the following libraries:
```
liblua5.3-0
libssl3
curl
```
</details>

<details>
<summary>
Arch Linux
</summary>

`pacman -Syu` the following libraries:
```
lua53
openssl
curl
```
</details>

##### Build Dependencies
These are needed for you to *build* the server, in addition to the [runtime dependencies](#runtime-dependencies).

<details>
<summary>
Ubuntu 22.04
</summary>

`apt-get install` the following libraries and programs:
```
git
libz-dev
rapidjson-dev
liblua5.3
libssl-dev
libwebsocketpp-dev
libcurl4-openssl-dev
cmake
g++-10
libboost1.74-all-dev
libssl3
curl
```
</details>

<details>
<summary>
Arch Linux
</summary>

`pacman -Syu` the following libraries and programs:
```
lua53
openssl
curl
git
cmake
g++
cmake
zlib
boost
websocketpp
```
</details>

#### macOS

Dependencies for **macOS** can be installed with homebrew.
```
brew install lua@5.3 rapidjson websocketpp cmake openssl@1.1
```
Some packages are included in **macOS** but you might want to install homebrew versions.
```
brew install curl zlib git make
```

### How to build

On Windows, use git-bash for these commands. On Linux, these should work in your shell.

1. Make sure you have all [prerequisites](#prerequisites) installed
2. Clone the repository in a location of your choice with `git clone https://github.com/BeamMP/BeamMP-Server` . 
3. Change into the BeamMP-Server directory by running `cd BeamMP-Server`. 
4. Checkout the branch or tag of the release you want to compile, for example `git checkout tags/v3.0.2` for version 3.0.2. You can find the latest version [here](https://github.com/BeamMP/BeamMP-Server/tags).
6. Run `cmake . -DCMAKE_BUILD_TYPE=Release` (with `.`). This may take some time, and will update all submodules and prepare the build.
7. Run `make -j` . This step will take some time and will use a lot of CPU and RAM. Remove the `-j` if you run out of memory. *If you change something in the source code, you only have to re-run this step.*
8. You now have a `BeamMP-Server` file in your directory, which is executable with `./BeamMP-Server` (`.\BeamMP-Server.exe` for windows). Follow the (Windows or Linux, doesnt matter) instructions on the [wiki](https://wiki.beammp.com/en/home/server-installation) for further setup after installation (which we just did), such as port-forwarding and getting a key to actually run the server.

*tip: to run the server in the background, simply (in bash, zsh, etc) run:* `nohup ./BeamMP-Server &`*.*

## Support
The BeamMP project is supported by community donations via our [Patreon](https://www.patreon.com/BeamMP). This brings perks such as Patreon-only channels on our Discord, early access to new updates, and more server keys. 

## Copyright

Copyright (c) 2019-present Anonymous275 (@Anonymous-275), Lion Kortlepel (@lionkor).
BeamMP-Server code is not in the public domain and is not free software. One must be granted explicit permission by the copyright holder(s) in order to modify or distribute any part of the source or binaries. Special permission to modify the source-code is implicitly granted only for the purpose of upstreaming those changes directly to github.com/BeamMP/BeamMP-Server via a GitHub pull-request.
Commercial usage is prohibited, unless explicit permission has been granted prior to usage.
