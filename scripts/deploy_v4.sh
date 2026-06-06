#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
WINDIR='C:\Users\dspfa\Dev\ohos-tools'; WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
$HDC kill >/dev/null 2>&1; $HDC start >/dev/null 2>&1; sleep 2
echo "[push libv4force + add to LD_PRELOAD]"
cp /tmp/libv4force.so "$WSLWIN/v4.so"; sh "rm -rf /data/local/tmp/v4.so"
$HDC file send "$WINDIR\\v4.so" /data/local/tmp/v4.so 2>&1|tr -d '\r'|grep -iE 'finish|fail'|tail -1
sh "mount -o remount,rw / 2>/dev/null; cp /data/local/tmp/v4.so /system/android/lib/libv4force.so; chmod 644 /system/android/lib/libv4force.so; chcon u:object_r:system_lib_file:s0 /system/android/lib/libv4force.so 2>/dev/null"
sh "grep -q libv4force /data/local/tmp/start_asx.sh || sed -i 's#libjdnshook.so#libjdnshook.so:/system/android/lib/libv4force.so#' /data/local/tmp/start_asx.sh"
sh "grep LD_PRELOAD /data/local/tmp/start_asx.sh"
echo "[re-enable IPv6 (prove libv4force is the fix, not the disable)]"
sh "echo 0 > /proc/sys/net/ipv6/conf/all/disable_ipv6 2>&1; echo 0 > /proc/sys/net/ipv6/conf/wlan0/disable_ipv6 2>&1"
echo "[restart appspawn-x]"
sh "aa force-stop com.github.ashutoshgngwr.noice" >/dev/null 2>&1
for p in $(sh "pgrep -f appspawn-x"); do sh "kill -9 $p" >/dev/null 2>&1; done; sleep 2
sh "setenforce 0; setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"; sleep 13
sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0"
sh "power-shell wakeup; power-shell timeout -o 86400000; /data/local/tmp/bpfgrant 13731 oh_sock_permission_map; rm -f /data/local/tmp/netlog.log" >/dev/null 2>&1
echo "[cold launch noice EMPTY cache -> live library fetch via IPv4]"
P=""
for t in 1 2 3 4; do
  sh "aa force-stop com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 2
  sh "rm -f $BASE/databases/com.github.ashutoshgngwr.noice.db* 2>/dev/null; rm -rf $BASE/cache/cdn-cache/* $BASE/cache/okhttp* 2>/dev/null"
  sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 18
  P=$(npid); [ -z "$P" ] && { echo "t$t no-spawn"; continue; }
  break
done
[ -z "$P" ] && { echo no-spawn; echo V4DONE; exit 0; }
sleep 10
SZ=$(sh "snapshot_display -f /data/local/tmp/v4lib.jpeg >/dev/null 2>&1; stat -c%s /data/local/tmp/v4lib.jpeg")
echo "library shot=$SZ (>45k = LIVE LOAD WORKS = CONNECTIVITY FIXED)"
echo "=== netlog: connect result now (should succeed via AF_INET) ==="
sh "grep -aE 'socket\]|connect\]' /data/local/tmp/netlog.log 2>/dev/null | tail -6"
cd $HOME/openharmony/docs/engine/V3-NOICE-DPAD-FINDINGS
$HDC file recv /data/local/tmp/v4lib.jpeg library-v4fixed.jpeg >/dev/null 2>&1
echo V4DONE
