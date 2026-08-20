#!/bin/bash
# targeted android-15.0.0_r1 AOSP source sync for the arm64 support-lib build.
# Small repos: --depth=1 full. Big repos (frameworks/base/native): blobless + sparse.
cd $WLROOT/aosp-15
TAG=android-15.0.0_r1; BASE=https://android.googlesource.com/platform
clone_full(){ local p=$1 d=$2; echo "[SYNC] $p (full depth1)..."; rm -rf "$d"; git clone --depth=1 -b $TAG "$BASE/$p" "$d" 2>&1 | tail -1; }
clone_sparse(){ local p=$1 d=$2; shift 2; echo "[SYNC] $p (blobless sparse: $*)..."; rm -rf "$d";
  git clone --depth=1 --filter=blob:none --sparse -b $TAG "$BASE/$p" "$d" 2>&1 | tail -1;
  (cd "$d" && git sparse-checkout set "$@" 2>&1 | tail -1); }
clone_full system/libbase       system/libbase
clone_full system/logging       system/logging
clone_full system/core          system/core
clone_full external/fmtlib      external/fmtlib
clone_full external/zlib        external/zlib
clone_full system/libziparchive system/libziparchive
clone_sparse frameworks/base    frameworks/base    libs/androidfw core/jni/include
clone_sparse frameworks/native  frameworks/native  libs/binder include
echo "[SYNC] sizes:"; du -sh system/libbase system/core frameworks/base frameworks/native 2>/dev/null
echo "[SYNC] key sources present:"
for f in system/libbase/strings.cpp system/core/libutils/String8.cpp frameworks/base/libs/androidfw/AssetsProvider.cpp frameworks/native/libs/binder/Binder.cpp; do [ -f "$f" ] && echo "  OK $f" || echo "  MISS $f"; done
echo "AOSP15_SYNC_DONE"
