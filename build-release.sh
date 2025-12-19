#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/mapper/build"

if [ -d "$BUILD_DIR" ]; then
    echo "Error: Build directory already exists: $BUILD_DIR"
    echo "Please remove it first if you want to reconfigure."
    exit 1
fi

# Disable unidecode tests (requires Boost) -- a horrible hack that skips more horrible dependencies
sed -i 's/^add_subdirectory(tests)/#add_subdirectory(tests)/' "$SCRIPT_DIR/mapper/deps/unidecode/CMakeLists.txt"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG" \
      -DBUILD_TESTING=OFF \
      -DWITHOUT_TESTS=ON \
      -DBUILD_SANDBOX=OFF \
      -DWITH_WERROR=OFF \
      -DSKIP_PERFORMANCE_COMPARISON=ON \
      ..

# Build pcre2 first to generate pcre2.h header (needed by jpcre2)
make pcre2-8-static

# Now build everything in parallel
make -j$(nproc)

echo "Release build complete. Binaries are in: $BUILD_DIR"
