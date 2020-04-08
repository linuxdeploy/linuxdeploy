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

cmake "$REPO_ROOT" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo "${EXTRA_CMAKE_ARGS[@]}"

make -j$(nproc)

## Run Unit Tests
ctest -V

strip_path=$(which strip)

if [ "$ARCH" == "i386" ]; then
    # download i386 strip for i386 AppImage
    # https://github.com/linuxdeploy/linuxdeploy/issues/59
    wget http://security.ubuntu.com/ubuntu/pool/main/b/binutils/binutils-multiarch_2.24-5ubuntu14.2_i386.deb
    echo "0106f170cebf5800e863a558cad039e4f16a76d3424ae943209c3f6b0cacd511  binutils-multiarch_2.24-5ubuntu14.2_i386.deb" | sha256sum -c
    wget http://security.ubuntu.com/ubuntu/pool/main/b/binutils/binutils-multiarch-dev_2.24-5ubuntu14.2_i386.deb
    echo "ed9ca4fbbf492233228f79fae6b349a2ed2ee3e0927bdc795425fccf5fae648e  binutils-multiarch-dev_2.24-5ubuntu14.2_i386.deb" | sha256sum -c
    dpkg -x binutils-multiarch_2.24-5ubuntu14.2_i386.deb out/
    dpkg -x binutils-multiarch-dev_2.24-5ubuntu14.2_i386.deb out/
    rm binutils-multiarch*.deb
    strip_path=$(readlink -f out/usr/bin/strip)
    export LD_LIBRARY_PATH=$(readlink -f out/usr/lib)
fi

# args are used more than once
LINUXDEPLOY_ARGS=("--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "$REPO_ROOT/resources/linuxdeploy.png" "-d" "$REPO_ROOT/resources/linuxdeploy.desktop" "-e" "/usr/bin/patchelf" "-e" "$strip_path")

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
export OUTPUT="linuxdeploy-devbuild-$ARCH.AppImage"

# build AppImage using plugin
AppDir/usr/bin/linuxdeploy-plugin-appimage --appdir AppDir/

# rename AppImage to avoid "Text file busy" issues when using it to create another one
mv "$OUTPUT" test.AppImage

# verify that the resulting AppImage works
./test.AppImage "${LINUXDEPLOY_ARGS[@]}"

# check whether AppImage plugin is found and works
./test.AppImage "${LINUXDEPLOY_ARGS[@]}" --output appimage

mv "$OUTPUT"* "$OLD_CWD"/
