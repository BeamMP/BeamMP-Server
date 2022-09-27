#!/bin/sh

set -e

git submodule update --init --recursive

docker build . --tag beammp-static

docker run -v $(pwd):/root -it --rm beammp-static sh -c "cd root; cmake . -B build -DGIT_SUBMODULE=OFF -DLUA_INCLUDE_DIR=/lua/src -DSOL2_SINGLE=ON -DFIND_OPENSSL=OFF -DOPENSSL_CRYPTO=\"/usr/lib/libcrypto.a\" -DOPENSSL_SSL=\"/usr/lib/libssl.a\" -DFIND_ZLIB=OFF -DZLIB_ZLIB=/lib/libz.a -DCURL_FOUND=ON -DCURL_LIBRARIES=\"/usr/lib/libcurl.a\" -DLUA_LIBRARIES=\"/lua/src/liblua.a\" -DSENTRY_BACKEND=none -DSENTRY_TRANSPORT=none; make -C build -j BeamMP-Server"

