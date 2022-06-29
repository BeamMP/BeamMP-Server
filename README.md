# BeamMP-Server

[![CMake Windows Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Windows%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Windows+Build%22)
[![CMake Linux Build](https://github.com/BeamMP/BeamMP-Server/workflows/CMake%20Linux%20Build/badge.svg?branch=master)](https://github.com/BeamMP/BeamMP-Server/actions?query=workflow%3A%22CMake+Linux+Build%22)

This is the server for the multiplayer mod **[BeamMP](https://beammp.com/)** for the game [BeamNG.drive](https://www.beamng.com/).
The server is the point throug which all clients communicate. You can write lua mods for the server, detailed instructions on the [BeamMP Wiki](https://wiki.beammp.com).

**For Linux, you __need__ the runtime dependencies, listed below under "prerequisites".**

## Support + Contact

Feel free to ask any questions via the following channels:

- **IRC**: `#beammp` on [irc.libera.chat](https://web.libera.chat/)
- **Discord**: [click for invite](https://discord.gg/beammp)

## Minimum Requirements

These values are guesstimated and are subject to change with each release.

* RAM: 50+ MiB usable (not counting OS overhead)
* CPU: >1GHz, preferably multicore
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

**__Do not compile from `master`. Always build from a release tag, i.e. `tags/v2.3.3`!__**

Currently only linux and windows are supported (generally). See [Releases](https://github.com/BeamMP/BeamMP-Server/releases/) for official binary releases. On systems to which we do not provide binaries (so anything but windows), you are allowed to compile the program and use it. Other restrictions, such as not being allowed to distribute those binaries, still apply (see [copyright notice](#copyright)).

### Prerequisites

#### Windows

Please use the prepackaged binaries in [Releases](https://github.com/BeamMP/BeamMP-Server/releases/).

Dependencies for **windows** can be installed with `vcpkg`.
These are:
```
lua
zlib
rapidjson
openssl
websocketpp
curl
```

#### Linux

These package names are in the debian / ubuntu style. Feel free to PR your own guide for a different distro.

Runtime dependencies for **linux** are (debian/ubuntu):
```
libz-dev
rapidjson-dev
liblua5.3
libssl-dev
libwebsocketpp-dev
libcurl4-openssl-dev
```

Build-time dependencies for **linux** are:
```
git
make
cmake
g++
```

For other distributions (e.g. Arch) you want to find packages for:
- libz
- rapidjson
- lua5.3
- ssl / openssl
- websocketpp
- curl (with ssl support)
- \+ the build time dependencies from above

#### macOS

Dependencies for **macOS** can be installed with homebrew.
```
brew install lua@5.3 rapidjson websocketpp cmake openssl@1.1
```
Some packages are included in **macOS** but you might want to install homebrew versions.
```
brew install curl zlib git gcc make
```
Compile with homebrew gcc on apple silicon:
```
cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/opt/homebrew/bin/gcc-11 -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-11
make
```
Currently lionkor/commandline doesn't support macOS, so you need to manually patch it to make it working.
```patch
diff --git a/commandline.cpp b/commandline.cpp
index 367a0c3..ee61449 100644
--- a/commandline.cpp
+++ b/commandline.cpp
@@ -2,7 +2,7 @@
 #include <functional>
 #include <mutex>

-#if defined(__linux) || defined(__linux__)
+#if defined(__linux) || defined(__linux__) || defined(__APPLE__)
 #include <pthread.h>
 #include <stdio.h>
 #include <termios.h>
@@ -16,7 +16,7 @@

 #if defined(WIN32)
 #define WINDOWS
-#elif defined(__linux) || defined(__linux__)
+#elif defined(__linux) || defined(__linux__) || defined(__APPLE__)
 #define LINUX
 #else
 #error "platform not supported"
```


### How to build

On windows, use git-bash for these commands. On Linux, these should work in your shell.

1. Make sure you have all [prerequisites](#prerequisites) installed
2. Clone the repository in a location of your choice with `git clone --recurse-submodules https://github.com/BeamMP/BeamMP-Server`. 
3. Ensure that all submodules are initialized by running `git submodule update --init --recursive`. Then change into the cloned directory by running `cd BeamMP-Server`.
4. Checkout the branch of the release you want to compile (`master` is often unstable), for example `git checkout tags/v3.0.1` for version 3.0.1. You can find the latest version [here](https://github.com/BeamMP/BeamMP-Server/tags).
5. Run `cmake . -DCMAKE_BUILD_TYPE=Release` (with `.`)
6. Run `make`
7. You will now have a `BeamMP-Server` file in your directory, which is executable with `./BeamMP-Server` (`.\BeamMP-Server.exe` for windows). Follow the (windows or linux, doesnt matter) instructions on the [wiki](https://wiki.beammp.com/en/home/Server_Mod) for further setup after installation (which we just did), such as port-forwarding and getting a key to actually run the server.

*tip: to run the server in the background, simply (in bash, zsh, etc) run:* `nohup ./BeamMP-Server &`*.*

## Copyright

Copyright (c) 2019-present Anonymous275 (@Anonymous-275), Lion Kortlepel (@lionkor).
BeamMP-Server code is not in the public domain and is not free software. One must be granted explicit permission by the copyright holder(s) in order to modify or distribute any part of the source or binaries. Special permission to modify the source-code is implicitly granted only for the purpose of upstreaming those changes directly to github.com/BeamMP/BeamMP-Server via a GitHub pull-request.
Commercial usage is prohibited, unless explicit permission has been granted prior to usage.
