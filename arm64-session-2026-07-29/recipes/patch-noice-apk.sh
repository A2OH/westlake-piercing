#!/bin/bash
# App-dex surgery: rewrite invoke-interface-on-Proxy call sites to invoke-static into a merged
# westlake/WlProxy helper. This is what made the sound library load.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
OUT="${OUT:-/tmp/noice-patch}"; rm -rf "$OUT"; mkdir -p "$OUT/cls" "$OUT/dex"
recv "$ASX/noice.apk" "$OUT/orig.apk" || exit 1
cd "$OUT" && unzip -o -q orig.apk classes.dex

javac -nowarn -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -d "$OUT/cls" "$HERE/../java/westlake/WlProxy.java"
$D8 --release --min-api 30 --lib "$ANDROID_JAR" --output "$OUT/dex" $(find "$OUT/cls" -name '*.class')

javac -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/FindSites.java" "$HERE/../tools/DexRewrite.java"
echo "--- call sites before ---"
java -cp "$DEXLIB_CP:$OUT" FindSites "$OUT/classes.dex"
java -cp "$DEXLIB_CP:$OUT" DexRewrite "$OUT/classes.dex" "$OUT/dex/classes.dex" "$OUT/classes-patched.dex"

python3 - "$OUT/orig.apk" "$OUT/classes-patched.dex" "$OUT/noice.apk" <<'PY'
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
# ⚠️CRITICAL: without zipalign, resources.arsc lands misaligned and EVERY UI string renders as the
# wrong glyph ("Settings" -> "JettinOs"). It looks exactly like a font bug. Always A/B the baseline.
$ZIPALIGN -f -p 4 "$OUT/noice.apk" "$OUT/noice-aligned.apk"
"$HDC" shell "[ -f $ASX/noice.apk.pre440 ] || cp $ASX/noice.apk $ASX/noice.apk.pre440"
push "$OUT/noice-aligned.apk" "$ASX/noice.apk"
echo "noice.apk patched + deployed"
