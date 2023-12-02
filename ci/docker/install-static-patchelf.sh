#! /bin/bash

set -e
set -x

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" linuxdeploy-build-XXXXXX)

pushd "$BUILD_DIR"

# fetch source code
git clone https://github.com/NixOS/patchelf.git .

# cannot use -b since it's not supported in really old versions of git
git checkout 0.15.0

# prepare configure script
./bootstrap.sh

# configure static build
env LDFLAGS="-static -static-libgcc -static-libstdc++" ./configure --prefix=/usr

# build binary
make -j "$(nproc)"

# install into separate destdir to avoid polluting the $PATH with tools like ld that will break things
make install DESTDIR="${TOOLS_DIR}"
