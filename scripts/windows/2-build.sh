#!/bin/bash

set -ex

cmake --build bin --parallel -t BeamMP-Server --config Release
