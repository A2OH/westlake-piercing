#!/bin/bash
# build_appspawnx_arm64.sh — build appspawn-x for aarch64 against OHOS 6.1.0.31.
# Mirrors the arm32 compile_appspawnx.sh but: OHOS 6.1 SDK toolchain+sysroot,
# aarch64 target, 6.1 inner-API headers from the sparse-synced source tree, AOSP
# libnativehelper headers, my arm64 libart. COMPILE phase validates 6.1 header
# compatibility (surfaces API drift = the porting work); LINK pulls board .so's.
set -o pipefail
OHSRC=/home/dspfac/ohos-6.1-src                       # sparse-synced 6.1 inner-API headers
NDK=/home/dspfac/ohos-sdk-6.1/linux/native            # 6.1 SDK: clang15 + arm64 sysroot
SYSROOT=$NDK/sysroot
CXX=$NDK/llvm/bin/clang++
AOSP=/home/dspfac/aosp-android-11                     # libnativehelper (jni.h, JniInvocation)
ADAPTER=/home/dspfac/bridge-build/src/framework       # appspawn-x src + bionic_compat headers
LIBARTDIR=/home/dspfac/art-universal-build/build-ohos-arm64/lib
BC=$ADAPTER/appspawn-x/bionic_compat/include
O=/home/dspfac/bridge-build-arm64/out; mkdir -p $O
TMP=/home/dspfac/bridge-build-arm64/appspawnx_build; rm -rf $TMP; mkdir -p $TMP
BOARD=5cdbf6af00000000000000000923012c

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

CFLAGS="--target=aarch64-linux-ohos --sysroot=$SYSROOT \
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
NH=/home/dspfac/art-latest/build-ohos-arm64/nativehelper
AB=/home/dspfac/art-latest/build-ohos-arm64/android-base
BUILTINS=$NDK/llvm/lib/clang/15.0.4/lib/aarch64-linux-ohos/libclang_rt.builtins.a
LIBDIR=$O/board_libs   # 7 OHOS .so's pulled from board /system/lib64 (recv via Windows path)
$CXX --target=aarch64-linux-ohos --sysroot=$SYSROOT -fuse-ld=lld \
  $TMP/*.o $NH/JniInvocation.o $NH/JNIHelp.o $NH/JniConstants.o $AB/liblog_symbols.o \
  -L$LIBDIR -lhilog -lipc_single.z -lsamgr_proxy.z -lbegetutil.z -lselinux.z -lhap_restorecon.z -ltokensetproc_shared.z \
  $LIBARTDIR/libart.so -lc++_shared -lc -ldl -lpthread $BUILTINS \
  `# 2026-07-21: -lc++_shared. spawn_server.cpp pulls OHOS platform headers (ipc/samgr/`\
  `# c_utils) which use inline namespace std::__n1, so it needs the PLATFORM libc++ —`\
  `# /system/lib64/libc++_shared.so (__n1). The NDK libc++.so and prebuilt libc++.a are`\
  `# both __h and do NOT provide those std::string symbols (append/assign/operator=/…).`\
  -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-in-shared-libs \
  -o $O/appspawn-x && echo "OK appspawn-x -> $O/appspawn-x ($(ls -la $O/appspawn-x|awk '{print $5}') bytes, aarch64 PIE)"
