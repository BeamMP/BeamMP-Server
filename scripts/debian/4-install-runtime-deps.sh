#!/bin/bash

set -ex

apt-get update -y
apt-get upgrade -y

apt-get install -y liblua5.3-0 curl

