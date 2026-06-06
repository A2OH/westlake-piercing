#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map; power-shell wakeup" >/dev/null 2>&1
echo "=== cold launch with EMPTY cache+db (forces LIVE library fetch) ==="
sh "aa force-stop com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 2
sh "rm -f $BASE/databases/com.github.ashutoshgngwr.noice.db* 2>/dev/null; rm -rf $BASE/cache/cdn-cache/* 2>/dev/null; rm -rf $BASE/cache/okhttp* 2>/dev/null"
P=""
for t in 1 2 3; do
  [ "$t" -gt 1 ] && { sh "aa force-stop com.github.ashutoshgngwr.noice">/dev/null 2>&1; sleep 2; }
  sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 18
  P=$(npid); [ -n "$P" ] && break
done
[ -z "$P" ] && { echo no-spawn; echo LTDONE; exit 0; }
S=/data/service/el1/public/appspawnx/adapter_child_$P.stderr
echo "noice=$P. Waiting for library load attempt..."
sleep 8
SZ=$(sh "snapshot_display -f /data/local/tmp/live.jpeg >/dev/null 2>&1; stat -c%s /data/local/tmp/live.jpeg")
echo "library shot=$SZ (>45k=POPULATED from LIVE network; <25k=blank=live fetch FAILED)"
echo "=== okhttp/network activity for the live library fetch ==="
sh "grep -aiE 'library.json|cdn.trynoice|okhttp.*(connect|response|fail)|SSL|handshake|UnknownHost|ConnectException|SocketTimeout|Unable to resolve|NetworkOnMain' $S 2>/dev/null | grep -avE 'FIX-VTABLE' | tail -10"
cd $HOME/openharmony/docs/engine/V3-NOICE-DPAD-FINDINGS
$HDC file recv /data/local/tmp/live.jpeg library-livefetch.jpeg >/dev/null 2>&1
echo LTDONE
