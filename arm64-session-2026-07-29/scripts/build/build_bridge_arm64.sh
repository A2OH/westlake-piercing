#!/bin/bash
# build_bridge_arm64.sh — compile+link liboh_adapter_bridge.so for arm64/OHOS 6.1.
# ★CRITICAL: use the OHOS prebuilts clang + OHOS libc++ (libcxx-ohos, __h ABI namespace)
# — NOT the SDK/NDK clang (__n1). The board's OHOS libs use __h; a __n1 bridge cannot
# resolve any std::string-passing OHOS API (mangled-name mismatch). Same toolchain as libart.
#
# Paths come from recipes/env.sh ($WLROOT = the parent dir holding every source tree).
# BRIDGE_SRC defaults to this repo's committed bridge-full-src/, so the build no longer depends on an
# untracked local directory. Point it at a live working tree if you are editing sources in place.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; KIT="$(cd "$HERE/../.." && pwd)"
. "$KIT/recipes/env.sh"
BRIDGE_SRC="${BRIDGE_SRC:-$KIT/bridge-full-src}"
BUILD_INPUTS="${BUILD_INPUTS:-$KIT/bridge-build-inputs}"
for d in "$BRIDGE_SRC" "$BUILD_INPUTS"; do
  [ -d "$d" ] || { echo "FATAL: missing $d"; exit 1; }
done
OHOS_LLVM=$WLROOT/openharmony/prebuilts/clang/ohos/linux-x86_64/llvm
OCXX=$OHOS_LLVM/bin/clang++
[ -x "$OCXX" ] || { echo "FATAL: OHOS clang++ not at $OCXX — see REPRODUCE.md section 0"; exit 1; }
LIBCXX="-nostdinc++ -isystem $OHOS_LLVM/include/libcxx-ohos/include/c++/v1"
NDK=$WLROOT/ohos-sdk-6.1/linux/native   # sysroot (musl libc) only
OVL=$BUILD_INPUTS/overlay; STUB=$BUILD_INPUTS/stubinc
O=$WLROOT/bridge-build-arm64/bridge_build; OUT=$WLROOT/bridge-build-arm64/out; LIBDIR=$OUT/board_libs; mkdir -p $O $OUT
# bridge_incs_all.txt stores @ROOT@ rather than a literal path: it is consumed through $(cat ...),
# and command-substitution output is NOT re-expanded, so a literal $WLROOT would reach clang unexpanded.
INCS="-I$OVL/inc -I$STUB $(sed -e "s|@BRIDGE_SRC@|$BRIDGE_SRC|g" -e "s|@ROOT@|$WLROOT|g" "$BUILD_INPUTS/bridge_incs_all.txt")"
SQLINCS="-I$BRIDGE_SRC/framework/sqlite/inc -I$WLROOT/third_party/sqlite/include -I$WLROOT/aosp-15/frameworks/base/libs/androidfw/include -I$WLROOT/aosp-15/system/core/libcutils/include -I$BRIDGE_SRC/framework/sqlite"
CFBASE="--target=aarch64-linux-ohos --sysroot=$NDK/sysroot $LIBCXX -fPIC -O2 -std=c++17 -D__OHOS__ -include signal.h -include sys/stat.h -include log/log.h -include $STUB/pkcs7_stub.h -Wno-unused-parameter -Wno-error -Wno-c99-designator -Wno-narrowing -Wno-c++11-narrowing"
ok=0; fl=0; OBJS=""
for src in $(find $BRIDGE_SRC -name '*.cpp' | grep -v appspawn-x); do
  n=$(basename $src .cpp); [ -f $OVL/cpp/$n.cpp ] && src=$OVL/cpp/$n.cpp
  CF="$CFBASE"; case $n in oh_window_manager_client|session_stage_adapter|oh_surface_bridge|rs_surface_helper) CF="$CFBASE -include $STUB/skopts_hash_compat.h -I$WLROOT/ohos-6.1-src/foundation/window/window_manager/window_scene/common/include -I$WLROOT/ohos-6.1-src/foundation/window/window_manager/interfaces/innerkits/wm -I$WLROOT/ohos-6.1-src/foundation/window/window_manager/interfaces/innerkits/dm -I$WLROOT/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/composer -I$WLROOT/ohos-6.1-src/foundation/graphic/graphic_2d/rosen/modules/composer/vsync/include -I$WLROOT/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/common -I$WLROOT/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/surface -I$WLROOT/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/hyper_graphic_manager";;
    android_database_SQLiteConnection|android_database_SQLiteCommon|wl_CursorWindow|wl_sqlite3_android|android_database_CursorWindow) CF="$CFBASE $SQLINCS";;
    oh_bundle_mgr_client) CF="$CFBASE -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/extend_resource -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/bundlemgr_ext -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/app_control -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/bundle_resource -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/default_app -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/overlay -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/quick_fix -I$WLROOT/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/verify -I$WLROOT/openharmony/out/rk3568/gen/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core";;
    android_view_MotionEvent_aosp|android_input_Input_aosp) CF="$CFBASE -I$BRIDGE_SRC/framework/android-runtime/include";;
    wl_window_stub) CF="$CFBASE -include $STUB/skopts_hash_compat.h -include occupied_area_change_info.h -include key_event.h";; esac
  if $OCXX $CF $INCS -c -o $O/$n.o "$src" 2>$O/$n.err; then ok=$((ok+1)); OBJS="$OBJS $O/$n.o"; else fl=$((fl+1)); echo "FAIL $n"; FAILED="$FAILED $n"; fi
done
echo "compiled $ok/$((ok+fl)) (OHOS toolchain, __h ABI)"

# ★This script cannot `set -e`: two files are EXPECTED to fail and the link must still happen.
# So assert on the failure SET, not the count -- a different file failing keeps the count identical
# while silently dropping your change from the .so. EXPECTED_FAILURES are unused sources that have
# never compiled on arm64; if you legitimately fix or add one, update this list in the same commit.
EXPECTED_FAILURES="apk_bundle_parser oh_skia_ahb_shim"
got=$(echo $FAILED | tr " " "\n" | sort | tr -d "\r" | paste -sd,)
want=$(echo $EXPECTED_FAILURES | tr " " "\n" | sort | paste -sd,)
if [ "$got" != "$want" ]; then
  echo "*** COMPILE FAILURE SET CHANGED ***"
  echo "    expected: $want"
  echo "    actual:   ${got:-<none>}"
  echo "    inspect $O/<name>.err. The link below will still run, but the .so is NOT trustworthy."
  BRIDGE_BUILD_SUSPECT=1
fi
# WESTLAKE: sqlite3 amalgamation compiled as C (cannot compile as C++ - void* conversions)
if [ ! -f $O/wl_sqlite3.o ]; then
  $OHOS_LLVM/bin/clang --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -fPIC -O2 -std=c99 \
    -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_FILE_PERMISSIONS=0600 \
    -c -o $O/wl_sqlite3.o $WLROOT/third_party/sqlite/src/sqlite3.c 2>$O/wl_sqlite3.err \
    && echo "  sqlite3 amalgamation OK" || echo "  FAIL wl_sqlite3 (see $O/wl_sqlite3.err)"
fi
[ -f $O/wl_sqlite3.o ] && OBJS="$OBJS $O/wl_sqlite3.o"
LFLAGS=""; for l in $(ls $LIBDIR/*.so 2>/dev/null | xargs -n1 basename | sed 's/^lib//;s/\.so$//'); do LFLAGS="$LFLAGS -l$l"; done
$OCXX --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -shared -fPIC -fuse-ld=lld \
  -Wl,-soname,liboh_adapter_bridge.so -Wl,--allow-multiple-definition -Wl,--unresolved-symbols=ignore-all \
  -Wl,--no-as-needed $OBJS -L$LIBDIR $LFLAGS -Wl,--as-needed \
  -o $OUT/liboh_adapter_bridge.so && echo "LINKED (NEEDED=$($NDK/llvm/bin/llvm-readelf -d $OUT/liboh_adapter_bridge.so|grep -c NEEDED)) -> $OUT/liboh_adapter_bridge.so"
[ -n "$BRIDGE_BUILD_SUSPECT" ] && { echo "REFUSING to report success: the compile failure set changed (see above)"; exit 1; }
exit 0
