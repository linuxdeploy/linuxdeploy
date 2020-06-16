#! /bin/bash

set -xe

old_cwd=$(readlink -f .)
here=$(readlink -f $(dirname "$0"))

if [ -z "$ARCH" ]; then
    echo '$ARCH not set' 1>&2
    exit 1
fi

DOCKERFILE="$here/Dockerfile.$ARCH"
IMAGE="linuxdeploy-build-$ARCH"

if [ ! -f "$DOCKERFILE" ]; then
    echo "Architecture not supported: $ARCH" 1>&2
    exit 1
fi

(cd "$here" && docker build -f "$DOCKERFILE" -t "$IMAGE" .)

docker run --rm -it -v "$here"/..:/ws:ro -v "$old_cwd":/out -e CI=1 -e OUTDIR_OWNER=$(id -u) "$IMAGE" /bin/bash -xe -c "cd /out && /ws/travis/build-in-docker.sh"
