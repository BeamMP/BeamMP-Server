#!/bin/sh
apt install make 
apt install cmake 
apt install g++ 
apt install liblua5.3
apt install libz-dev
apt install rapidjson-dev
apt install libcurl4-openssl-dev
cmake .
make -j 2 # 2 threads is enough
