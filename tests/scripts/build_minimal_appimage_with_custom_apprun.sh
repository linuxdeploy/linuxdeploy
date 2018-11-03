#!/usr/bin/env bash

if [[ -z ${ARCH+x} ]]; then
    echo "ARCH not set."
    exit -1;
fi

LINUXDEPLOY=$1
if [[ -z ${LINUXDEPLOY+x} ]]; then
    echo "LINUXDEPLOY path not set."
    exit -1;
fi

RUNNABLE=$2
if [[ -z ${RUNNABLE+x} ]]; then
    echo "RUNNABLE path not set."
    exit -1;
fi

DESKTOP_FILE=$3
if [[ -z ${DESKTOP_FILE+x} ]]; then
    echo "DESKTOP_FILE path not set."
    exit -1;
fi

ICON=$4
if [[ -z ${ICON+x} ]]; then
    echo "ICON path not set."
    exit -1;
fi


LINUXDEPLOYDIR=`mktemp -d`
pushd "$LINUXDEPLOYDIR"

    cp "$LINUXDEPLOY" .

    wget https://github.com/TheAssassin/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-"$ARCH".AppImage
    chmod +x linuxdeploy-plugin-appimage-"$ARCH".AppImage

    filename=$(basename -- "$LINUXDEPLOY")
    LINUXDEPLOY="$LINUXDEPLOYDIR/$filename"
popd

APPDIR=`mktemp -d`

cp $RUNNABLE "$APPDIR/AppRun"

mkdir -p "$APPDIR/usr/bin"
cp "$RUNNABLE" "$APPDIR/usr/bin"

mkdir -p "$APPDIR/usr/share/applications"
cp "$DESKTOP_FILE" "$APPDIR/usr/share/applications"

mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"
cp "$ICON" "$APPDIR/usr/share/icons/hicolor/scalable/apps"

"$LINUXDEPLOY" --appdir "$APPDIR" --output=appimage
SUCCEED=$?

echo " "
echo " AppDir contents: "
find "$APPDIR"
echo " "

rm -rf "$APPDIR"
rm -rf "$LINUXDEPLOYDIR"

exit "$SUCCEED"
