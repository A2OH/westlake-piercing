#!/bin/bash
# build_bridge_arm64.sh — compile+link liboh_adapter_bridge.so for arm64/OHOS 6.1.
# AOSP=/home/dspfac/bridge-build/aosp (the adapter's OWN newer AOSP — NOT aosp-11).
NDK=/home/dspfac/ohos-sdk-6.1/linux/native; CXX=$NDK/llvm/bin/clang++
OVL=/home/dspfac/bridge-build-arm64/overlay; STUB=/home/dspfac/bridge-build-arm64/stubinc
O=/home/dspfac/bridge-build-arm64/bridge_build; OUT=/home/dspfac/bridge-build-arm64/out; mkdir -p $O $OUT
INCS="-I$OVL/inc -I$STUB $(cat /home/dspfac/bridge-build-arm64/bridge_incs_all.txt)"
CFBASE="--target=aarch64-linux-ohos --sysroot=$NDK/sysroot -fPIC -O2 -std=c++17 -D__OHOS__ -include signal.h -include sys/stat.h -include log/log.h -include $STUB/pkcs7_stub.h -Wno-unused-parameter -Wno-error -Wno-c99-designator -Wno-narrowing -Wno-c++11-narrowing"
ok=0; fl=0; OBJS=""
for src in $(find /home/dspfac/bridge-build/src -name '*.cpp' | grep -v appspawn-x); do
  n=$(basename $src .cpp); [ -f $OVL/cpp/$n.cpp ] && src=$OVL/cpp/$n.cpp
  CF="$CFBASE"; case $n in oh_window_manager_client|session_stage_adapter|oh_surface_bridge|rs_surface_helper) CF="$CFBASE -include $STUB/skopts_hash_compat.h";; esac
  if $CXX $CF $INCS -c -o $O/$n.o "$src" 2>$O/$n.err; then ok=$((ok+1)); OBJS="$OBJS $O/$n.o"; else fl=$((fl+1)); echo "FAIL $n"; fi
done
echo "compiled $ok/$((ok+fl))"
$CXX --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -shared -fPIC -fuse-ld=lld \
  -Wl,-soname,liboh_adapter_bridge.so -Wl,--unresolved-symbols=ignore-all -Wl,--allow-multiple-definition \
  $OBJS -L$OUT/board_libs -o $OUT/liboh_adapter_bridge.so && echo "LINKED -> $OUT/liboh_adapter_bridge.so"
