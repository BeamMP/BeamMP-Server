#!/bin/sh

set -e

printf "enter DSN (optional): "
read DSN

docker build -f Ubuntu-20.04-Dockerfile . -t beammp-server-build:Ubuntu-20.04
docker build -f Ubuntu-22.04-Dockerfile . -t beammp-server-build:Ubuntu-22.04
docker build -f ArchLinux-Dockerfile . -t beammp-server-build:ArchLinux
docker build -f Debian-11-Dockerfile . -t beammp-server-build:Debian-11

CMD="cd /beammp; cmake . -DGIT_SUBMODULE=OFF -DCMAKE_BUILD_TYPE=Release -DBEAMMP_SECRET_SENTRY_URL=\"${DSN}\" -B /build && make -j -C /build BeamMP-Server"

docker run -v $(pwd)/..:/beammp -v $(pwd)/../build-ubuntu-20.04:/build -it --rm beammp-server-build:Ubuntu-20.04 bash -c "${CMD}"
docker run -v $(pwd)/..:/beammp -v $(pwd)/../build-ubuntu-22.04:/build -it --rm beammp-server-build:Ubuntu-22.04 bash -c "${CMD}"
docker run -v $(pwd)/..:/beammp -v $(pwd)/../build-archlinux:/build -it --rm beammp-server-build:ArchLinux bash -c "${CMD}"
docker run -v $(pwd)/..:/beammp -v $(pwd)/../build-debian-11:/build -it --rm beammp-server-build:Debian-11 bash -c "${CMD}"
