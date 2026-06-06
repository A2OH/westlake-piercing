#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
WINDIR='C:\Users\dspfa\Dev\ohos-tools'; WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools
BUNDLE=/data/app/el1/bundle/public/com.github.ashutoshgngwr.noice/android
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
$HDC kill >/dev/null 2>&1; $HDC start >/dev/null 2>&1; sleep 2
echo "[deploy coroutine-fixed noice]"
cp /tmp/noice-coro-signed.apk "$WSLWIN/nc.apk"; sh "rm -rf /data/local/tmp/nc.apk"
$HDC file send "$WINDIR\\nc.apk" /data/local/tmp/nc.apk 2>&1|tr -d '\r'|grep -iE 'finish|fail'|tail -1
sh "mount -o remount,rw / 2>/dev/null; cp /data/local/tmp/nc.apk $BUNDLE/base.apk; chmod 644 $BUNDLE/base.apk; chown 0:0 $BUNDLE/base.apk; chcon u:object_r:app_install_file:s0 $BUNDLE/base.apk 2>/dev/null; rm -rf $BUNDLE/oat $BASE/code_cache"
sh "param set persist.sys.usb.config hdc_debug; param set persist.usb.setting.gadget_conn_prompt false; sync"
echo "[reboot]"; sh "reboot"; sleep 60
$HDC kill >/dev/null 2>&1; sleep 2; $HDC start >/dev/null 2>&1; sleep 3
for i in $(seq 1 18); do T=$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key'|head -1); [ -n "$T" ] && break; /mnt/c/Windows/System32/taskkill.exe /F /IM hdc.exe>/dev/null 2>&1; sleep 2; $HDC start>/dev/null 2>&1; sleep 6; done
[ -z "$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key')" ] && { echo HDC-DROPPED; exit 0; }
sh "mkdir -p /dev/memcg/perf_sensitive 2>/dev/null; setenforce 0"
sh "setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"; sleep 14
sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0"
sh "power-shell wakeup; power-shell timeout -o 86400000" >/dev/null 2>&1
sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map; hilog -p off; param set hilog.private.on false" >/dev/null 2>&1
P=""
for t in 1 2 3 4; do
  [ "$t" -gt 1 ] && { sh "aa force-stop com.github.ashutoshgngwr.noice">/dev/null 2>&1; sleep 2; }
  sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null; cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null; chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
  sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 16
  P=$(npid); [ -z "$P" ] && { echo "t$t no-spawn"; continue; }
  SZ=$(sh "snapshot_display -f /data/local/tmp/i.jpeg>/dev/null 2>&1; stat -c%s /data/local/tmp/i.jpeg")
  echo "t$t pid=$P shot=$SZ"; [ "${SZ:-0}" -gt 45000 ] && break
done
[ -z "$P" ] && { echo "no-populated"; echo DCDONE; exit 0; }
echo "noice rendering pid=$P. focus=$(sh "hidumper -s WindowManagerService -a '-a' 2>/dev/null"|grep -i 'Focus window'|grep -oE '[0-9]+'|head -1)"
echo "=== COROUTINE FIX TEST: subscription + Register x3 each (should NOT crash now) ==="
ck() { local label="$1" tap1="$2" tap2="$3"; local q=$(npid); [ -z "$q" ] && { for r in 1 2 3; do sh "aa force-stop com.github.ashutoshgngwr.noice">/dev/null 2>&1; sleep 2; sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null; chown -R 13731:13731 $BASE/databases 2>/dev/null; power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice">/dev/null 2>&1; sleep 15; q=$(npid); [ -n "$q" ] && break; done; }
  sh "echo '$tap1' > /data/local/tmp/noice_tap"; sleep 3; sh "echo '$tap2' > /data/local/tmp/noice_tap"; sleep 7; local now=$(npid)
  if [ "$now" = "$q" ]; then echo "  [PASS] $label SURVIVED (pid $now)"; else echo "  [FAIL] $label CRASHED ($q->${now:-dead})"; fi
  sh "echo '40 60' > /data/local/tmp/noice_tap"; sleep 2; sh "echo '1' > /data/local/tmp/noice_tap"; sleep 2; }
ck "subscription #1" "5" "360 513"
ck "subscription #2" "5" "360 513"
ck "subscription #3" "5" "360 513"
ck "register #1" "5" "360 257"
ck "register #2" "5" "360 257"
ck "register #3" "5" "360 257"
echo "final pid=$(npid)"
echo DCDONE
