#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
P=$(npid)
echo "=== noice netns vs shell netns (confirm the theory) ==="
echo "shell net ns: $(sh 'readlink /proc/self/ns/net')"
[ -n "$P" ] && echo "noice($P) net ns: $(sh "readlink /proc/$P/ns/net")"
echo "=== broker map exists on device? ==="
sh "ls -la /sys/fs/bpf/netsys/maps/broker_sock_permission_map /sys/fs/bpf/netsys/maps/oh_sock_permission_map 2>/dev/null"
echo "=== GRANT broker_sock_permission_map[13731]=1 ==="
sh "/data/local/tmp/bpfgrant 13731 broker_sock_permission_map 2>&1 | tail -4"
echo "=== also re-grant oh map (belt+suspenders) ==="
sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map >/dev/null 2>&1"
echo "=== relaunch noice + NetTest ==="
sh "rm -f /data/local/tmp/httptest.log; aa force-stop com.github.ashutoshgngwr.noice >/dev/null 2>&1"; sleep 2
sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null; cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null; chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1
for w in $(seq 1 12); do sleep 5; d=$(sh "grep -c 'NetTest done' /data/local/tmp/httptest.log 2>/dev/null"); [ "${d:-0}" -gt 0 ] && break; done
echo "noice=$(npid)"
echo "=== HTTPS RESULT (broker map granted) ==="
sh "cat /data/local/tmp/httptest.log 2>/dev/null | head -14"
echo BROKERDONE
