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
# inspired by https://github.com/andrew-d/static-binaries/blob/master/binutils/build.sh
./configure --prefix=/usr --disable-nls --enable-static-link --disable-shared-plugins --disable-dynamicplugin --disable-tls --disable-pie

# citing the script linked above: "This strange dance is required to get things to be statically linked."
make -j "$(nproc)"
make clean
make -j "$(nproc)" LDFLAGS="-all-static"

# install into user-specified destdir
make install DESTDIR="$(readlink -f "$INSTALL_DESTDIR")"
