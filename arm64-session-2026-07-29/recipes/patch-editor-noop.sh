#!/bin/bash
# §553 — no-op Editor.loadHandleDrawables so a text field can be focused at all.
#
# WHY: tapping ANY EditText cancelled its own touch, so no text field on the board could be typed
# into. Traced live:
#   [WESTLAKE-ASSET2] png path='res/drawable-xhdpi-v4/text_select_handle_middle_mtrl_alpha.png' len=508
#   [WESTLAKE-XMLTHROW] XmlPullParserException
#       at BitmapDrawable.updateStateFromTypedArray(BitmapDrawable.java:854)   <- "requires a valid 'src'"
#       at DrawableInflater.inflateFromXmlForDensity ... Resources.getDrawable
#       at TextView.getTextSelectHandle(TextView.java:3881)
#       at Editor.loadHandleDrawables(Editor.java:7199)
#       at View.dispatchTouchEvent(View.java:15655)
# The PNG is FOUND (508 bytes) but fails to DECODE — these `*_mtrl_alpha.png` are ALPHA_8 / grayscale
# PNGs and this port's decoder returns null for them, so updateStateFromTypedArray throws. The
# exception escapes through View.dispatchTouchEvent, the gesture is cancelled (ACTION_CANCEL, seen as
# `nativeGetAction -> 3` right after `DOWN handled=1`), and the field never takes focus.
#
# The handles are purely the drag-to-select UI, which nothing here needs, so blank the loader. Typing
# and the caret work; only long-press selection handles are lost.
# ⚠️Narrower than patching BitmapDrawable.updateStateFromTypedArray, which would silently turn EVERY
# undecodable bitmap into an empty drawable across the whole framework.
# The real cure is ALPHA_8 PNG decoding.
#
# Additive: starts from the CURRENTLY DEPLOYED framework.jar. Editor lives in classes4.dex here.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
KIT="$(cd "$HERE/.." && pwd)"
board_online || exit 1
OUT="${OUT:-/tmp/fw-noop553}"; rm -rf "$OUT"; mkdir -p "$OUT/x"
recv "$ASX/fw/framework.jar" "$OUT/orig.jar" || exit 1
( cd "$OUT/x" && unzip -o -q ../orig.jar 'classes*.dex' )
javac -nowarn -cp "$DEXLIB_CP" -d "$OUT" "$KIT/tools/PatchNoOp.java"
java -cp "$DEXLIB_CP:$OUT" PatchNoOp "$OUT/x/classes4.dex" "$OUT/x/classes4-patched.dex" \
  "Landroid/widget/Editor;" loadHandleDrawables

python3 - "$OUT/orig.jar" "$OUT/x/classes4-patched.dex" "$OUT/framework.jar" <<'PY'
import sys, zipfile
base, dex, out = sys.argv[1:4]
zin = zipfile.ZipFile(base); zout = zipfile.ZipFile(out, 'w')
if 'classes4.dex' not in zin.namelist():
    sys.exit('FATAL: %s has no classes4.dex' % base)
swapped = 0
for i in zin.infolist():
    if i.filename == 'classes4.dex':
        data = open(dex,'rb').read(); swapped += 1
    else:
        data = zin.read(i.filename)
    zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
    zi.compress_type = i.compress_type; zi.external_attr = i.external_attr
    zi.create_system = i.create_system
    zout.writestr(zi, data)
zout.close(); zin.close()
print('swapped classes4.dex:', swapped)
PY
"$ZIPALIGN" -f -p 4 "$OUT/framework.jar" "$OUT/framework-aligned.jar"
"$HDC" shell "[ -f $ASX/fw/framework.jar.pre553 ] || cp $ASX/fw/framework.jar $ASX/fw/framework.jar.pre553"
push "$OUT/framework-aligned.jar" "$ASX/fw/framework.jar"
echo "§553 applied — RESTART AND CONFIRM IT BOOTS"
