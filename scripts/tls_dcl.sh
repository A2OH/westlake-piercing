#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
WINDIR='C:\Users\dspfa\Dev\ohos-tools'; WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools
BIMG=/tmp/tagsoup-boot/out/boot-image
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
EV=$HOME/openharmony/docs/engine/V3-NOICE-DPAD-FINDINGS
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
warm() { for p in $(sh "pgrep -f appspawn-x"); do sh "kill -9 $p">/dev/null 2>&1; done; sleep 2; sh "setenforce 0; setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"; sleep 14; sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0; power-shell wakeup; power-shell timeout -o 86400000">/dev/null 2>&1; for g in 1 2 3 4; do sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map >/dev/null 2>&1"; sleep 2; done; }
echo "[1] repackage jar + extra dex"
cp /tmp/tlsshim/adapter-runtime-bcp.jar /tmp/tlsshim/arb3.jar
( cd /tmp/tlsshim && zip -j arb3.jar classes.dex >/dev/null 2>&1 )
cp /tmp/tlsshim/arb3.jar /tmp/tagsoup-boot/out/adapter/adapter-runtime-bcp.jar
echo "[2] boot regen"
cd $HOME/openharmony; WORK=/tmp/tagsoup-boot bash docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh >/tmp/dcl_regen.log 2>&1
echo "regen tail: $(tail -1 /tmp/dcl_regen.log)"
echo "[3] deploy jar + boot + tlsjni-extra.dex"
$HDC kill >/dev/null 2>&1; $HDC start >/dev/null 2>&1; sleep 2; sh "mount -o remount,rw / 2>/dev/null"
cp /tmp/tlsshim/arb3.jar "$WSLWIN/arb3.jar"; sh "rm -f /data/local/tmp/arb3.jar"; $HDC file send "$WINDIR\\arb3.jar" /data/local/tmp/arb3.jar >/dev/null 2>&1
sh "cp /data/local/tmp/arb3.jar /system/android/framework/adapter-runtime-bcp.jar; chmod 644 /system/android/framework/adapter-runtime-bcp.jar; chcon u:object_r:system_file:s0 /system/android/framework/adapter-runtime-bcp.jar 2>/dev/null"
cp /tmp/tlsjava/dexout/classes.dex "$WSLWIN/tlsx.dex"; sh "rm -f /data/local/tmp/tlsx.dex"; $HDC file send "$WINDIR\\tlsx.dex" /data/local/tmp/tlsx.dex >/dev/null 2>&1
sh "cp /data/local/tmp/tlsx.dex /system/android/framework/tlsjni-extra.dex; chmod 644 /system/android/framework/tlsjni-extra.dex; chcon u:object_r:system_file:s0 /system/android/framework/tlsjni-extra.dex 2>/dev/null"
cd "$BIMG"; for f in *; do cp "$f" "$WSLWIN/bg_$f"; sh "rm -f /data/local/tmp/bg_$f"; $HDC file send "$WINDIR\\bg_$f" /data/local/tmp/bg_$f >/dev/null 2>&1; sh "cp /data/local/tmp/bg_$f /system/android/framework/arm/$f"; done
sh "chmod 644 /system/android/framework/arm/boot*; chcon u:object_r:system_file:s0 /system/android/framework/arm/boot* 2>/dev/null; sync"
echo "extra dex on device: $(sh 'ls -la /system/android/framework/tlsjni-extra.dex | awk "{print \$5}"')"
echo "[4] reboot"; sh "param set persist.sys.usb.config hdc_debug; param set persist.usb.setting.gadget_conn_prompt false; sync"; sh "reboot"; sleep 65
$HDC kill >/dev/null 2>&1; sleep 2; $HDC start >/dev/null 2>&1; sleep 3
for i in $(seq 1 18); do T=$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key'|head -1); [ -n "$T" ] && break; /mnt/c/Windows/System32/taskkill.exe /F /IM hdc.exe>/dev/null 2>&1; sleep 2; $HDC start>/dev/null 2>&1; sleep 6; done
[ -z "$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key')" ] && { echo HDC-DROPPED; exit 0; }
sh "mkdir -p /dev/memcg/perf_sensitive 2>/dev/null; setenforce 0; setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"; sleep 15
sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0; power-shell wakeup; power-shell timeout -o 86400000" >/dev/null 2>&1
echo "[5] warm-retry + validate"
RESULT="(none)"
for cyc in $(seq 1 8); do
  echo "=== cycle $cyc ==="; warm
  sh "rm -f /data/local/tmp/httptest.log; aa force-stop com.github.ashutoshgngwr.noice >/dev/null 2>&1"; sleep 2
  sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null; cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null; chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
  sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1
  for w in $(seq 1 10); do sleep 5; d=$(sh "grep -c 'NetTest done' /data/local/tmp/httptest.log 2>/dev/null"); [ "${d:-0}" -gt 0 ] && break; done
  api=$(sh "grep -m1 'api.trynoice' /data/local/tmp/httptest.log 2>/dev/null"); echo "  noice=$(npid)  api: $api"
  if echo "$api" | grep -qE '\-> HTTP [0-9]'; then RESULT="TLS-OK"; echo "  *** NATIVE TLS WORKS ***"; sh "cat /data/local/tmp/httptest.log 2>/dev/null|head -8"; break; fi
  if echo "$api" | grep -qvE 'android_getaddrinfo|missing INTERNET' && [ -n "$api" ]; then RESULT="TLS-ERR"; echo "  *** DNS through, result: ***"; sh "cat /data/local/tmp/httptest.log 2>/dev/null|head -12"; break; fi
done
echo "RESULT=$RESULT"
if [ -n "$(npid)" ]; then sh "echo '5' > /data/local/tmp/noice_tap"; sleep 3; sh "echo '360 513' > /data/local/tmp/noice_tap"; sleep 9; for k in 1 2 3; do sh "aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice">/dev/null 2>&1; sleep 1; sh "snapshot_display -f /data/local/tmp/dv$k.jpeg>/dev/null 2>&1"; $HDC file recv /data/local/tmp/dv$k.jpeg $EV/tls-dcl_$k.jpeg >/dev/null 2>&1; echo "cap$k=$(sh 'stat -c%s /data/local/tmp/dv'$k'.jpeg')"; sleep 2; done; fi
echo TLSDCLDONE
