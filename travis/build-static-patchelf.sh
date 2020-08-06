#! /bin/bash

set -e
set -x

INSTALL_DESTDIR="$1"

if [[ "$INSTALL_DESTDIR" == "" ]]; then
    echo "Error: build dir $BUILD_DIR does not exist" 1>&2
    exit 1
fi

# support cross-compilation for 32-bit ISAs
case "$ARCH" in
    "x86_64"|"amd64")
        ;;
    "i386"|"i586"|"i686")
        export CFLAGS="-m32"
        export CXXFLAGS="-m32"
        ;;
    *)
        echo "Error: unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# use RAM disk if possible
if [ "$CI" == "" ] && [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

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
git checkout 0.8

# prepare configure script
./bootstrap.sh

# configure static build
env LDFLAGS="-static -static-libgcc -static-libstdc++" ./configure --prefix=/usr

# build binary
make -j "$(nproc)"

# install into user-specified destdir
make install DESTDIR="$INSTALL_DESTDIR"
