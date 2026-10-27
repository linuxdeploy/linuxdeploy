#!/usr/bin/env bash

set -euo pipefail

this_dir="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")"

apply_patch() {
    if ! git -C "$1" apply --check --reverse "$this_dir/upstream-patches/$2" 2>/dev/null; then
        git -C "$1" apply "$this_dir/upstream-patches/$2"
    fi
}

case "${1:-}" in
    linuxdeploy)
        apply_patch lib/cmake-scripts cmake-scripts/pr-1.patch
        apply_patch lib/linuxdeploy-desktopfile linuxdeploy-desktopfile/pr-11.patch
        apply_patch lib/linuxdeploy-desktopfile/lib/cmake-scripts cmake-scripts/pr-1.patch
        ;;
    linuxdeploy-plugin-appimage)
        apply_patch linuxdeploy-plugin-appimage linuxdeploy-plugin-appimage/pr-45.patch
        apply_patch linuxdeploy-plugin-appimage linuxdeploy-plugin-appimage/pr-46.patch
        ;;
    *)
        echo "Usage: $0 linuxdeploy|linuxdeploy-plugin-appimage" >&2
        exit 1
        ;;
esac
