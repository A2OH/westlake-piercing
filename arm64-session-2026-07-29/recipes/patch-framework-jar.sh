#!/bin/bash
# framework.jar dex surgery: 6 invoke-interface/range sites on IActivityManager -> invoke-static/range
# into a merged android/app/WlAmsBridge. Needed for registerReceiver + PendingIntent under the
# Proxy-stub AMS. Boots fine; verify before trusting.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
OUT="${OUT:-/tmp/fw-patch}"; rm -rf "$OUT"; mkdir -p "$OUT/cls" "$OUT/dex" "$OUT/x"
"$HDC" file recv "$ASX/fw/framework.jar" "$(wslpath -w "$WIN_STAGE/framework-orig.jar")" >/dev/null
cp "$WIN_STAGE/framework-orig.jar" "$OUT/orig.jar"
cd "$OUT/x" && unzip -o -q ../orig.jar 'classes*.dex' && cd "$OUT"

# The helper references @hide types, so compile against throwaway stub interfaces and then dex ONLY
# the helper class — otherwise the stubs would shadow the real framework classes at runtime.
mkdir -p "$OUT/stub/android/app" "$OUT/stub/android/content"
cat > "$OUT/stub/android/app/IActivityManager.java" <<'J'
package android.app; public interface IActivityManager extends android.os.IInterface { }
J
cat > "$OUT/stub/android/app/IApplicationThread.java" <<'J'
package android.app; public interface IApplicationThread extends android.os.IInterface { }
J
cat > "$OUT/stub/android/content/IIntentReceiver.java" <<'J'
package android.content; public interface IIntentReceiver extends android.os.IInterface { }
J
cat > "$OUT/stub/android/content/IIntentSender.java" <<'J'
package android.content; public interface IIntentSender extends android.os.IInterface { }
J
javac -nowarn -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -d "$OUT/cls" \
  $(find "$OUT/stub" -name '*.java') "$HERE/../java/android/app/WlAmsBridge.java"
$D8 --release --min-api 30 --lib "$ANDROID_JAR" --output "$OUT/dex" "$OUT/cls/android/app/WlAmsBridge.class"

javac -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/FwRewrite.java"
# both target methods live in classes.dex; the other 4 dex files are untouched
java -Xmx4g -cp "$DEXLIB_CP:$OUT" FwRewrite "$OUT/x/classes.dex" "$OUT/dex/classes.dex" "$OUT/x/classes-patched.dex"

python3 - "$OUT/orig.jar" "$OUT/x/classes-patched.dex" "$OUT/framework.jar" <<'PY'
import sys, zipfile
base, dex, out = sys.argv[1:4]
zin = zipfile.ZipFile(base); zout = zipfile.ZipFile(out, 'w')
for i in zin.infolist():
    data = open(dex,'rb').read() if i.filename == 'classes.dex' else zin.read(i.filename)
    zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
    zi.compress_type = i.compress_type; zi.external_attr = i.external_attr
    zi.create_system = i.create_system
    zout.writestr(zi, data)
zout.close(); zin.close()
PY
$ZIPALIGN -f -p 4 "$OUT/framework.jar" "$OUT/framework-aligned.jar"
"$HDC" shell "[ -f $ASX/fw/framework.jar.bak-pre464 ] || cp $ASX/fw/framework.jar $ASX/fw/framework.jar.bak-pre464"
push "$OUT/framework-aligned.jar" "$ASX/fw/framework.jar"
echo "framework.jar patched + deployed — RESTART AND CONFIRM IT BOOTS before doing anything else"
