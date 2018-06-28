#! /bin/bash

set -e
set -x

# use RAM disk if possible
if [ "$CI" == "" ] && [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" AppImageUpdate-build-XXXXXX)

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

cmake "$REPO_ROOT"

make VERBOSE=1

# args are used more than once
LINUXDEPLOY_ARGS=("--init-appdir" "--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "$REPO_ROOT/resources/linuxdeploy.png" "--create-desktop-file" "-e" "/usr/bin/patchelf" "-e" "/usr/bin/strip")

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${LINUXDEPLOY_ARGS[@]}"

# bundle AppImage plugin
wget https://github.com/TheAssassin/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage
chmod +x linuxdeploy-plugin-appimage*.AppImage
mv linuxdeploy-plugin-appimage*.AppImage AppDir/usr/bin/

# build AppImage using plugin
AppDir/usr/bin/linuxdeploy-plugin-appimage*.AppImage --appdir AppDir/

# rename AppImage to avoid "Text file busy" issues when using it to create another one
mv ./linuxdeploy*.AppImage test.AppImage

# verify that the resulting AppImage works
./test.AppImage "${LINUXDEPLOY_ARGS[@]}"

# check whether AppImage plugin is found and works
./test.AppImage "${LINUXDEPLOY_ARGS[@]}" --output appimage

mv linuxdeploy*.AppImage "$OLD_CWD"
