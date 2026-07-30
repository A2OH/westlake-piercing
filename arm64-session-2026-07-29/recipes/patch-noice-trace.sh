#!/bin/bash
# §480 — inject execution tracing into the DEPLOYED noice.apk to find where the play path stops.
#
# Needed because normal introspection is unavailable here: libart's throw probe caps at 40 and is
# saturated, Throwable.getStackTrace() returns empty, and printStackTrace is a no-op. So instead of
# catching a throw, trace which branches actually execute.
#
# Starts from the apk ON THE DEVICE, so the §440 WlProxy surgery already in it is preserved.
# Backup: noice.apk.pre480
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
OUT="${OUT:-/tmp/noice-trace}"; rm -rf "$OUT"; mkdir -p "$OUT/cls" "$OUT/dex" "$OUT/stub/adapter/compat"

recv "$ASX/noice.apk" "$OUT/orig.apk" || exit 1
( cd "$OUT" && unzip -o -q orig.apk classes.dex )

# WlProbe lives in the BCP; compile against a throwaway stub and dex ONLY WlTrace.
cat > "$OUT/stub/adapter/compat/WlProbe.java" <<'J'
package adapter.compat;
public final class WlProbe { public static void log(String s) { } }
J
# ★WlTrace must NOT be compiled against android.jar: it is plain Java and android.jar lacks
# LambdaMetafactory, which breaks anything the JDK desugars.
javac -nowarn -source 8 -target 8 -d "$OUT/cls" \
  "$OUT/stub/adapter/compat/WlProbe.java" "$HERE/../java/westlake/WlTrace.java" 2>/dev/null
"$D8" --release --min-api 30 --output "$OUT/dex" "$OUT/cls/westlake/WlTrace.class"
test -f "$OUT/dex/classes.dex" || { echo "FAIL: d8 produced no dex"; exit 1; }

javac -nowarn -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/DexMerge.java" "$HERE/../tools/InjectTrace.java"
java -Xmx4g -cp "$DEXLIB_CP:$OUT" DexMerge "$OUT/classes.dex" "$OUT/dex/classes.dex" "$OUT/merged.dex"

SPM="Lcom/github/ashutoshgngwr/noice/engine/SoundPlayerManager;"
FOCUS="Lcom/github/ashutoshgngwr/noice/engine/a;"
SVC="Lcom/github/ashutoshgngwr/noice/service/SoundPlaybackService;"
LSP="Lcom/github/ashutoshgngwr/noice/engine/LocalSoundPlayer;"
java -Xmx4g -cp "$DEXLIB_CP:$OUT" InjectTrace "$OUT/merged.dex" "$OUT/traced.dex" \
  "$SPM:g:-1:s0" \
  "$SPM:g:67:s1" \
  "$SPM:i:-1:s2" \
  "$FOCUS:b:-1:s3" \
  "$FOCUS:onAudioFocusChange:-1:s4" \
  "$SVC:onStartCommand:-1:s7" \
  "$SVC:onStartCommand:335:s8" \
  "$SVC:onStartCommand:339:s9" \
  "$LSP:e:-1:s5" \
  "$SPM:b:-1:s6" \
  "$LSP:o:-1:s10" \
  "$LSP:o:11:s11" \
  "$LSP:a:-1:s14" \
  "$LSP:b:-1:s15" \
  "$LSP:d:-1:s16" \
  "$LSP:m:-1:s17" \
  "Lcom/github/ashutoshgngwr/noice/engine/SoundPlayer;:k:-1:s18" \
  "$LSP:<init>:-1:s19" \
  "Lcom/github/ashutoshgngwr/noice/engine/MediaPlayer;:A:7:mpstate:0" \
  "$LSP:n:-1:s20" \
  "Lcom/github/ashutoshgngwr/noice/engine/LocalSoundPlayer\$loadSoundMetadataJob\$1;:v:-1:s22" \
  "Lcom/github/ashutoshgngwr/noice/engine/LocalSoundPlayer\$loadSoundMetadataJob\$1;:k:-1:s23"

python3 - "$OUT/orig.apk" "$OUT/traced.dex" "$OUT/noice.apk" <<'PY'
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
# ⚠️zipalign or every glyph renders wrong (misaligned mmap'd resources.arsc).
"$ZIPALIGN" -f -p 4 "$OUT/noice.apk" "$OUT/noice-aligned.apk"
"$HDC" shell "[ -f $ASX/noice.apk.pre480 ] || cp $ASX/noice.apk $ASX/noice.apk.pre480"
push "$OUT/noice-aligned.apk" "$ASX/noice.apk"
echo "trace build deployed"
