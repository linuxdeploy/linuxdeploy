#! /bin/bash

set -euxo pipefail

ARCH="${ARCH:-$(uname -m)}"

# map uname -m's names to those $ARCH uses, like build-in-docker.sh
case "$ARCH" in
    amd64) ARCH=x86_64 ;;
    i?86) ARCH=i386 ;;
    armv7*|armv8l) ARCH=armhf ;;
    arm64) ARCH=aarch64 ;;
esac

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

# CMake 4 rejects projects which require CMake < 3.5, like the googletest cmake-scripts fetches and
# linuxdeploy-plugin-appimage; as an environment variable, it also reaches the latter's build
export CMAKE_POLICY_VERSION_MINIMUM=3.5

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
    ${FORCE_BUILD_GTEST:+"-DFORCE_BUILD_GTEST=$FORCE_BUILD_GTEST"} \
    "${extra_cmake_args[@]}"

nprocs="$(nproc)"
[[ "${CI:-}" == "" ]] && [[ "$nprocs" -gt 2 ]] && nprocs="$(nproc --ignore=1)"

ninja -j"$nprocs" -v

## Run Unit Tests
ctest -V

# args are used more than once
# bundle the patchelf and strip linuxdeploy uses, which $PATCHELF and $STRIP may select (e.g., STRIP=llvm-strip)
patchelf_path="$(which "${PATCHELF:-patchelf}")"
strip_path="$(which "${STRIP:-strip}")"
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

# linuxdeploy looks for the bundled tools by their default names (llvm-strip works when called strip)
for tool in patchelf strip; do
    tool_path_var="${tool}_path"
    tool_name="$(basename "${!tool_path_var}")"
    if [[ "$tool_name" != "$tool" ]]; then
        ln -sf "$tool_name" AppDir/usr/bin/"$tool"
    fi
done

# bundle AppImage plugin
mkdir -p AppDir/plugins

if [ $# -ge 1 ] && [ "$1" = "--skip-plugins" ]; then
    exit 0
fi

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
