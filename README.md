# BeamMP-Server

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
