#!/bin/sh

cmake . -B build -DCMAKE_BUILD_TYPE=Release -DGIT_SUBMODULE=OFF

make -j -C build BeamMP-Server
