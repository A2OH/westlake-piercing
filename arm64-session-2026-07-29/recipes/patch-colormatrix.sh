#!/bin/bash
# WESTLAKE §675 — replace the android.graphics.ColorMatrix STUB in adapter-mainline-stubs.jar
# with a real implementation.
#
# Why: the shipped stub is literally `<init>()V` and nothing else (DumpCls confirms), so
#   com.ss.android.image.AsyncImageView   <clinit> -> NoSuchMethodError set([F)V in ColorMatrix
#   com.ss.android.common.util.UiUtils    <clinit> -> same
# Both are left half-initialised by the tolerate-clinit path. AsyncImageView is the view class
# every Toutiao feed row uses: feed row layouts (NightModeAsyncImageView) get inflated while
# FeedCommonRecyclerView stays empty.
#
# Structure copied from patch-mainline-stubs.sh (§471), which replaced a class in this same jar.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
OUT="${OUT:-/tmp/colormatrix-patch}"; rm -rf "$OUT"; mkdir -p "$OUT/cls" "$OUT/dex" "$OUT/x"

recv "$ASX/fw/adapter-mainline-stubs.jar" "$OUT/orig.jar" || exit 1
cd "$OUT/x" && unzip -o -q ../orig.jar 'classes*.dex' && cd "$OUT"
test -f "$OUT/x/classes.dex" || { echo "FAIL: no classes.dex in the stubs jar"; exit 1; }

# ColorMatrix needs nothing from android.jar (java.lang + java.util only), so compile against the
# JDK. Compiling it against android.jar would put our source and the platform's own copy of the
# same type in one compilation.
javac -nowarn -source 8 -target 8 -d "$OUT/cls" \
  "$HERE/../java/android/graphics/ColorMatrix.java" 2>&1 | grep -v 'bootstrap class path' || true
test -f "$OUT/cls/android/graphics/ColorMatrix.class" || { echo "FAIL: javac produced no class"; exit 1; }

# ⚠️d8 silently produces no dex on some inputs — check before packing, or you ship a stale jar.
"$D8" --release --min-api 30 --lib "$ANDROID_JAR" --output "$OUT/dex" \
  "$OUT/cls/android/graphics/ColorMatrix.class"
test -f "$OUT/dex/classes.dex" || { echo "FAIL: d8 produced no dex"; exit 1; }

javac -nowarn -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/DexMerge.java"
java -cp "$DEXLIB_CP:$OUT" DexMerge "$OUT/x/classes.dex" "$OUT/dex/classes.dex" "$OUT/x/classes-patched.dex"

python3 - "$OUT/orig.jar" "$OUT/x/classes-patched.dex" "$OUT/stubs.jar" <<'PY'
import sys, zipfile
base, dex, out = sys.argv[1:4]
zin = zipfile.ZipFile(base); zout = zipfile.ZipFile(out, 'w')
if 'classes.dex' not in zin.namelist():
    sys.exit('FATAL: %s has no classes.dex' % base)
for i in zin.infolist():
    data = open(dex,'rb').read() if i.filename == 'classes.dex' else zin.read(i.filename)
    zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
    zi.compress_type = i.compress_type; zi.external_attr = i.external_attr
    zi.create_system = i.create_system
    zout.writestr(zi, data)
zout.close(); zin.close()
PY
# ⚠️ALWAYS zipalign after a python repack: it misaligns STORED, mmap'd entries.
"$ZIPALIGN" -f -p 4 "$OUT/stubs.jar" "$OUT/stubs-aligned.jar"

"$HDC" shell "[ -f $ASX/fw/adapter-mainline-stubs.jar.bak-pre675 ] || cp $ASX/fw/adapter-mainline-stubs.jar $ASX/fw/adapter-mainline-stubs.jar.bak-pre675"
push "$OUT/stubs-aligned.jar" "$ASX/fw/adapter-mainline-stubs.jar"
echo "adapter-mainline-stubs.jar patched (§675 ColorMatrix) — RESTART AND CONFIRM IT BOOTS"
