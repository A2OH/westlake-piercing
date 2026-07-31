#!/bin/bash
# §496 — build and deploy the stub libmedia_jni.so for arm64.
#
# MediaCodecList.<clinit> calls System.loadLibrary("media_jni"). The library does not exist on this
# board and THIS RUNTIME ABORTS on a failed nativeLoad rather than throwing:
#     [PF202N] Runtime_nativeLoad path=libmedia_jni.so
#     Runtime aborting...
# Binding the natives from the bridge is not sufficient on its own, for two reasons: the load itself
# must succeed, and ART re-resolves the class's natives against the newly loaded library, dropping the
# bridge's earlier RegisterNatives. The stub therefore re-registers them from its JNI_OnLoad.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
LLVM=$WLROOT/openharmony/prebuilts/clang/ohos/linux-x86_64/llvm
NDK=$WLROOT/ohos-sdk-6.1/linux/native
OUT=${OUT:-/tmp/libmedia_jni.so}
"$LLVM/bin/clang" --target=aarch64-linux-ohos --sysroot="$NDK/sysroot" -fPIC -O2 -shared \
  -I"$WLROOT/bridge-build/aosp/libnativehelper/include_jni" \
  -o "$OUT" "$HERE/../native-tools/libmedia_jni_stub.c"
push "$OUT" "$ASX/libmedia_jni.so"
"$HDC" shell "chmod 755 $ASX/libmedia_jni.so"
echo "libmedia_jni stub deployed — expect [WESTLAKE-496] in the child log"
