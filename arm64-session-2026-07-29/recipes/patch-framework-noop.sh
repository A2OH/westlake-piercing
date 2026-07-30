#!/bin/bash
# §478 — no-op framework methods that only throw on this runtime and whose work we do not need.
#
# Applies to the CURRENTLY DEPLOYED framework.jar (additive), unlike patch-framework-jar.sh which
# must start from a pristine baseline because it asserts an exact rewrite-site count.
#
# MediaRouter.updateWifiDisplayStatus NPEs on every route scan (the display service hands back a null
# WifiDisplayStatus here) and fired 23 times in two minutes. libart's throw probe caps at 40 TOTAL, so
# this inert throw was crowding out the exception actually blocking playback.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
OUT="${OUT:-/tmp/fw-noop}"; rm -rf "$OUT"; mkdir -p "$OUT/x"
recv "$ASX/fw/framework.jar" "$OUT/orig.jar" || exit 1
( cd "$OUT/x" && unzip -o -q ../orig.jar 'classes*.dex' )

javac -nowarn -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/PatchNoOp.java"
# All of these deref a WifiDisplayStatus that is null on this board (no wifi-display support) and are
# called on every route scan. MediaRouter.selectRouteStatic derefs it too but is deliberately NOT
# no-op'd here: it performs real route selection and muzzling it could break audio output routing.
# That is why ~21 of these NPEs still remain; silence them at the source (a non-null WifiDisplayStatus
# from the display service) rather than by disabling more of MediaRouter.
java -Xmx4g -cp "$DEXLIB_CP:$OUT" PatchNoOp "$OUT/x/classes2.dex" "$OUT/x/classes2-patched.dex" \
  "Landroid/media/MediaRouter;" updateWifiDisplayStatus getWifiDisplayStatusCode isWifiDisplayEnabled

python3 - "$OUT/orig.jar" "$OUT/x/classes2-patched.dex" "$OUT/framework.jar" <<'PY'
import sys, zipfile
base, dex, out = sys.argv[1:4]
zin = zipfile.ZipFile(base); zout = zipfile.ZipFile(out, 'w')
if 'classes2.dex' not in zin.namelist():
    sys.exit('FATAL: %s has no classes2.dex' % base)
for i in zin.infolist():
    data = open(dex,'rb').read() if i.filename == 'classes2.dex' else zin.read(i.filename)
    zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
    zi.compress_type = i.compress_type; zi.external_attr = i.external_attr
    zi.create_system = i.create_system
    zout.writestr(zi, data)
zout.close(); zin.close()
PY
"$ZIPALIGN" -f -p 4 "$OUT/framework.jar" "$OUT/framework-aligned.jar"
"$HDC" shell "[ -f $ASX/fw/framework.jar.bak-pre478 ] || cp $ASX/fw/framework.jar $ASX/fw/framework.jar.bak-pre478"
push "$OUT/framework-aligned.jar" "$ASX/fw/framework.jar"
echo "framework.jar no-op patch applied — RESTART AND CONFIRM IT BOOTS"
