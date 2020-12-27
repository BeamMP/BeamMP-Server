# BeamMP-Server

The Server is the way we link client to each other and handle authentication, compression, and data management. It also allows lua plugins, that system is always being reviewed and improved with detailed instructions on wiki.beammp.com.


## Linux / POSIX specific build instructions

Currently only linux and windows are supported (generally). 

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
3. `cd` into it with `cd BeamMP-Server`
4. Run `cmake .` (with `.`)
5. Run `make`
6. You will now have a `BeamMP-Server` file in your directory, which is executable with `./BeamMP-Server`. Follow the (windows or linux, doesnt matter) instructions on the [wiki](https://wiki.beammp.com/en/home/Server_Mod) for further setup after installation (which we just did), such as port-forwarding and getting a key to actually run the server.

*tip: to run the server in the background, simply (in bash, zsh, etc) run: `nohup ./BeamMP-Server &`!`.*

## Copyright

Copyright (c) 2019-present Anonymous275. BeamMP Server code is not in the public domain and is not free software. One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries, the only permission that has been granted is to use the software in its compiled form as distributed from the BeamMP.com website. Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
