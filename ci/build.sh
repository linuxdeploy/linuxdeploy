#! /bin/bash

set -euxo pipefail

# use RAM disk if possible
if [ -d /docker-ramdisk ]; then
    TEMP_BASE=/docker-ramdisk
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

# work around ninja colors bug
extra_cmake_args=()
if [ -t 0 ]; then
    extra_cmake_args+=(
        "-DCMAKE_C_FLAGS=-fdiagnostics-color=always"
        "-DCMAKE_CXX_FLAGS=-fdiagnostics-color=always"
    )
fi

# configure build for AppImage release
cmake \
    -G Ninja \
    "$repo_root" \
    -DSTATIC_BUILD=ON \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    "${extra_cmake_args[@]}"

nprocs="$(nproc)"
[[ "${CI:-}" == "" ]] && [[ "$nprocs" -gt 2 ]] && nprocs="$(nproc --ignore=1)"

ninja -j"$nprocs" -v

## Run Unit Tests
ctest -V

# args are used more than once
patchelf_path="$(which patchelf)"
strip_path="$(which strip)"
linuxdeploy_args=(
    --appdir AppDir
    -e bin/linuxdeploy
    -i "$repo_root/resources/linuxdeploy.png"
    -d "$repo_root/resources/linuxdeploy.desktop"
    -e "$patchelf_path"
    -e "$strip_path"
)

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${linuxdeploy_args[@]}"

# bundle AppImage plugin
mkdir -p AppDir/plugins

# build linuxdeploy-plugin-appimage instead of using prebuilt versions
# this prevents a circular dependency
# the other repository provides a script for this purpose that builds a bundle we can use
git clone --recursive https://github.com/linuxdeploy/linuxdeploy-plugin-appimage
bash linuxdeploy-plugin-appimage/ci/build-bundle.sh
mv linuxdeploy-plugin-appimage-bundle AppDir/plugins/linuxdeploy-plugin-appimage

ln -s ../../plugins/linuxdeploy-plugin-appimage/usr/bin/linuxdeploy-plugin-appimage AppDir/usr/bin/linuxdeploy-plugin-appimage

# interpreted by linuxdeploy-plugin-appimage
export UPD_INFO="gh-releases-zsync|linuxdeploy|linuxdeploy|continuous|linuxdeploy-$ARCH.AppImage.zsync"
export OUTPUT="linuxdeploy-$ARCH.AppImage"

# special set of builds using a different experimental runtime, used for testing purposes
if [[ "${USE_STATIC_RUNTIME:-}" != "" ]]; then
    custom_runtime_url="https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-$ARCH"
    wget "$custom_runtime_url"
    runtime_filename="$(echo "$custom_runtime_url" | rev | cut -d/ -f1 | rev)"
    LDAI_RUNTIME_FILE="$(readlink -f "$runtime_filename")"
    export LDAI_RUNTIME_FILE
    export OUTPUT="linuxdeploy-static-$ARCH.AppImage"
fi

# build AppImage using plugin
AppDir/usr/bin/linuxdeploy-plugin-appimage --appdir AppDir/

# rename AppImage to avoid "Text file busy" issues when using it to create another one
mv "$OUTPUT" test.AppImage

# qemu is not happy about the AppImage type 2 magic bytes, so we need to "fix" that
dd if=/dev/zero bs=1 count=3 seek=8 conv=notrunc of=test.AppImage

# verify that the resulting AppImage works
./test.AppImage "${linuxdeploy_args[@]}"

# check whether AppImage plugin is found and works
./test.AppImage "${linuxdeploy_args[@]}" --output appimage

# using a glob because we want to include the .zsync file
mv "$OUTPUT"* "$old_cwd"/
