#!/bin/bash
# §508 — no-op android.media.PlayerBase's AudioService bookkeeping calls.
#
# Every AudioTrack build was dying here:
#
#   java.lang.IllegalArgumentException
#     at android.media.PlayerBase.baseRegisterPlayer(PlayerBase.java:123)
#     at android.media.AudioTrack.<init>(AudioTrack.java:908)
#     at android.media.AudioTrack$Builder.build(AudioTrack.java:1450)
#     at com.google.android.exoplayer2.audio.DefaultAudioSink$f.b(...)   <- buildAudioTrack
#
# which is why the shim log showed AudioTrack created -> started -> released in a loop with
# writeCalls=0 and the OH renderer reporting "volume data counts: 0": ExoPlayer never got a usable
# track to write PCM into.
#
# baseRegisterPlayer exists solely to register the player with AudioService so that service can do
# volume/focus/player bookkeeping. This port HAS NO AudioService — and its IAudioService is a dynamic
# Proxy, which additionally trips the §436 invoke-interface defect (see the standing
# "No InvokeType(4) method isVolumeFixed()Z in class android.media.IAudioService" throws). There is
# nothing on the other end to register with, so doing nothing is the honest behaviour.
#
# ⚠️How this differs from §478, which was reverted as over-reach: that one muzzled MediaRouter
# throws that were genuinely INERT, purely so they would stop saturating libart's 40-slot throw probe
# — editing the platform to fix instrumentation. This one removes a call that actively breaks
# playback and whose remote peer does not exist. Different justification entirely.
#
# ★Start minimal. Only baseRegisterPlayer is no-op'd here. If baseStart/basePause/baseStop then throw
# on the same missing service, add them ONE at a time and re-verify — §506 is the cautionary tale for
# binding/blanking a batch on prediction rather than on evidence.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
OUT="${OUT:-/tmp/fw-playerbase}"; rm -rf "$OUT"; mkdir -p "$OUT/x"
recv "$ASX/fw/framework.jar" "$OUT/orig.jar" || exit 1
( cd "$OUT/x" && unzip -o -q ../orig.jar 'classes*.dex' )

javac -nowarn -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/PatchNoOp.java" "$HERE/../tools/HasCls.java" 2>/dev/null \
  || javac -nowarn -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/PatchNoOp.java"

# PlayerBase may live in either dex depending on how framework.jar was split; find it rather than
# assuming classes2.dex the way the §478 recipe does.
DEX=""
for d in "$OUT"/x/classes*.dex; do
  if java -Xmx4g -cp "$DEXLIB_CP:$OUT" PatchNoOp "$d" "$OUT/probe.dex" \
       "Landroid/media/PlayerBase;" baseRegisterPlayer >/dev/null 2>&1; then
    DEX="$d"; break
  fi
done
[ -n "$DEX" ] || { echo "FATAL: android.media.PlayerBase not found in any classes*.dex"; exit 1; }
echo "PlayerBase lives in $(basename "$DEX")"

# baseStart is the documented follow-on, added on evidence rather than prediction:
# after no-op'ing baseRegisterPlayer alone, the shim log changed from
#   native_setup -> start -> release      (IAE thrown from baseRegisterPlayer)
# to
#   native_setup -> release               (no start at all)
# native_start is only reached from AudioTrack.play() AFTER baseStart(), so play() now throws before
# it. That is this patch's own consequence: with baseRegisterPlayer neutered, mPlayerIId keeps the
# invalid value 0, and baseStart hands that to getService().playerEvent(...). Same missing
# AudioService, one method further along.
java -Xmx4g -cp "$DEXLIB_CP:$OUT" PatchNoOp "$DEX" "$OUT/patched.dex" \
  "Landroid/media/PlayerBase;" baseRegisterPlayer baseStart

python3 - "$OUT/orig.jar" "$OUT/patched.dex" "$(basename "$DEX")" "$OUT/framework.jar" <<'PY'
import sys, zipfile
base, dex, name, out = sys.argv[1:5]
zin = zipfile.ZipFile(base); zout = zipfile.ZipFile(out, 'w')
if name not in zin.namelist():
    sys.exit('FATAL: %s has no %s' % (base, name))
for i in zin.infolist():
    data = open(dex,'rb').read() if i.filename == name else zin.read(i.filename)
    zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
    zi.compress_type = i.compress_type; zi.external_attr = i.external_attr
    zi.create_system = i.create_system
    zout.writestr(zi, data)
zout.close(); zin.close()
PY

# ★zipalign -f -p 4 is mandatory after any python repack — skipping it has previously corrupted
# resource loading badly enough that every glyph rendered wrong.
"$ZIPALIGN" -f -p 4 "$OUT/framework.jar" "$OUT/framework-aligned.jar"
"$HDC" shell "cp $ASX/fw/framework.jar $ASX/fw/framework.jar.bak-pre508" >/dev/null 2>&1
push "$OUT/framework-aligned.jar" "$ASX/fw/framework.jar" || exit 1
echo "§508 deployed. Rollback: cp $ASX/fw/framework.jar.bak-pre508 $ASX/fw/framework.jar"
echo "★ boot image staleness: check the vdex mtime vs the jar before trusting this took effect."
