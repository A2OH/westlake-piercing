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
CFBASE="--target=aarch64-linux-ohos --sysroot=$NDK/sysroot $LIBCXX -fPIC -O2 -std=c++17 -D__OHOS__ -include signal.h -include sys/stat.h -include log/log.h -include $STUB/pkcs7_stub.h -Wno-unused-parameter -Wno-error -Wno-c99-designator -Wno-narrowing -Wno-c++11-narrowing"
ok=0; fl=0; OBJS=""
for src in $(find /home/dspfac/bridge-build/src -name '*.cpp' | grep -v appspawn-x); do
  n=$(basename $src .cpp); [ -f $OVL/cpp/$n.cpp ] && src=$OVL/cpp/$n.cpp
  CF="$CFBASE"; case $n in oh_window_manager_client|session_stage_adapter|oh_surface_bridge|rs_surface_helper) CF="$CFBASE -include $STUB/skopts_hash_compat.h";; esac
  if $OCXX $CF $INCS -c -o $O/$n.o "$src" 2>$O/$n.err; then ok=$((ok+1)); OBJS="$OBJS $O/$n.o"; else fl=$((fl+1)); echo "FAIL $n"; fi
done
echo "compiled $ok/$((ok+fl)) (OHOS toolchain, __h ABI)"
LFLAGS=""; for l in $(ls $LIBDIR/*.so 2>/dev/null | xargs -n1 basename | sed 's/^lib//;s/\.so$//'); do LFLAGS="$LFLAGS -l$l"; done
$OCXX --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -shared -fPIC -fuse-ld=lld \
  -Wl,-soname,liboh_adapter_bridge.so -Wl,--allow-multiple-definition -Wl,--unresolved-symbols=ignore-all \
  -Wl,--no-as-needed $OBJS -L$LIBDIR $LFLAGS -Wl,--as-needed \
  -o $OUT/liboh_adapter_bridge.so && echo "LINKED (NEEDED=$($NDK/llvm/bin/llvm-readelf -d $OUT/liboh_adapter_bridge.so|grep -c NEEDED)) -> $OUT/liboh_adapter_bridge.so"
