#!/bin/bash
# Build the BCP helper jar: compile java/ -> dex -> replace classes2.dex in oh-adapter-framework.jar.
# These classes are what make TLS, the service binds, audio focus and the stub services work.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
SRC="$HERE/../java"; OUT="${OUT:-/tmp/ohaf-build}"
rm -rf "$OUT"; mkdir -p "$OUT/cls" "$OUT/dex"

# WlProbe uses lambdas, and android.jar has NO LambdaMetafactory -> compile it against the JDK.
javac -nowarn -source 8 -target 8 -d "$OUT/cls" "$SRC/adapter/compat/WlProbe.java"
# everything else needs android.jar
javac -nowarn -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -cp "$OUT/cls" -d "$OUT/cls" \
  "$SRC"/adapter/compat/Westlake*.java \
  "$SRC"/adapter/compat/WlAmsBind.java "$SRC"/adapter/compat/WlAudioFocus.java \
  "$SRC"/adapter/compat/WlShortcutService.java "$SRC"/adapter/compat/WlMediaSession.java \
  "$SRC"/android/net/ssl/SSLSockets.java
# ⚠️d8 REJECTS javac-21 anonymous inner classes and enums. Use named classes only.
# ⚠️If d8 fails it produces NO classes.dex — check for it before packing, or you ship a stale jar.
$D8 --release --min-api 30 --lib "$ANDROID_JAR" --output "$OUT/dex" $(find "$OUT/cls" -name '*.class')
test -f "$OUT/dex/classes.dex" || { echo "FAIL: d8 produced no dex"; exit 1; }

# pull the current jar and swap classes2.dex only
"$HDC" file recv "$ASX/fw/oh-adapter-framework.jar" "$(wslpath -w "$WIN_STAGE/ohaf-base.jar")" >/dev/null
cp "$WIN_STAGE/ohaf-base.jar" "$OUT/base.jar"
python3 - "$OUT/base.jar" "$OUT/dex/classes.dex" "$OUT/ohaf.jar" <<'PY'
import sys, zipfile
base, dex, out = sys.argv[1:4]
zin = zipfile.ZipFile(base); zout = zipfile.ZipFile(out, 'w')
for i in zin.infolist():
    data = open(dex,'rb').read() if i.filename == 'classes2.dex' else zin.read(i.filename)
    zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
    zi.compress_type = i.compress_type; zi.external_attr = i.external_attr
    zi.create_system = i.create_system
    zout.writestr(zi, data)
zout.close(); zin.close()
PY
# ⚠️ALWAYS zipalign: python zipfile misaligns STORED, mmap'd entries.
$ZIPALIGN -f -p 4 "$OUT/ohaf.jar" "$OUT/ohaf-aligned.jar"
push "$OUT/ohaf-aligned.jar" "$ASX/fw/oh-adapter-framework.jar"
echo "ohaf jar deployed"
