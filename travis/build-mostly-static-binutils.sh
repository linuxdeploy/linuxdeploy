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
wget https://ftp.gnu.org/gnu/binutils/binutils-2.35.tar.xz -O- | tar xJ --strip-components=1

# configure static build
# unfortunately, these flags have no real effect for the build, but hey, at least we try
common_flags="-static -static-libgcc -static-libstdc++"
export CFLAGS="$common_flags $CFLAGS"
export CXXFLAGS="$common_flags $CXXFLAGS"
export LDFLAGS="$common_flags $LDFLAGS"

# this flag apparently breaks down the dependency to a minimum of libc functions and allows us to run the tool
# on very old distros
./configure --prefix=/usr --with-static-standard-libraries

# build binary
make -j "$(nproc)"

# install into user-specified destdir
make install DESTDIR="$(readlink -f "$INSTALL_DESTDIR")"
