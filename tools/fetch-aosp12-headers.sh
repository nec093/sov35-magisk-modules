#!/bin/bash
# Shallow-clone the AOSP projects whose headers the modules on this branch are
# built against. usage: fetch-aosp12-headers.sh [DEST] (default ./aosp12-hdr)
set -e
TAG=android-12.1.0_r11   # phh GSI SQ3A.220705.003.A1
DEST=${1:-aosp12-hdr}
mkdir -p "$DEST" && cd "$DEST"
for r in system/media system/core; do
  [ -d "$r/.git" ] && continue
  mkdir -p "$(dirname "$r")"
  git clone -q --depth 1 -b "$TAG" "https://android.googlesource.com/platform/$r" "$r"
done
echo "AOSP12_HDR=$(pwd)"
