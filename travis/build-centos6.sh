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
REPO_ROOT=$(readlink -f $(dirname $(dirname "$0")))
OLD_CWD=$(readlink -f .)

pushd "$BUILD_DIR"

cmake "$REPO_ROOT" -DCMAKE_INSTALL_PREFIX=/usr -DUSE_SYSTEM_CIMG=Off -DUSE_CCACHE=Off

make -j$(nproc)

ctest -V

# args are used more than once
LINUXDEPLOY_ARGS=("--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "/ws/resources/linuxdeploy.png" "-d" "/ws/resources/linuxdeploy.desktop" "-e" "$(which patchelf)" "-e" "$(which strip)")

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${LINUXDEPLOY_ARGS[@]}"

# bundle AppImage plugin
mkdir -p AppDir/plugins

wget https://github.com/TheAssassin/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-"$ARCH".AppImage
chmod +x linuxdeploy-plugin-appimage-"$ARCH".AppImage
sed -i 's|AI\x02|\x00\x00\x00|' linuxdeploy*.AppImage
./linuxdeploy-plugin-appimage-"$ARCH".AppImage --appimage-extract
mv squashfs-root/ AppDir/plugins/linuxdeploy-plugin-appimage

ln -s ../../plugins/linuxdeploy-plugin-appimage/AppRun AppDir/usr/bin/linuxdeploy-plugin-appimage

export UPD_INFO="gh-releases-zsync|linuxdeploy|linuxdeploy|continuous|linuxdeploy-$ARCH.AppImage.zsync"
export OUTPUT=linuxdeploy-"$ARCH".AppImage

# build AppImage using plugin
AppDir/usr/bin/linuxdeploy-plugin-appimage --appdir AppDir/

# rename AppImage to avoid "Text file busy" issues when using it to create another one
mv "$OUTPUT" test.AppImage
# also have to patch our test AppImage, but we can leave the newly produced one untouched
sed -i 's|AI\x02|\x00\x00\x00|' test.AppImage

# verify that the resulting AppImage works
./test.AppImage --appimage-extract-and-run "${LINUXDEPLOY_ARGS[@]}"

# check whether AppImage plugin is found and works
./test.AppImage --appimage-extract-and-run "${LINUXDEPLOY_ARGS[@]}" --output appimage

chown "$OUTDIR_OWNER" "$OUTPUT"

mv "$OUTPUT"* "$OLD_CWD"/

