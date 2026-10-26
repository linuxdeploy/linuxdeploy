#!/usr/bin/env bash

set -euo pipefail

# usage: [BUILD_DIR=<dir>] test-dependency-tracing.sh [--record <platform>], run it from the repository root
# --record runs only the real tests, and saves the ldd calls they make for the mock tests, see
# tests/core/ldd_recordings/README.md
record_platform=
if [ "$#" -gt 0 ]; then
    if [ "$#" -ne 2 ] || [ "$1" != "--record" ] || [ -z "$2" ]; then
        echo "Usage: $0 [--record <platform>] (e.g., debian, alpine-musl or freebsd)" >&2
        exit 2
    fi
    record_platform="$2"
fi

# reuse and keep $BUILD_DIR if set (e.g., between local runs), otherwise use a temporary directory
if [ -n "${BUILD_DIR:-}" ]; then
    mkdir -p "$BUILD_DIR"
    build_dir="$(cd "$BUILD_DIR" && pwd)"
else
    # use RAM disk if possible
    if [ -d /docker-ramdisk ]; then
        TEMP_BASE=/docker-ramdisk
    else
        TEMP_BASE=${TEMP_BASE:-/tmp}
    fi

    build_dir=$(mktemp -d -p "$TEMP_BASE" linuxdeploy-build-XXXXXX)
    trap 'rm -rf "$build_dir"' EXIT
fi
echo "build_dir: $build_dir"

echo "## build"

# the toolchain is selected through the environment (e.g., $CC, $CXXFLAGS and $LDFLAGS), which CMake reads by itself
cmake \
    -S . \
    -B "$build_dir" \
    -G Ninja \
    -DBUILD_TESTING=ON \
    -DSTATIC_BUILD=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    ${FORCE_BUILD_GTEST:+"-DFORCE_BUILD_GTEST=$FORCE_BUILD_GTEST"}

if [ -n "$record_platform" ]; then
    cmake --build "$build_dir" --target test_dependency_tracing_real

    echo "## record $record_platform"

    recordings_dir="tests/core/ldd_recordings/$record_platform"
    # inside the build directory, so that it is removed with it, and emptied in case the directory is reused
    recording_dir="$build_dir/recordings"
    rm -rf "$recording_dir"

    status=0
    LINUXDEPLOY_LDD_RECORD_DIR="$recording_dir" "$build_dir/tests/core/test_dependency_tracing_real" || status=$?

    if ! compgen -G "$recording_dir/*.txt" > /dev/null; then
        echo "No recordings were saved" >&2
        exit 1
    fi

    mkdir -p "$recordings_dir"

    # GoogleTest runs only some scenarios if filtered or sharded through the environment
    partial_run=
    if [ -n "${GTEST_FILTER:-}" ] || [ -n "${GTEST_TOTAL_SHARDS:-}" ] || [ -n "${GTEST_SHARD_INDEX:-}" ]; then
        partial_run=1
    fi

    if [ "$status" -eq 0 ] && [ -z "$partial_run" ]; then
        # replace all recordings, so those of removed or renamed scenarios do not remain
        rm -f "${recordings_dir:?}"/*.txt
        cp "$recording_dir"/*.txt "$recordings_dir"/
        echo "## recordings saved to $recordings_dir"
    elif [ "$status" -eq 0 ]; then
        # keep the recordings of the scenarios which did not run
        cp "$recording_dir"/*.txt "$recordings_dir"/
        echo "## filtered or sharded run, recordings of the scenarios which ran saved to $recordings_dir"
    else
        # the tests may have stopped early, so keep the recordings of the scenarios which did not run
        cp "$recording_dir"/*.txt "$recordings_dir"/
        echo "## the real tests failed, recordings of the scenarios which ran saved to $recordings_dir" >&2
    fi

    exit "$status"
fi

cmake --build "$build_dir"

echo "## tests"

# failing tests print the ldd calls they made, see tests/core/test_dependency_tracing_real.cpp
# run all tests, as other code depends on the platform, too (e.g., exec*'s search rules, llvm-strip with LLVM)
ctest -V --test-dir "$build_dir"

echo "## integration test"

appdir="$build_dir/appdir"
linuxdeploy_log="$build_dir/linuxdeploy.log"

simple_bin="$build_dir/tests/simple_executable/simple_executable"
simple_lib="$build_dir/tests/simple_library/libsimple_library.so"
parent_lib="$build_dir/tests/dependency_parent/libdependency_parent.so"
leaf_lib="$build_dir/tests/dependency_leaf/libdependency_leaf.so"

# print everything needed to investigate a failure
on_error() {
    set +x
    echo "## integration test failed"
    for file in "$simple_bin" "$simple_lib" "$parent_lib" "$leaf_lib"; do
        echo "### ldd $file"
        ldd "$file" || true
    done
    if [ -f "$linuxdeploy_log" ]; then
        echo "### linuxdeploy log"
        cat "$linuxdeploy_log"
    fi
    if [ -d "$appdir" ]; then
        echo "### AppDir"
        find "$appdir" | sort
    fi
}
trap on_error ERR

echo "## linuxdeploy"

# a reused build directory may contain the AppDir of an earlier run
rm -rf "$appdir"
"$build_dir/bin/linuxdeploy" -v0 \
    --appdir "$appdir" \
    --executable "$simple_bin" \
    > "$linuxdeploy_log" 2>&1

echo "## check appdir"

set -x
test -x "$appdir/usr/bin/simple_executable"
for library in libsimple_library libdependency_parent libdependency_leaf; do
    test -f "$appdir/usr/lib/$library.so"
done
# musl's combined libc/loader must not be deployed, regardless of which name ldd reports it with (#340)
for runtime_object in "$appdir"/usr/lib/{ld-musl-*,libc.musl-*,libc.so,libc_musl.so,ldd}; do
    if [ -e "$runtime_object" ]; then
        echo "musl runtime object was deployed: $runtime_object" >&2
        # unlike exit, false triggers the ERR trap (on_error)
        false
    fi
done
set +x

echo "## run simple_executable from appdir:"

if env -u LD_LIBRARY_PATH "$appdir/usr/bin/simple_executable"; then
    result=0
else
    result=$?
fi

printf "\n\nExit status: %d\n" "$result"

[ "$result" -eq 0 ]

echo "## integration test passed"
