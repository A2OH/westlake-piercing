#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
WINDIR='C:\Users\dspfa\Dev\ohos-tools'; WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools
BIMG=/tmp/tagsoup-boot/out/boot-image
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
$HDC kill >/dev/null 2>&1; $HDC start >/dev/null 2>&1; sleep 2
sh "mount -o remount,rw / 2>/dev/null"
echo "[push libtlsjni.so]"
cp /tmp/libtlsjni.so "$WSLWIN/tlsjni.so"; sh "rm -f /data/local/tmp/tlsjni.so"
$HDC file send "$WINDIR\\tlsjni.so" /data/local/tmp/tlsjni.so 2>&1|tr -d '\r'|grep -iE 'finish|fail'|tail -1
sh "cp /data/local/tmp/tlsjni.so /system/android/lib/libtlsjni.so; chmod 644 /system/android/lib/libtlsjni.so; chcon u:object_r:system_lib_file:s0 /system/android/lib/libtlsjni.so 2>/dev/null"
echo "[push adapter-runtime-bcp.jar]"
cp /tmp/tlsshim/adapter-runtime-bcp-new.jar "$WSLWIN/arb.jar"; sh "rm -f /data/local/tmp/arb.jar"
$HDC file send "$WINDIR\\arb.jar" /data/local/tmp/arb.jar 2>&1|tr -d '\r'|grep -iE 'finish|fail'|tail -1
sh "cp /data/local/tmp/arb.jar /system/android/framework/adapter-runtime-bcp.jar; chmod 644 /system/android/framework/adapter-runtime-bcp.jar; chcon u:object_r:system_file:s0 /system/android/framework/adapter-runtime-bcp.jar 2>/dev/null"
echo "[push boot image -> /system/android/framework/arm/]"
cd "$BIMG"; n=0
for f in *; do cp "$f" "$WSLWIN/bi_$f"; sh "rm -f /data/local/tmp/bi_$f"; $HDC file send "$WINDIR\\bi_$f" /data/local/tmp/bi_$f >/dev/null 2>&1; sh "cp /data/local/tmp/bi_$f /system/android/framework/arm/$f" && n=$((n+1)); done
echo "boot files pushed: $n"
sh "chmod 644 /system/android/framework/arm/boot*; chcon u:object_r:system_file:s0 /system/android/framework/arm/boot* 2>/dev/null; sync"
echo "arb.jar dex md5: $(sh 'md5sum /system/android/framework/arm/boot-adapter-runtime-bcp.oat | cut -c1-8')"
echo "[reboot]"; sh "param set persist.sys.usb.config hdc_debug; param set persist.usb.setting.gadget_conn_prompt false; sync"; sh "reboot"; sleep 60
$HDC kill >/dev/null 2>&1; sleep 2; $HDC start >/dev/null 2>&1; sleep 3
for i in $(seq 1 18); do T=$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key'|head -1); [ -n "$T" ] && break; /mnt/c/Windows/System32/taskkill.exe /F /IM hdc.exe>/dev/null 2>&1; sleep 2; $HDC start>/dev/null 2>&1; sleep 6; done
[ -z "$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key')" ] && { echo HDC-DROPPED; exit 0; }
sh "mkdir -p /dev/memcg/perf_sensitive 2>/dev/null; setenforce 0"
sh "setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"; sleep 14
sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0"
sh "power-shell wakeup; power-shell timeout -o 86400000; /data/local/tmp/bpfgrant 13731 oh_sock_permission_map; rm -f /data/local/tmp/httptest.log" >/dev/null 2>&1
P=""
for t in 1 2 3 4 5; do
  sh "aa force-stop com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 2
  sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null; cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null; chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
  sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 20
  P=$(npid); [ -z "$P" ] && { echo "t$t no-spawn"; continue; }
  h=$(sh "grep -c NetTest /data/local/tmp/httptest.log 2>/dev/null"); h=${h:-0}
  echo "t$t pid=$P"; [ "$h" -gt 0 ] && break; sleep 5
done
echo "=== HTTPS TEST RESULT (native TLS) ==="
sh "cat /data/local/tmp/httptest.log 2>/dev/null | head -40"
echo TLSDEPLOYDONE
