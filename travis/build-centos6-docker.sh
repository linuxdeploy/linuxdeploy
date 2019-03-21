#! /bin/bash

set -xe

old_cwd=$(readlink -f .)
here=$(readlink -f $(dirname "$0"))

DOCKERFILE=Dockerfile.centos6
IMAGE=linuxdeploy-build-centos6

docker build -f "$DOCKERFILE" -t "$IMAGE" .

docker run --rm -i -v "$here"/..:/ws:ro -v "$old_cwd":/out -e OUTDIR_OWNER=$(id -u) "$IMAGE" /bin/bash -xe /ws/travis/build-centos6.sh
