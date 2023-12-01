#! /bin/bash

set -e
set -x

# use RAM disk if possible
if [ "$CI" == "" ] && [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

build_dir=$(mktemp -d -p "$TEMP_BASE" linuxdeploy-build-XXXXXX)

cleanup () {
    if [ -d "$build_dir" ]; then
        rm -rf "$build_dir"
    fi
}

trap cleanup EXIT

# store repo root as variable
repo_root="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")"/..
old_cwd="$(readlink -f "$PWD")"

pushd "$build_dir"

extra_cmake_args=()

case "$ARCH" in
    "x86_64")
        ;;
    "i386")
        echo "Enabling x86_64->i386 cross-compile toolchain"
        extra_cmake_args=("-DCMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/i386-linux-gnu.cmake" "-DUSE_SYSTEM_CIMG=OFF")
        ;;
    *)
        echo "Architecture not supported: $ARCH" 1>&2
        exit 1
    ;;
esac

# fetch up-to-date CMake
mkdir cmake-prefix
wget -O- https://github.com/Kitware/CMake/releases/download/v3.18.1/cmake-3.18.1-Linux-x86_64.tar.gz | tar -xz -C cmake-prefix --strip-components=1
export PATH="$(readlink -f cmake-prefix/bin):$PATH"
cmake --version

# configure build for AppImage release
cmake "$repo_root" -DSTATIC_BUILD=On -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo "${extra_cmake_args[@]}"

make -j"$(nproc)"

# build patchelf
"$repo_root"/ci/build-static-patchelf.sh "$(readlink -f out/)"
patchelf_path="$(readlink -f out/usr/bin/patchelf)"

# build custom strip
"$repo_root"/ci/build-static-binutils.sh "$(readlink -f out/)"
strip_path="$(readlink -f out/usr/bin/strip)"

# use tools we just built for linuxdeploy test run
PATH="$(readlink -f out/usr/bin):$PATH"
export PATH

## Run Unit Tests
ctest -V

# args are used more than once
linuxdeploy_args=("--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "$repo_root/resources/linuxdeploy.png" "-d" "$repo_root/resources/linuxdeploy.desktop" "-e" "$patchelf_path" "-e" "$strip_path")

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${linuxdeploy_args[@]}"

# bundle AppImage plugin
mkdir -p AppDir/plugins

wget https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-"$ARCH".AppImage
chmod +x linuxdeploy-plugin-appimage-"$ARCH".AppImage
./linuxdeploy-plugin-appimage-"$ARCH".AppImage --appimage-extract
mv squashfs-root/ AppDir/plugins/linuxdeploy-plugin-appimage

ln -s ../../plugins/linuxdeploy-plugin-appimage/AppRun AppDir/usr/bin/linuxdeploy-plugin-appimage

# interpreted by linuxdeploy-plugin-appimage
export UPD_INFO="gh-releases-zsync|linuxdeploy|linuxdeploy|continuous|linuxdeploy-$ARCH.AppImage.zsync"
export OUTPUT="linuxdeploy-$ARCH.AppImage"

# special set of builds using a different experimental runtime, used for testing purposes
if [[ "$USE_STATIC_RUNTIME" != "" ]]; then
    export OUTPUT="linuxdeploy-static-$ARCH.AppImage"
    wget https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-"$ARCH"
    runtime_filename="$(echo "$CUSTOM_RUNTIME_URL" | rev | cut -d/ -f1 | rev)"
    export LDAI_RUNTIME_FILE"$(readlink -f "$runtime_filename")"
fi

# build AppImage using plugin
AppDir/usr/bin/linuxdeploy-plugin-appimage --appdir AppDir/

# rename AppImage to avoid "Text file busy" issues when using it to create another one
mv "$OUTPUT" test.AppImage

# verify that the resulting AppImage works
./test.AppImage "${linuxdeploy_args[@]}"

# check whether AppImage plugin is found and works
./test.AppImage "${linuxdeploy_args[@]}" --output appimage

# using a glob because we want to include the .zsync file
mv "$OUTPUT"* "$old_cwd"/
