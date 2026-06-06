#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
EV=$HOME/openharmony/docs/engine/V3-NOICE-DPAD-FINDINGS/navtour
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
mkdir -p "$EV"
sh "param set persist.sys.usb.config hdc; sync"; echo "[reboot for clean state]"; sh "reboot"; sleep 60
$HDC kill >/dev/null 2>&1; sleep 2; $HDC start >/dev/null 2>&1; sleep 3
for i in $(seq 1 18); do T=$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key'|head -1); [ -n "$T" ] && break; /mnt/c/Windows/System32/taskkill.exe /F /IM hdc.exe>/dev/null 2>&1; sleep 2; $HDC start>/dev/null 2>&1; sleep 6; done
[ -z "$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key')" ] && { echo HDC-DROPPED; exit 0; }
sh "mkdir -p /dev/memcg/perf_sensitive 2>/dev/null; setenforce 0"
sh "setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"; sleep 14
sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0"
sh "power-shell wakeup; power-shell timeout -o 86400000; param set persist.sys.usb.config none" >/dev/null 2>&1
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
[ -z "$P" ] && { echo "no-populated"; echo TOURDONE; exit 0; }
cap() { local name="$1" tap="$2" slp="${3:-4}"; [ -n "$tap" ] && sh "echo '$tap' > /data/local/tmp/noice_tap"; sleep "$slp"; sh "snapshot_display -f /data/local/tmp/t.jpeg >/dev/null 2>&1"; $HDC file recv /data/local/tmp/t.jpeg "$EV/$name.jpeg" >/dev/null 2>&1; echo "  captured $name : pid=$(npid) shot=$(sh 'stat -c%s /data/local/tmp/t.jpeg')"; }
echo "=== NAV TOUR (capturing each page) ==="
cap "1-library"     "1" 4
cap "2-soundinfo"   "84 337" 5
cap "3-back"        "1" 3
cap "4-volume"      "492 337" 5
cap "5-back"        "1" 3
cap "6-saved"       "2" 5
cap "7-sleeptimer"  "3" 5
cap "8-alarms"      "4" 5
cap "9-addalarm"    "648 1090" 6
cap "10-back"       "1" 3
cap "11-account"    "5" 5
echo "final pid=$(npid)"
echo TOURDONE
