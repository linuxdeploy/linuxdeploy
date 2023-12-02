#! /bin/bash

set -euxo pipefail

# use RAM disk if possible
if [ -d /docker-ramdisk ]; then
    TEMP_BASE=/docker-ramdisk
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
REPO_ROOT="$(readlink -f "$(dirname "$(dirname "${BASH_SOURCE[0]}")")")"

pushd "$BUILD_DIR"

cmake "$REPO_ROOT" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

nprocs="$(nproc)"
[[ "${CI:-}" == "" ]] && [[ "$nprocs" -gt 2 ]] && nprocs="$(nproc --ignore=1)"

# build, run tests and show coverage report
make -j"$nprocs" coverage_text
