#! /bin/bash

set -xe

# get a compiler that allows for using modern-ish C++ (>= 11) on a distro that doesn't normally support it
# before you ask: yes, the binaries will work on CentOS 6 even without devtoolset (they somehow partially link C++
# things statically while using others from the system...)
# so, basically, it's magic!
. /opt/rh/devtoolset-6/enable

mkdir build
cd build

cmake /ws -DCMAKE_INSTALL_PREFIX=/usr -DUSE_SYSTEM_CIMG=Off

make -j8

ctest -V

# args are used more than once
LINUXDEPLOY_ARGS=("--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "/ws/resources/linuxdeploy.png" "-d" "/ws/resources/linuxdeploy.desktop" "-e" "$(which patchelf)" "-e" "$(which strip)")

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${LINUXDEPLOY_ARGS[@]}"

tar cfvz /out/appdir.tgz AppDir

# cannot add appimage plugin yet, since it won't work on CentOS 6 (at least for now)
# therefore we also need to use appimagetool directly to build an AppImage
# but we can still prepare the AppDir
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x appimagetool-x86_64.AppImage
sed -i 's/AI\x02/\x00\x00\x00/' appimagetool*.AppImage

appimage=linuxdeploy-centos6-x86_64.AppImage
./appimagetool-x86_64.AppImage --appimage-extract
squashfs-root/AppRun AppDir "$appimage" 2>&1

chown "$OUTDIR_OWNER" "$appimage"

mv "$appimage" /out

