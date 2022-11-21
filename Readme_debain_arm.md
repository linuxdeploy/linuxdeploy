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



If you encounnter any of these errors:

    /usr/bin/ld: cannot find -lz
    /usr/bin/ld: cannot find -ljpeg
    /usr/bin/ld: cannot find -lm
    /usr/bin/ld: cannot find -lc
    /usr/bin/ld: cannot find -lpng



    install theses dependencies

    sudo libpng-static libstdc++-static glibc-static zlib-static wine-devel
