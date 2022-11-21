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


if you encounter these errors `error: ‘filesystem’ in namespace ‘std’ does not name a type; did you mean ‘system’?`

either manual change `#include <filesystem>` to `#include <experimental/filesystem>` or create a soft link
find /usr -name "filesystem" 2>/dev/null


If you encounter this error

    `error: variable ‘std::array<linuxdeploy::plugin::plugin_process_handler::run(const std::filesystem::__cxx11::path&) const::pipe_to_be_logged, 2> pipes_to_be_logged’ has initializer but incomplete type
    55 |             std::array<pipe_to_be_logged, 2> pipes_to_be_logged{
      |                                              ^~~~~~~~~~~~~~~~~`

add the line below in `include/linuxdeploy/plugin/plugin_process_handler.h`

    #include <array>

