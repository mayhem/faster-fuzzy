# Build stage
FROM ubuntu:24.04 AS builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    g++-14 \
    gcc-14 \
    cmake \
    git \
    libreadline-dev \
    libbsd-dev \
    liblapack-dev \
    libblas-dev \
    libopenblas-dev \
    libarmadillo-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set GCC 14 as default compiler
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100

# Copy the repository (submodules included, build dirs excluded via .dockerignore)
WORKDIR /src
COPY . .

# Build all targets using release script
RUN ./build-release.sh

# Runtime stage - smaller final image
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libreadline8 \
    libbsd0 \
    liblapack3 \
    libblas3 \
    libopenblas0 \
    libarmadillo12 \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Create target directory
RUN mkdir -p /mapper

# Copy built binaries from builder stage
COPY --from=builder /src/mapper/build/indexer /mapper/
COPY --from=builder /src/mapper/build/test /mapper/
COPY --from=builder /src/mapper/build/explore /mapper/
COPY --from=builder /src/mapper/build/create /mapper/
COPY --from=builder /src/mapper/build/server /mapper/

# Copy templates for the server
COPY --from=builder /src/mapper/templates /mapper/templates

WORKDIR /data

# Default command
CMD ["/mapper/server", "-i", "/index", "-t", "/mapper/data"]
