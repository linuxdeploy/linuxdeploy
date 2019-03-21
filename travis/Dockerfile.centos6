FROM centos:6

RUN yum install -y centos-release-scl-rh && \
    yum install -y devtoolset-6 wget curl patchelf vim-common fuse libfuse2 libtool autoconf automake zlib-devel libjpeg-devel libpng-devel nano git && \
    wget https://github.com/Kitware/CMake/releases/download/v3.13.4/cmake-3.13.4-Linux-x86_64.tar.gz -O- | tar xz --strip-components=1 -C/usr/local

RUN wget http://springdale.math.ias.edu/data/puias/computational/6/x86_64//patchelf-0.8-2.sdl6.x86_64.rpm && \
    echo "3d746306f5f7958b9487e6d08f53bb13  patchelf-0.8-2.sdl6.x86_64.rpm" || md5sum -c && \
    rpm -i patchelf-0.8-2.sdl6.x86_64.rpm

