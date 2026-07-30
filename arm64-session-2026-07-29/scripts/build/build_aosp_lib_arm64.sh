#!/bin/bash
# build_aosp_lib_arm64.sh — AOSP native support libs for arm64/OHOS 6.1 (android-15 sources).
OH=/home/dspfac/openharmony; A=/home/dspfac/aosp-15; NDK=/home/dspfac/ohos-sdk-6.1/linux/native; SR=$NDK/sysroot
CXX=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang++; CC=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang
O=/home/dspfac/bridge-build-arm64/aosp_lib; mkdir -p $O
BUILTINS=$(find $OH/prebuilts/clang/ohos -path '*aarch64-linux-ohos*builtins.a' 2>/dev/null | head -1)
PSDK=/home/dspfac/bridge-build-arm64/out/board_libs   # board libs (hilog, z) to link against
WARN="-Wno-unused-parameter -Wno-format -Wno-sign-compare -Wno-missing-field-initializers -Wno-c99-designator -Wno-gnu-designator -Wno-extern-c-compat -Wno-deprecated-declarations -Wno-c++11-narrowing -Wno-error -Wno-implicit-function-declaration"
COMMON="--target=aarch64-linux-ohos --sysroot=$SR -fPIC -O2 -DNDEBUG -D__OHOS__ -D_GNU_SOURCE -include /home/dspfac/bridge-build-arm64/musl_compat.h $WARN"
CXXF="$CXX $COMMON -std=gnu++20"; CF="$CC $COMMON -std=gnu11 -D__ANDROID_API__=34"
LNK="$CXX --target=aarch64-linux-ohos -B/home/dspfac/ohos-sdk-6.1/linux/native/sysroot/usr/lib/aarch64-linux-ohos -L/home/dspfac/ohos-sdk-6.1/linux/native/sysroot/usr/lib/aarch64-linux-ohos -L$O -L$PSDK -shared -fPIC"
bld() {
  local N=$1 I=$2; shift 2; local D=/tmp/cc64/$N; mkdir -p $D; local ok=0 fl=0 fails=""
  for s in "$@"; do local b=$(basename $s); b=${b%.*}; local ext=${s##*.}; local comp=$CXXF; [ "$ext" = "c" ] && comp=$CF
    if $comp $I -c -o $D/$b.o $s 2>/tmp/cc64/${N}_${b}.err; then ok=$((ok+1)); else fl=$((fl+1)); fails="$fails $b"; fi; done
  echo "  lib$N: compiled $ok/$((ok+fl))${fails:+ FAIL:$fails}"
  [ $fl -gt 0 ] && { for f in $fails; do echo "    $f: $(grep -m1 'error:' /tmp/cc64/${N}_${f}.err|grep -oE 'error:.*'|cut -c1-64)"; done | head -3; return 1; }
  if $LNK -o $O/lib$N.so $D/*.o -lc $EXTRA_LIBS -ldl -lpthread $BUILTINS 2>/tmp/cc64/${N}_link.err; then
    echo "  ✅ lib$N.so ($(ls -la $O/lib$N.so|awk '{print $5}') B)"
  else echo "  ❌ lib$N.so LINK: $(grep -m2 -iE 'undefined|error' /tmp/cc64/${N}_link.err|grep -oE '(undefined symbol|error):.*'|cut -c1-60|tr '\n' ';')"; return 1; fi
}
LB="-I$A/system/libbase/include -I$A/system/logging/liblog/include -I$A/system/core/include -I$A/system/core/libcutils/include -I$A/external/fmtlib/include"
# 1. log
EXTRA_LIBS="-lhilog" bld log "-I$A/system/logging/liblog/include -I$A/system/libbase/include -I$A/system/core/include -I$A/system/core/libcutils/include -DLIBLOG_LOG_TAG=1006 -DSNET_EVENT_LOG_TAG=1397638484 -D__ANDROID_API__=34" \
  $A/system/logging/liblog/logger_name.cpp $A/system/logging/liblog/logger_read.cpp $A/system/logging/liblog/logger_write.cpp $A/system/logging/liblog/logprint.cpp $A/system/logging/liblog/properties.cpp $A/system/logging/liblog/log_time.cpp $A/system/logging/liblog/event_tag_map.cpp $A/system/logging/liblog/log_event_write.cpp $A/system/logging/liblog/log_event_list.cpp
# 2. base
EXTRA_LIBS="-llog" bld base "$LB -DANDROID_BASE_UNIQUE_FD_DISABLE_FDSAN" \
  $A/system/libbase/chrono_utils.cpp $A/system/libbase/file.cpp $A/system/libbase/hex.cpp $A/system/libbase/logging.cpp $A/system/libbase/mapped_file.cpp $A/system/libbase/parsebool.cpp $A/system/libbase/parsenetaddress.cpp $A/system/libbase/posix_strerror_r.cpp $A/system/libbase/process.cpp $A/system/libbase/properties.cpp $A/system/libbase/stringprintf.cpp $A/system/libbase/strings.cpp $A/system/libbase/threads.cpp $A/system/libbase/errors_unix.cpp $A/system/libbase/cmsg.cpp /home/dspfac/bridge-build-arm64/musl_compat.c
# 3. utils (android-15 split: libutils/ + libutils/binder/)
#    Looper.cpp added 2026-07-21: libhwui.so NEEDs android::Looper::Looper(bool)
#    (RenderThread) and failed to relocate without it.
EXTRA_LIBS="-lbase -llog -lcutils" bld utils "-I$A/system/core/libutils/include -I$A/system/core/libutils/binder/include -I$A/system/core/libcutils/include -I$A/system/libbase/include -I$A/system/logging/liblog/include -I$A/system/core/include -I$A/external/fmtlib/include" \
  $A/system/core/libutils/FileMap.cpp $A/system/core/libutils/JenkinsHash.cpp $A/system/core/libutils/LightRefBase.cpp $A/system/core/libutils/NativeHandle.cpp $A/system/core/libutils/Printer.cpp $A/system/core/libutils/StopWatch.cpp $A/system/core/libutils/SystemClock.cpp $A/system/core/libutils/Threads.cpp $A/system/core/libutils/Timers.cpp $A/system/core/libutils/Tokenizer.cpp $A/system/core/libutils/misc.cpp $A/system/core/libutils/Looper.cpp $A/system/core/libutils/binder/Errors.cpp $A/system/core/libutils/binder/RefBase.cpp $A/system/core/libutils/binder/SharedBuffer.cpp $A/system/core/libutils/binder/String16.cpp $A/system/core/libutils/binder/String8.cpp $A/system/core/libutils/binder/StrongPointer.cpp $A/system/core/libutils/binder/Unicode.cpp $A/system/core/libutils/binder/VectorImpl.cpp

# 4. cutils
EXTRA_LIBS="-lbase -llog" bld cutils "-I$A/system/core/libcutils/include -I$A/system/logging/liblog/include -I$A/system/libbase/include -I$A/system/core/include" \
  $A/system/core/libcutils/config_utils.cpp $A/system/core/libcutils/hashmap.cpp $A/system/core/libcutils/iosched_policy.cpp $A/system/core/libcutils/load_file.cpp $A/system/core/libcutils/native_handle.cpp $A/system/core/libcutils/properties.cpp $A/system/core/libcutils/record_stream.cpp $A/system/core/libcutils/str_parms.cpp $A/system/core/libcutils/fs_config.cpp $A/system/core/libcutils/canned_fs_config.cpp $A/system/core/libcutils/trace-host.cpp
# 5. ziparchive
EXTRA_LIBS="-lbase -llog -lz" bld ziparchive "-DZLIB_CONST -I$A/system/libziparchive/include -I$A/system/libziparchive/incfs_support/include -I/home/dspfac/bridge-build-arm64/stubinc -I$A/system/libbase/include -I$A/system/logging/liblog/include -I$A/system/core/include -I$A/external/zlib" \
  $A/system/libziparchive/zip_archive.cc $A/system/libziparchive/zip_archive_stream_entry.cc $A/system/libziparchive/zip_cd_entry_map.cc $A/system/libziparchive/zip_error.cpp $A/system/libziparchive/zip_writer.cc
# 6. androidfw (the bridge blocker: EmptyAssetsProvider/ApkAssets/AssetManager2/ResXML)
FWI="-I$A/frameworks/base/libs/androidfw/include -I$A/frameworks/base/libs/androidfw/include_pathutils -I$A/frameworks/native/include -I$A/system/incremental_delivery/incfs/util/include -I/home/dspfac/bridge-build-arm64/stubinc -I$A/system/core/libutils/include -I$A/system/core/libutils/binder/include -I$A/system/core/libcutils/include -I$A/system/libbase/include -I$A/system/logging/liblog/include -I$A/system/core/include -I$A/system/libziparchive/include -I$A/external/zlib -I$A/external/fmtlib/include -I$A/frameworks/base/core/jni/include"
EXTRA_LIBS="-lbase -llog -lutils -lcutils -lziparchive -lz" bld androidfw "$FWI" \
  $A/frameworks/base/libs/androidfw/ApkAssets.cpp $A/frameworks/base/libs/androidfw/ApkParsing.cpp $A/frameworks/base/libs/androidfw/Asset.cpp $A/frameworks/base/libs/androidfw/AssetDir.cpp $A/frameworks/base/libs/androidfw/AssetManager.cpp $A/frameworks/base/libs/androidfw/AssetManager2.cpp $A/frameworks/base/libs/androidfw/AssetsProvider.cpp $A/frameworks/base/libs/androidfw/AttributeResolution.cpp $A/frameworks/base/libs/androidfw/ChunkIterator.cpp $A/frameworks/base/libs/androidfw/ConfigDescription.cpp $A/frameworks/base/libs/androidfw/FileStream.cpp $A/frameworks/base/libs/androidfw/Idmap.cpp $A/frameworks/base/libs/androidfw/LoadedArsc.cpp $A/frameworks/base/libs/androidfw/Locale.cpp $A/frameworks/base/libs/androidfw/LocaleData.cpp $A/frameworks/base/libs/androidfw/misc.cpp $A/frameworks/base/libs/androidfw/PathUtils.cpp $A/frameworks/base/libs/androidfw/PosixUtils.cpp $A/frameworks/base/libs/androidfw/ResourceTimer.cpp $A/frameworks/base/libs/androidfw/ResourceTypes.cpp $A/frameworks/base/libs/androidfw/ResourceUtils.cpp $A/frameworks/base/libs/androidfw/StreamingZipInflater.cpp $A/frameworks/base/libs/androidfw/StringPool.cpp $A/frameworks/base/libs/androidfw/TypeWrappers.cpp $A/frameworks/base/libs/androidfw/Util.cpp $A/frameworks/base/libs/androidfw/ZipFileRO.cpp /home/dspfac/aosp-15/frameworks/base/libs/androidfw/ZipUtils.cpp /home/dspfac/aosp-15/frameworks/base/libs/androidfw/BigBuffer.cpp /home/dspfac/aosp-15/frameworks/base/libs/androidfw/BigBufferStream.cpp /home/dspfac/aosp-15/system/incremental_delivery/incfs/util/map_ptr.cpp
echo "--- full chain done; androidfw+binder in next pass ---"
