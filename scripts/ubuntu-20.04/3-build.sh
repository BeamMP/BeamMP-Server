#!/bin/bash

set -ex

cmake --build bin --parallel -t BeamMP-Server

objcopy --only-keep-debug bin/BeamMP-Server bin/BeamMP-Server.debug
objcopy --add-gnu-debuglink bin/BeamMP-Server bin/BeamMP-Server.debug

strip -s bin/BeamMP-Server
