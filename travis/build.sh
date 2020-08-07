#! /bin/bash

set -e
set -x

# use RAM disk if possible
if [ "$CI" == "" ] && [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" linuxdeploy-build-XXXXXX)

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

# store repo root as variable
REPO_ROOT=$(readlink -f $(dirname $(dirname $0)))
OLD_CWD=$(readlink -f .)

pushd "$BUILD_DIR"

if [ "$ARCH" == "x86_64" ]; then
    EXTRA_CMAKE_ARGS=()
elif [ "$ARCH" == "i386" ]; then
    EXTRA_CMAKE_ARGS=("-DCMAKE_TOOLCHAIN_FILE=$REPO_ROOT/cmake/toolchains/i386-linux-gnu.cmake" "-DUSE_SYSTEM_CIMG=OFF")
else
    echo "Architecture not supported: $ARCH" 1>&2
    exit 1
fi

# fetch up-to-date CMake
mkdir cmake-prefix
wget -O- https://github.com/Kitware/CMake/releases/download/v3.18.1/cmake-3.18.1-Linux-x86_64.tar.gz | tar -xz -C cmake-prefix --strip-components=1
export PATH="$(readlink -f cmake-prefix/bin):$PATH"
cmake --version

# configure build for AppImage release
cmake "$REPO_ROOT" -DSTATIC_BUILD=On -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo "${EXTRA_CMAKE_ARGS[@]}"

make -j"$(nproc)" linuxdeploy

## Run Unit Tests
ctest -V

# build patchelf
"$REPO_ROOT"/travis/build-static-patchelf.sh "$(readlink -f out/)"
patchelf_path="$(readlink -f out/usr/bin/patchelf)"

# build custom strip
"$REPO_ROOT"/travis/build-mostly-static-binutils.sh "$(readlink -f out/)"
strip_path="$(readlink -f out/usr/bin/strip)"

# use tools we just built for linuxdeploy run
export PATH="$(readlink -f out/usr/bin):$PATH"

# args are used more than once
LINUXDEPLOY_ARGS=("--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "$REPO_ROOT/resources/linuxdeploy.png" "-d" "$REPO_ROOT/resources/linuxdeploy.desktop" "-e" "$patchelf_path" "-e" "$strip_path")

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${LINUXDEPLOY_ARGS[@]}"

# bundle AppImage plugin
mkdir -p AppDir/plugins

wget https://github.com/TheAssassin/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-"$ARCH".AppImage
chmod +x linuxdeploy-plugin-appimage-"$ARCH".AppImage
./linuxdeploy-plugin-appimage-"$ARCH".AppImage --appimage-extract
mv squashfs-root/ AppDir/plugins/linuxdeploy-plugin-appimage

ln -s ../../plugins/linuxdeploy-plugin-appimage/AppRun AppDir/usr/bin/linuxdeploy-plugin-appimage

export UPD_INFO="gh-releases-zsync|linuxdeploy|linuxdeploy|continuous|linuxdeploy-$ARCH.AppImage.zsync"
export OUTPUT="linuxdeploy-$ARCH.AppImage"

# build AppImage using plugin
AppDir/usr/bin/linuxdeploy-plugin-appimage --appdir AppDir/

# rename AppImage to avoid "Text file busy" issues when using it to create another one
mv "$OUTPUT" test.AppImage

# verify that the resulting AppImage works
./test.AppImage "${LINUXDEPLOY_ARGS[@]}"

# check whether AppImage plugin is found and works
./test.AppImage "${LINUXDEPLOY_ARGS[@]}" --output appimage

mv "$OUTPUT"* "$OLD_CWD"/
