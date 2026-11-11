#! /bin/bash

log_message() {
    color="$1"
    shift
    if [ -t 0 ]; then tput setaf "$color"; fi
    if [ -t 0 ]; then tput bold; fi
    echo "$@"
    if [ -t 0 ]; then tput sgr0; fi
}
info() {
    log_message 2 "[info] $*"
}
warning() {
    log_message 3 "[warning] $*"
}
error() {
    log_message 1 "[error] $*"
}

set -euo pipefail

export ARCH="${ARCH:-$(uname -m)}"

this_dir="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")"

# map uname -m's names (e.g., armv7l) to those $ARCH uses, as it also names the AppImage
case "$ARCH" in
    x86_64|amd64)
        ARCH=x86_64
        docker_platform=linux/amd64
        ;;
    i?86)
        ARCH=i386
        docker_platform=linux/386
        ;;
    armhf|armv7*|armv8l)
        ARCH=armhf
        docker_platform=linux/arm/v7
        ;;
    aarch64|arm64)
        ARCH=aarch64
        docker_platform=linux/arm64/v8
        ;;
    *)
        echo "Unsupported \$ARCH: $ARCH"
        exit 3
        ;;
esac

target="${1:-${TARGET:-debian}}"
toolchain="${TOOLCHAIN:-}"
build_args=()
login_shell=

case "$target" in
    debian)
        # GNU is the only Debian toolchain, so it is left out of the image tag
        case "$toolchain" in
            ""|gnu) toolchain= ;;
            *) error "Unsupported Debian toolchain: $toolchain"
               exit 2
            ;;
        esac
        dockerfile="$this_dir/docker/Dockerfile"
        build_args+=("--build-arg" "ARCH=$ARCH")
        ;;
    alpine)
        dockerfile="$this_dir/docker/Dockerfile.Alpine"
        toolchain="${toolchain:-musl}"
        case "$toolchain" in
            musl|llvm) build_args+=("--target" "$toolchain") ;;
            *) error "Unsupported Alpine toolchain: $toolchain"
               exit 2
            ;;
        esac
        ;;
    gentoo)
        toolchain="${toolchain:-llvm}"
        case "$toolchain" in
            gnu)  gentoo_flavor=latest ;;
            musl) gentoo_flavor=musl ;;
            llvm) gentoo_flavor=musl-llvm ;;
            *) error "Unsupported Gentoo toolchain: $toolchain"
               exit 2
            ;;
        esac
        build_args+=("--build-arg" "GENTOO_FLAVOR=$gentoo_flavor")
        dockerfile="$this_dir/docker/Dockerfile.Gentoo"
        # Gentoo sets up the toolchain environment in /etc/profile.env
        login_shell=1
        ;;
    *)
        error "Unsupported target: $target"
        exit 2
        ;;
esac

# first, we need to build the image
# we always attempt to build it, it will only be rebuilt if Docker detects changes
# optionally, we'll pull the base image beforehand
info "Building $target Docker image for $ARCH (Docker platform: $docker_platform)"

if [[ "${UPDATE:-}" == "" ]]; then
    warning "\$UPDATE not set, base image will not be pulled!"
else
    build_args+=("--pull")
fi

image_tag="linuxdeploy-build-$target"
if [[ -n "$toolchain" ]]; then
    image_tag+="-$toolchain"
fi

docker build \
    -f "$dockerfile" \
    --platform "$docker_platform" \
    "${build_args[@]}" \
    -t "$image_tag" \
    "$this_dir"/docker

docker_args=()
# only if there's more than 1G of free space in RAM, we can build in a RAM disk
if [[ "${GITHUB_ACTIONS:-}" != "" ]]; then
    warning "Building on GitHub actions, which does not support --tmpfs flag -> building on regular disk"
elif [[ "$(free -m  | grep "Mem:" | awk '{print $4}')" -gt 1024 ]]; then
    info "Host system has enough free memory -> building in RAM disk"
    docker_args+=(
        "--tmpfs"
        "/docker-ramdisk:exec,mode=777"
    )
else
    warning "Host system does not have enough free memory -> building on regular disk"
fi

case "${BUILD_TYPE:-}" in
    coverage)           build_script="ci/test-coverage.sh" ;;
    dependency-tracing) build_script="ci/test-dependency-tracing.sh" ;;
    record-ldd-calls)   build_script="ci/test-dependency-tracing.sh --record $target${toolchain:+-$toolchain}" ;;
    *)                  build_script="ci/build.sh" ;;
esac


if [ -t 1 ]; then
    # needed on unixoid platforms to properly terminate the docker run process with Ctrl-C
    docker_args+=("-t")
fi

DOCKER_OPTS=()
# fix for https://stackoverflow.com/questions/51195528/rcc-error-in-resource-qrc-cannot-find-file-png
if [ "${CI:-}" != "" ]; then
    docker_args+=(
        "--security-opt"
        "seccomp:unconfined"
    )
fi

# run the build with the current user to
#   a) make sure root is not required for builds
#   b) allow the build scripts to "mv" the binaries into the /out directory
uid="${UID:-"$(id -u)"}"

# rootless Podman maps the user to a subordinate uid, which cannot write to the mounted working tree
# docker info has no .Host.Security.Rootless, so this is only true for rootless Podman
if [[ "$(docker info --format '{{.Host.Security.Rootless}}' 2>/dev/null)" == "true" ]]; then
    docker_args+=("--userns=keep-id")
fi

# pass GoogleTest's filter and sharding on, e.g., to record only some scenarios
for gtest_var in GTEST_FILTER GTEST_TOTAL_SHARDS GTEST_SHARD_INDEX; do
    if [[ -n "${!gtest_var:-}" ]]; then
        docker_args+=("-e" "$gtest_var")
    fi
done

# other distributions' /etc/profile would reset the $PATH set up by their Dockerfiles
bash_args=(-xc)
if [[ -n "$login_shell" ]]; then
    # no tracing in login shells, it would print every line of /etc/profile
    bash_args=(--login -c)
fi

info "Running build with uid $uid"
docker run \
    --rm \
    -i \
    -e GITHUB_RUN_NUMBER \
    -e ARCH \
    -e BUILD_TYPE \
    -e CI \
    --user "$uid" \
    "${docker_args[@]}" \
    -v "$(readlink -f "$this_dir"/..):/ws" \
    -w /ws \
    "$image_tag" \
    bash "${bash_args[@]}" "$build_script"
