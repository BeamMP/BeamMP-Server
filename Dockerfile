FROM debian:12-slim AS builder

ARG VCPKG_COMMIT=5bf0c55239da398b8c6f450818c9e28d36bf9966
ARG BUILD_PARALLEL=2
ARG ENABLE_LTO=ON

ENV DEBIAN_FRONTEND=noninteractive \
    CMAKE_BUILD_TYPE=Release \
    VCPKG_FORCE_SYSTEM_BINARIES=1 \
    VCPKG_FEATURE_FLAGS=manifests

WORKDIR /work

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates \
      curl \
      zip unzip tar \
      git \
      cmake \
      ninja-build \
      build-essential \
      pkg-config \
      liblua5.3-0 liblua5.3-dev \
      binutils \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/microsoft/vcpkg /work/vcpkg && \
    cd /work/vcpkg && \
    git checkout ${VCPKG_COMMIT} && \
    ./bootstrap-vcpkg.sh -disableMetrics

ENV VCPKG_ROOT=/work/vcpkg

COPY vcpkg.json ./
COPY deps/ ./deps/
COPY cmake/ ./cmake/
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/
COPY test/ ./test/

RUN --mount=type=cache,target=/root/.cache/vcpkg,sharing=locked \
    --mount=type=cache,target=/work/vcpkg/downloads,sharing=locked \
    --mount=type=cache,target=/work/vcpkg/buildtrees,sharing=locked \
    --mount=type=cache,target=/work/vcpkg/packages,sharing=locked \
    --mount=type=cache,target=/work/build-server,sharing=locked \
    mkdir -p /work/out && \
    cmake -S /work -B /work/build-server -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/work/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXE_LINKER_FLAGS="-Wl,--export-dynamic" \
      -DCMAKE_CXX_FLAGS="-O3 -g -Wl,-z,norelro -Wl,--hash-style=gnu -Wl,-z,noseparate-code -ffunction-sections -fdata-sections -Wl,--gc-sections" \
      -DBeamMP-Server_ENABLE_LTO=${ENABLE_LTO} \
    && cmake --build /work/build-server --parallel ${BUILD_PARALLEL} -t BeamMP-Server \
    && objcopy --only-keep-debug /work/build-server/BeamMP-Server /work/build-server/BeamMP-Server.debug \
    && strip --strip-unneeded /work/build-server/BeamMP-Server \
    && objcopy --add-gnu-debuglink=/work/build-server/BeamMP-Server.debug /work/build-server/BeamMP-Server \
    && install -m 0755 /work/build-server/BeamMP-Server /work/out/BeamMP-Server

RUN --mount=type=cache,target=/root/.cache/vcpkg,sharing=locked \
    --mount=type=cache,target=/work/vcpkg/downloads,sharing=locked \
    --mount=type=cache,target=/work/vcpkg/buildtrees,sharing=locked \
    --mount=type=cache,target=/work/vcpkg/packages,sharing=locked \
    --mount=type=cache,target=/work/build-tests,sharing=locked \
    cmake -S /work -B /work/build-tests -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/work/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXE_LINKER_FLAGS="-Wl,--export-dynamic" \
      -DBeamMP-Server_ENABLE_LTO=OFF \
    && cmake --build /work/build-tests --parallel 1 -t BeamMP-Server-tests

FROM debian:12-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    liblua5.3-0 \
    lua5.3 \
    luarocks \
    build-essential \
    pkg-config \
    liblua5.3-dev \
    git \
    unzip \
    curl \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

RUN useradd -m -u 1000 beammp && \
    mkdir -p /app /app/data /config /resources && \
    chown -R beammp:beammp /app /config /resources

# /work/build-server is a BuildKit cache mount (not persisted in layers), so copy from /work/out.
COPY --from=builder /work/out/BeamMP-Server /app/BeamMP-Server

WORKDIR /app
USER beammp

EXPOSE 30814
ENTRYPOINT ["/app/BeamMP-Server"]
CMD ["--config=/config/ServerConfig.toml"]