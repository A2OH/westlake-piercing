#!/bin/bash
# build_bridge_arm64.sh — compile+link liboh_adapter_bridge.so for arm64/OHOS 6.1.
# ★CRITICAL: use the OHOS prebuilts clang + OHOS libc++ (libcxx-ohos, __h ABI namespace)
# — NOT the SDK/NDK clang (__n1). The board's OHOS libs use __h; a __n1 bridge cannot
# resolve any std::string-passing OHOS API (mangled-name mismatch). Same toolchain as libart.
OHOS_LLVM=/home/dspfac/openharmony/prebuilts/clang/ohos/linux-x86_64/llvm
OCXX=$OHOS_LLVM/bin/clang++
LIBCXX="-nostdinc++ -isystem $OHOS_LLVM/include/libcxx-ohos/include/c++/v1"
NDK=/home/dspfac/ohos-sdk-6.1/linux/native   # sysroot (musl libc) only
OVL=/home/dspfac/bridge-build-arm64/overlay; STUB=/home/dspfac/bridge-build-arm64/stubinc
O=/home/dspfac/bridge-build-arm64/bridge_build; OUT=/home/dspfac/bridge-build-arm64/out; LIBDIR=$OUT/board_libs; mkdir -p $O $OUT
INCS="-I$OVL/inc -I$STUB $(cat /home/dspfac/bridge-build-arm64/bridge_incs_all.txt)"
SQLINCS="-I/home/dspfac/bridge-build/src/framework/sqlite/inc -I/home/dspfac/third_party/sqlite/include -I/home/dspfac/aosp-15/frameworks/base/libs/androidfw/include -I/home/dspfac/aosp-15/system/core/libcutils/include -I/home/dspfac/bridge-build/src/framework/sqlite"
CFBASE="--target=aarch64-linux-ohos --sysroot=$NDK/sysroot $LIBCXX -fPIC -O2 -std=c++17 -D__OHOS__ -include signal.h -include sys/stat.h -include log/log.h -include $STUB/pkcs7_stub.h -Wno-unused-parameter -Wno-error -Wno-c99-designator -Wno-narrowing -Wno-c++11-narrowing"
ok=0; fl=0; OBJS=""
for src in $(find /home/dspfac/bridge-build/src -name '*.cpp' | grep -v appspawn-x); do
  n=$(basename $src .cpp); [ -f $OVL/cpp/$n.cpp ] && src=$OVL/cpp/$n.cpp
  CF="$CFBASE"; case $n in oh_window_manager_client|session_stage_adapter|oh_surface_bridge|rs_surface_helper) CF="$CFBASE -include $STUB/skopts_hash_compat.h -I/home/dspfac/ohos-6.1-src/foundation/window/window_manager/window_scene/common/include -I/home/dspfac/ohos-6.1-src/foundation/window/window_manager/interfaces/innerkits/wm -I/home/dspfac/ohos-6.1-src/foundation/window/window_manager/interfaces/innerkits/dm -I/home/dspfac/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/composer -I/home/dspfac/ohos-6.1-src/foundation/graphic/graphic_2d/rosen/modules/composer/vsync/include -I/home/dspfac/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/common -I/home/dspfac/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/surface -I/home/dspfac/ohos-6.1-src/foundation/graphic/graphic_2d/interfaces/inner_api/hyper_graphic_manager";;
    android_database_SQLiteConnection|android_database_SQLiteCommon|wl_CursorWindow|wl_sqlite3_android|android_database_CursorWindow) CF="$CFBASE $SQLINCS";;
    oh_bundle_mgr_client) CF="$CFBASE -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/extend_resource -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/bundlemgr_ext -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/app_control -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/bundle_resource -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/default_app -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/overlay -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/quick_fix -I/home/dspfac/ohos-6.1-src/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core/include/verify -I/home/dspfac/openharmony/out/rk3568/gen/foundation/bundlemanager/bundle_framework/interfaces/inner_api/appexecfwk_core";;
    android_view_MotionEvent_aosp|android_input_Input_aosp) CF="$CFBASE -I/home/dspfac/bridge-build/src/framework/android-runtime/include";;
    wl_window_stub) CF="$CFBASE -include $STUB/skopts_hash_compat.h -include occupied_area_change_info.h -include key_event.h";; esac
  if $OCXX $CF $INCS -c -o $O/$n.o "$src" 2>$O/$n.err; then ok=$((ok+1)); OBJS="$OBJS $O/$n.o"; else fl=$((fl+1)); echo "FAIL $n"; fi
done
echo "compiled $ok/$((ok+fl)) (OHOS toolchain, __h ABI)"
# WESTLAKE: sqlite3 amalgamation compiled as C (cannot compile as C++ - void* conversions)
if [ ! -f $O/wl_sqlite3.o ]; then
  $OHOS_LLVM/bin/clang --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -fPIC -O2 -std=c99 \
    -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_FILE_PERMISSIONS=0600 \
    -c -o $O/wl_sqlite3.o /home/dspfac/third_party/sqlite/src/sqlite3.c 2>$O/wl_sqlite3.err \
    && echo "  sqlite3 amalgamation OK" || echo "  FAIL wl_sqlite3 (see $O/wl_sqlite3.err)"
fi
[ -f $O/wl_sqlite3.o ] && OBJS="$OBJS $O/wl_sqlite3.o"
LFLAGS=""; for l in $(ls $LIBDIR/*.so 2>/dev/null | xargs -n1 basename | sed 's/^lib//;s/\.so$//'); do LFLAGS="$LFLAGS -l$l"; done
$OCXX --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -shared -fPIC -fuse-ld=lld \
  -Wl,-soname,liboh_adapter_bridge.so -Wl,--allow-multiple-definition -Wl,--unresolved-symbols=ignore-all \
  -Wl,--no-as-needed $OBJS -L$LIBDIR $LFLAGS -Wl,--as-needed \
  -o $OUT/liboh_adapter_bridge.so && echo "LINKED (NEEDED=$($NDK/llvm/bin/llvm-readelf -d $OUT/liboh_adapter_bridge.so|grep -c NEEDED)) -> $OUT/liboh_adapter_bridge.so"
