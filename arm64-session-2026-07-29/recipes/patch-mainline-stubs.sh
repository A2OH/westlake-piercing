#!/bin/bash
# WESTLAKE §471 — add android.media.MediaServiceManager (+ ServiceRegisterer) to
# adapter-mainline-stubs.jar and replace MediaFrameworkPlatformInitializer with one that actually has
# getMediaServiceManager().
#
# Why: MediaSessionManager.<init> calls, in bytecode,
#   MediaFrameworkPlatformInitializer.getMediaServiceManager()
#     .getMediaSessionServiceRegisterer().get()  ->  ISessionManager$Stub.asInterface(binder)
# The shipped stub had the setter but not the getter, and no MediaServiceManager class at all, so the
# first hop threw NoSuchMethodError and getSystemService(MEDIA_SESSION_SERVICE) stayed null.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
OUT="${OUT:-/tmp/mainline-patch}"; rm -rf "$OUT"; mkdir -p "$OUT/cls" "$OUT/dex" "$OUT/x" "$OUT/stub/android/os"

recv "$ASX/fw/adapter-mainline-stubs.jar" "$OUT/orig.jar" || exit 1
cd "$OUT/x" && unzip -o -q ../orig.jar 'classes*.dex' && cd "$OUT"
test -f "$OUT/x/classes.dex" || { echo "FAIL: no classes.dex in the stubs jar"; exit 1; }

# android.os.ServiceManager is @hide, so compile against a throwaway stub and dex ONLY our classes —
# otherwise the stub would shadow the real framework class at runtime.
cat > "$OUT/stub/android/os/ServiceManager.java" <<'J'
package android.os;
public final class ServiceManager {
    public static IBinder getService(String name) { return null; }
    public static void addService(String name, IBinder service) { }
}
J
javac -nowarn -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -d "$OUT/cls" \
  "$OUT/stub/android/os/ServiceManager.java" \
  "$HERE/../java/android/media/MediaServiceManager.java" \
  "$HERE/../java/android/media/MediaFrameworkPlatformInitializer.java"

# ⚠️d8 silently produces no dex on some inputs — check before packing, or you ship a stale jar.
"$D8" --release --min-api 30 --lib "$ANDROID_JAR" --output "$OUT/dex" \
  "$OUT/cls/android/media/MediaServiceManager.class" \
  "$OUT/cls/android/media/MediaServiceManager\$ServiceRegisterer.class" \
  "$OUT/cls/android/media/MediaFrameworkPlatformInitializer.class"
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
"$HDC" shell "[ -f $ASX/fw/adapter-mainline-stubs.jar.bak-pre471 ] || cp $ASX/fw/adapter-mainline-stubs.jar $ASX/fw/adapter-mainline-stubs.jar.bak-pre471"
push "$OUT/stubs-aligned.jar" "$ASX/fw/adapter-mainline-stubs.jar"
echo "adapter-mainline-stubs.jar patched — RESTART AND CONFIRM IT BOOTS"
