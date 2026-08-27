#!/bin/bash
# build_appspawnx_arm64.sh — build appspawn-x for aarch64 against OHOS 6.1.0.31.
# Mirrors the arm32 compile_appspawnx.sh but: OHOS 6.1 SDK toolchain+sysroot,
# aarch64 target, 6.1 inner-API headers from the sparse-synced source tree, AOSP
# libnativehelper headers, my arm64 libart. COMPILE phase validates 6.1 header
# compatibility (surfaces API drift = the porting work); LINK pulls board .so's.
set -o pipefail
OHSRC=$WLROOT/ohos-6.1-src                       # sparse-synced 6.1 inner-API headers
NDK=$WLROOT/ohos-sdk-6.1/linux/native            # 6.1 SDK: sysroot (musl libc) ONLY
SYSROOT=$NDK/sysroot
# WESTLAKE §649 (2026-08-15): build with the OHOS PREBUILT clang + libcxx-ohos, exactly like
# build_bridge_arm64.sh. The SDK/NDK clang emits the `std::__n1` libc++ inline namespace, but the
# board's OHOS libraries (and our libart/bridge) use `std::__h`, so linking produced
#   ld.lld: error: undefined symbol: std::__n1::basic_string<...>::append(char const*, unsigned long)
# and appspawn-x could not be rebuilt at all. Same fix, same reason, as the bridge.
OHOS_LLVM=$WLROOT/openharmony/prebuilts/clang/ohos/linux-x86_64/llvm
LIBCXX="-nostdinc++ -isystem $OHOS_LLVM/include/libcxx-ohos/include/c++/v1"
CXX=$OHOS_LLVM/bin/clang++
AOSP=$WLROOT/aosp-android-11                     # libnativehelper (jni.h, JniInvocation)
ADAPTER=$WLROOT/bridge-build/src/framework       # appspawn-x src + bionic_compat headers
LIBARTDIR=$WLROOT/art-universal-build/build-ohos-arm64/lib
BC=$ADAPTER/appspawn-x/bionic_compat/include
O=$WLROOT/bridge-build-arm64/out; mkdir -p $O
TMP=$WLROOT/bridge-build-arm64/appspawnx_build; rm -rf $TMP; mkdir -p $TMP
BOARD="${BOARD:-$(${HDC:-hdc} list targets 2>/dev/null | head -1)}"   # target device; override with BOARD=<serial>

INC="-I$ADAPTER/appspawn-x/src \
-I$OHSRC/base/startup/appspawn/interfaces/innerkits/include \
-I$OHSRC/base/startup/appspawn/standard/appspawn_msg/include \
-I$OHSRC/base/startup/init/interfaces/innerkits/include \
-I$OHSRC/base/startup/init/interfaces/innerkits/include/syspara \
-I$OHSRC/base/hiviewdfx/hilog/interfaces/native/innerkits/include \
-I$OHSRC/commonlibrary/c_utils/base/include \
-I$OHSRC/foundation/communication/ipc/interfaces/innerkits/ipc_core/include \
-I$OHSRC/foundation/systemabilitymgr/samgr/interfaces/innerkits/samgr_proxy/include \
-I$OHSRC/third_party/json/include \
-I$OHSRC/base/security/selinux_adapter/interfaces/policycoreutils/include \
-I$OHSRC/third_party/selinux/libselinux/include \
-I$OHSRC/base/security/access_token/interfaces/innerkits/token_setproc/include \
-I$OHSRC/base/security/access_token/interfaces/innerkits/accesstoken/include \
-I$BC \
-I$AOSP/libnativehelper/include_jni \
-I$AOSP/libnativehelper/include \
-I$AOSP/libnativehelper/include_platform_header_only \
-I$AOSP/libnativehelper/include_platform"

CFLAGS="--target=aarch64-linux-ohos --sysroot=$SYSROOT $LIBCXX \
-fPIC -O2 -std=c++17 -D__OHOS__ -include signal.h \
\
-Wno-unused-parameter -Wno-missing-field-initializers -Wno-error -Wno-c99-designator"

SRCS="main.cpp appspawnx_runtime.cpp spawn_server.cpp child_main.cpp"
echo "=== appspawn-x compile (ARM64 / OHOS 6.1) ==="
ok=0; fl=0
for s in $SRCS; do
    n=${s%.cpp}
    printf "  %-28s " "$n"
    if $CXX $CFLAGS $INC -c -o $TMP/$n.o $ADAPTER/appspawn-x/src/$s 2>$TMP/$n.err; then
        echo OK; ok=$((ok+1))
    else
        echo "FAIL — first errors:"; grep -E 'error:' $TMP/$n.err | head -4 | sed 's/^/      /'; fl=$((fl+1))
    fi
done
echo "  compiled $ok/$((ok+fl))"
[ $ok -eq 0 ] && { echo "no objects — fix 6.1 header drift first"; exit 1; }
[ $fl -gt 0 ] && { echo "COMPILE phase incomplete ($fl failed) — resolve before link"; exit 2; }

echo "=== COMPILE clean — LINK appspawn-x (arm64) ==="
NH=$WLROOT/art-latest/build-ohos-arm64/nativehelper
AB=$WLROOT/art-latest/build-ohos-arm64/android-base
BUILTINS=$(ls $OHOS_LLVM/lib/clang/*/lib/aarch64-linux-ohos/libclang_rt.builtins.a 2>/dev/null | head -1)
[ -n "$BUILTINS" ] || BUILTINS=$NDK/llvm/lib/clang/15.0.4/lib/aarch64-linux-ohos/libclang_rt.builtins.a
LIBDIR=$O/board_libs   # 7 OHOS .so's pulled from board /system/lib64 (recv via Windows path)
$CXX --target=aarch64-linux-ohos --sysroot=$SYSROOT -fuse-ld=lld \
  $TMP/*.o $NH/JniInvocation.o $NH/JNIHelp.o $NH/JniConstants.o $AB/liblog_symbols.o \
  -L$LIBDIR -lhilog -lipc_single.z -lsamgr_proxy.z -lbegetutil.z -lselinux.z -lhap_restorecon.z -ltokensetproc_shared.z \
  $LIBARTDIR/libart.so -lc -ldl -lpthread $BUILTINS \
  -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-in-shared-libs \
  -o $O/appspawn-x && echo "OK appspawn-x -> $O/appspawn-x ($(ls -la $O/appspawn-x|awk '{print $5}') bytes, aarch64 PIE)"
