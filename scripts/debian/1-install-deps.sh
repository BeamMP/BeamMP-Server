set -ex

apt-get update -y
apt-get upgrade -y

apt-get install -y liblua5.3-0 liblua5.3-dev
apt-get install -y curl zip unzip tar
apt-get install -y cmake make git g++
