#!/bin/bash

set -ex

git config --global --add safe.directory $(pwd)
git config --global --add safe.directory $(pwd)/vcpkg
git config --global --add safe.directory $(pwd)/deps/commandline