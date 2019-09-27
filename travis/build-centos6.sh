#! /bin/bash

set -xe

mkdir build
cd build

cmake /ws -DCMAKE_INSTALL_PREFIX=/usr -DUSE_SYSTEM_CIMG=Off

make -j8

ctest -V

# args are used more than once
LINUXDEPLOY_ARGS=("--appdir" "AppDir" "-e" "bin/linuxdeploy" "-i" "/ws/resources/linuxdeploy.png" "-d" "/ws/resources/linuxdeploy.desktop" "-e" "$(which patchelf)" "-e" "$(which strip)")

# deploy patchelf which is a dependency of linuxdeploy
bin/linuxdeploy "${LINUXDEPLOY_ARGS[@]}"

# cannot add appimage plugin yet, since it won't work on CentOS 6 (at least for now)
# therefore we also need to use appimagetool directly to build an AppImage
# but we can still prepare the AppDir
APPIMAGETOOL_ARCH="$ARCH"
if [ "$APPIMAGETOOL_ARCH" == "i386" ]; then APPIMAGETOOL_ARCH="i686"; fi

wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-"$APPIMAGETOOL_ARCH".AppImage
chmod +x appimagetool-"$APPIMAGETOOL_ARCH".AppImage
sed -i 's/AI\x02/\x00\x00\x00/' appimagetool*.AppImage

appimage=linuxdeploy-centos6-"$ARCH".AppImage
./appimagetool-"$APPIMAGETOOL_ARCH".AppImage --appimage-extract
squashfs-root/AppRun AppDir "$appimage" 2>&1

chown "$OUTDIR_OWNER" "$appimage"

mv "$appimage" /out

