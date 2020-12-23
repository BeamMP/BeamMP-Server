# BeamMP-Server

The Server is the way we link client to each other and handle authentication, compression, and data management. It also allows lua plugins, that system is always being reviewed and improved with detailed instructions on wiki.beammp.com.


## Unix specific build instructions

1. You need boost >= 1.70.0

Check with your ditro's package manager whether it provides this. If it does, you should use that. 


If it doesnt provide it or you want to link it statically (like we do with our releases), then you have to do this:

download the latest boost source code.
Then, go to the downloaded directory and run
```sh
b2 link=static runtime-link=static threading=multi
```
And then either symlink, edit CMakeLists to find it, or simply run
```sh
b2 link=static runtime-link=static threading=multi install
```
(warning: installs boost into your system, you might not want this).

Then on invocation of cmake, ensure that you define `Boost_USE_STATIC_RUNTIME=ON`.


2. Building

Run cmake, and then make.

Example:

```bash
~/src/Server $ cmake -S . -B bin -DCMAKE_BUILD_TYPE=Release
...
~/src/Server $ make -C bin -j 5
```


Copyright (c) 2019-present Anonymous275. BeamMP Server code is not in the public domain and is not free software. One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries, the only permission that has been granted is to use the software in its compiled form as distributed from the BeamMP.com website. Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
