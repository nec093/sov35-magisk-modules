#!/bin/bash
# Build system/lib64/libaudiopolicymanagercustom.so for xzs_hires_audio.
#
#   LOS_ROOT    LineageOS 18.1 source tree (clang, bionic and libc++ headers);
#               the same tree sov35-vendor is built in
#   AOSP12_HDR  AOSP android-12.1.0_r11 headers, see ../../tools/fetch-aosp12-headers.sh
#   DEVLIBS     libc.so, libdl.so and liblog.so pulled from the device's /system/lib64
#               (link targets only; adb pull /system/lib64/{libc,libdl,liblog}.so)
set -e
cd "$(dirname "$0")"
: "${LOS_ROOT:?set LOS_ROOT to a LineageOS 18.1 tree}"
: "${AOSP12_HDR:?set AOSP12_HDR (tools/fetch-aosp12-headers.sh)}"
: "${DEVLIBS:?set DEVLIBS to a directory with libc/libdl/liblog from the device}"
CL=$(ls -d "$LOS_ROOT"/prebuilts/clang/host/linux-x86/clang-r383902b1/bin)
B=$LOS_ROOT/bionic/libc
"$CL/clang++" --target=aarch64-linux-android30 -std=c++17 -O2 -fPIC -shared \
  -fno-exceptions -fno-rtti -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter \
  -nostdlib -nostdinc++ \
  -isystem "$LOS_ROOT/external/libcxx/include" -isystem "$LOS_ROOT/external/libcxxabi/include" \
  -isystem "$B/include" -isystem "$B/kernel/uapi" -isystem "$B/kernel/uapi/asm-arm64" \
  -isystem "$B/kernel/android/uapi" \
  -I "$AOSP12_HDR/system/media/audio/include" -I "$AOSP12_HDR/system/core/libcutils/include" \
  -fuse-ld=lld -Wl,-soname,libaudiopolicymanagercustom.so -Wl,-z,defs -Wl,--hash-style=gnu \
  -Wl,--version-script=src/exports.map \
  src/xzs_apm.cpp -L"$DEVLIBS" -lc -ldl -llog \
  -o system/lib64/libaudiopolicymanagercustom.so
"$CL/llvm-readelf" --dyn-syms system/lib64/libaudiopolicymanagercustom.so | grep -E "AudioPolicyManager"
