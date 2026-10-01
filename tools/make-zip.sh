#!/bin/bash
# Package a module directory as an installable Magisk zip.
# usage: make-zip.sh modules/<id> [OUTDIR]   ->  OUTDIR/<id>-<version>.zip
set -e
dir=${1:?module directory}
out=${2:-out}
id=$(sed -n 's/^id=//p' "$dir/module.prop")
ver=$(sed -n 's/^version=//p' "$dir/module.prop")
mkdir -p "$out"
zip=$(realpath "$out")/$id-$ver.zip
rm -f "$zip"
(cd "$dir" && zip -qr9 "$zip" . -x 'build.sh' 'src/*' 'README.md')
echo "$zip"
