#!/bin/sh

# usage:
# ./docker-build.sh
#       builds the image, and then runs it
# ./docker-build.sh run
#       only runs it

set -e

if [ "$1" != "run" ]
then
    git submodule update --init --recursive
    docker build . --tag beammp-static
fi


docker run -v $(pwd):/root -it --rm beammp-static sh -c "cd root; cmake . -B build -DGIT_SUBMODULE=OFF -DLUA_INCLUDE_DIR=/lua/src -DSOL2_SINGLE=ON -DFIND_OPENSSL=OFF -DOPENSSL_CRYPTO=\"/usr/lib/libcrypto.a\" -DOPENSSL_SSL=\"/usr/lib/libssl.a\" -DFIND_ZLIB=OFF -DZLIB_ZLIB=/lib/libz.a -DCURL_FOUND=ON -DCURL_LIBRARIES=\"/usr/lib/libcurl.a\" -DLUA_LIBRARIES=\"/lua/src/liblua.a\" -DSENTRY_BACKEND=none -DSENTRY_TRANSPORT=none; make -C build -j BeamMP-Server"

# try to chown the build directory afterwards
sudo chown -R "$USER":"$USER" build

