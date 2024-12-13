# generic Dockerfile for all architectures
# used to "cache" prebuilt binaries of tools we use internally and installed dependencies
# needs to be re-run in CI every time as we cannot store auto-built Docker images due to GitHub's strict quota
# it will save a lot of time in local development environments, though

# we'll just use Debian as a base image for now, mainly because it produces less headache than Ubuntu with arm
# a big pro is that they ship an up to date CMake in their stable distribution
# also, they still provide IA-32 builds for some reason...
# some people in the AppImage community do not (want to) realize i386 is dead for good
# we are going to drop i686 in the future!
FROM debian:stable

# variables that need to be availabe during build and runtime must(!) be repeated after FROM
ARG ARCH

ENV APPIMAGE_EXTRACT_AND_RUN=1

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        file \
        xz-utils \
        wget \
        make \
        ca-certificates \
        ninja-build \
        gcc \
        g++ \
        gcovr \
        libmagic-dev \
        libjpeg-dev \
        libpng-dev \
        git \
        autoconf \
        automake \
        libfuse2 \
        cimg-dev \
        cmake \
        googletest \
        gdb \
        libc6-dev \
        build-essential \
        python3-minimal \
        python-is-python3 && \
    apt-get clean

# install into separate destdir to avoid polluting the $PATH with tools like ld that will break things
ENV TOOLS_DIR=/tools

COPY install-static-binutils.sh /
RUN bash /install-static-binutils.sh

COPY install-static-patchelf.sh /
RUN bash /install-static-patchelf.sh

# make patchelf and strip available in $PATH
# they are static binaries, so we can just copy them
RUN cp "$TOOLS_DIR"/usr/bin/patchelf /usr/local/bin && \
    cp "$TOOLS_DIR"/usr/bin/strip /usr/local/bin
