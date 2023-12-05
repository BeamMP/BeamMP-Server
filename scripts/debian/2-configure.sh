#!/bin/bash

set -ex

cmake . -B bin -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -s -Wl,-z,norelro -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,noseparate-code -ffunction-sections -fdata-sections -Wl,--gc-sections" -DBeamMP-Server_ENABLE_LTO=ON || cat "$SOURCE"/server/bin/vcpkg-bootstrap.log $1
