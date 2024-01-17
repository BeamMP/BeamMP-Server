#!/bin/bash

set -ex

apt-get update -y

apt-get install -y liblua5.3-0 liblua5.3-dev curl zip unzip tar cmake make git g++ ninja-build
