#!/bin/bash

set -ex

cmake . -B bin $1 -DCMAKE_BUILD_TYPE=Release -DBeamMP-Server_ENABLE_LTO=ON -DVCPKG_TARGET_TRIPLET=x64-windows-static
