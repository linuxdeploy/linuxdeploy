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
LINUXDEPLOY_ARGS=("--init-appdir" "--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "$REPO_ROOT/resources/linuxdeploy.png" "--create-desktop-file" "-e" "/usr/bin/patchelf")

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${LINUXDEPLOY_ARGS[@]}"

# verify that an AppImage can be built
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x appimagetool-x86_64.AppImage
./appimagetool-x86_64.AppImage AppDir

# verify that the resulting AppImage works
./linuxdeploy*.AppImage "${LINUXDEPLOY_ARGS[@]}"

mv linuxdeploy*.AppImage "$OLD_CWD"
