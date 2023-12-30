#!/bin/bash

set -ex

./vcpkg/bootstrap-vcpkg.sh

cmake . -B bin $1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -g -Wl,-z,norelro -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,noseparate-code -ffunction-sections -fdata-sections -Wl,--gc-sections" -DBeamMP-Server_ENABLE_LTO=ON
