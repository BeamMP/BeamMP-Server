# Build stage
FROM debian:12-slim AS builder

# Build environment variables
ENV VCPKG_BINARY_SOURCES="clear;x-gha,readwrite" \
    VCPKG_FORCE_SYSTEM_BINARIES=1 \
    CMAKE_BUILD_TYPE=Release \
    DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update -y && \
    apt-get install -y \
    liblua5.3-0 \
    liblua5.3-dev \
    curl \
    zip \
    unzip \
    tar \
    cmake \
    make \
    git \
    g++ \
    ninja-build \
    binutils \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source files
COPY .git .git
COPY vcpkg vcpkg

# Initialize Git submodules (vcpkg, etc.) if needed
# If vcpkg doesn't exist, clone it directly
RUN git submodule update --init --recursive || true; \

# Copy source files
COPY . .

# Bootstrap vcpkg
RUN ./vcpkg/bootstrap-vcpkg.sh

# Configure CMake
RUN cmake . -B bin \
    -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -g -Wl,-z,norelro -Wl,--hash-style=gnu -Wl,-z,noseparate-code -ffunction-sections -fdata-sections -Wl,--gc-sections" \
    -DBeamMP-Server_ENABLE_LTO=ON

# Build server
RUN cmake --build bin --parallel -t BeamMP-Server

# Extract debug information
RUN objcopy --only-keep-debug bin/BeamMP-Server bin/BeamMP-Server.debug && \
    objcopy --add-gnu-debuglink bin/BeamMP-Server bin/BeamMP-Server.debug

# Strip binary
RUN strip -s bin/BeamMP-Server

# Lightweight runtime stage
FROM debian:12-slim

# Install runtime dependencies only
RUN apt-get update -y && \
    apt-get install -y \
    liblua5.3-0 \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN useradd -m -u 1000 beammp && \
    mkdir -p /app /config /resources && \
    chown -R beammp:beammp /app /config /resources

# Copy binary from build stage
COPY --from=builder /build/bin/BeamMP-Server /app/BeamMP-Server

# Set working directory
WORKDIR /app

# Non-root user
USER beammp

# Expose default port (can be overridden)
EXPOSE 30814

# Entry point
ENTRYPOINT ["/app/BeamMP-Server"]

# Default arguments (can be overridden)
CMD ["--config=/config/ServerConfig.toml", "--working-directory=/app"]

