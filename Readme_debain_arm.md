# Debain/Fedora aarch64
Clone projects

    git clone https://github.com/kevinmukuna/linuxdeploy.git
    cd linuxdeploy
    git checkout arm64-builds
    git submodule update --init --recursive
    mkdir build
    cmake -S . -B build/

Prerequisite Debain

    sudo apt install -y cmake git patchelf ccache pkg-config libpng-dev libjpeg-dev

Prerequisite Fedora/Centos

    sudo yum install -y cmake git patchelf ccache pkg-config libpng libjpeg



