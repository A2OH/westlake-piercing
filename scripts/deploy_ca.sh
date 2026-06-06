#!/bin/bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
WINDIR='C:\Users\dspfa\Dev\ohos-tools'; WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid() { for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }
$HDC kill >/dev/null 2>&1; $HDC start >/dev/null 2>&1; sleep 2
echo "[push CA bundle + install to /system/etc/security/cacerts]"
cp /tmp/cacerts.tgz "$WSLWIN/ca.tgz"; sh "rm -rf /data/local/tmp/ca.tgz"
$HDC file send "$WINDIR\\ca.tgz" /data/local/tmp/ca.tgz 2>&1|tr -d '\r'|grep -iE 'finish|fail'|tail -1
sh "mount -o remount,rw / 2>/dev/null; mkdir -p /system/etc/security/cacerts; cd /system/etc/security/cacerts && tar xzf /data/local/tmp/ca.tgz && echo extracted; chmod 644 /system/etc/security/cacerts/*.0; chown 0:0 /system/etc/security/cacerts/*.0; chcon u:object_r:system_file:s0 /system/etc/security/cacerts/*.0 2>/dev/null"
echo "cacerts now: $(sh 'ls /system/etc/security/cacerts/*.0 2>/dev/null | wc -l') certs"
echo "[force-stop + relaunch noice + test subscription LIVE load]"
sh "power-shell wakeup; /data/local/tmp/bpfgrant 13731 oh_sock_permission_map" >/dev/null 2>&1
P=""
for t in 1 2 3 4; do
  sh "aa force-stop com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 2
  sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null; cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null; chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
  sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 15
  P=$(npid); [ -n "$P" ] && break
done
[ -z "$P" ] && { echo no-spawn; echo CADONE; exit 0; }
S=/data/service/el1/public/appspawnx/adapter_child_$P.stderr
echo "noice=$P. Tapping subscription..."
sh "echo '5' > /data/local/tmp/noice_tap"; sleep 3
sh "echo '360 513' > /data/local/tmp/noice_tap"; sleep 9
echo "after subscription: pid=$(npid)"
echo "=== still 'network unreachable'? (SSL error gone?) ==="
sh "grep -aiE 'SSLHandshake|CertPath|trust anchor|TrustManager|网络|SubscriptionPlan|listPlans.*(ok|success)' $S 2>/dev/null | grep -avE 'FIX-VTABLE' | tail -6"
cd $HOME/openharmony/docs/engine/V3-NOICE-DPAD-FINDINGS
# capture with re-foreground (focus race)
for k in 1 2 3; do sh "aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice">/dev/null 2>&1; sh "snapshot_display -f /data/local/tmp/cas$k.jpeg >/dev/null 2>&1"; $HDC file recv /data/local/tmp/cas$k.jpeg subscription-castest_$k.jpeg >/dev/null 2>&1; echo "cap$k=$(sh 'stat -c%s /data/local/tmp/cas'$k'.jpeg')"; sleep 2; done
echo CADONE
