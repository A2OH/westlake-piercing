#!/system/bin/sh
# §611 single-shot catalog launch: no retries, waits for the BIND verdict, reports diag counters.
cd /data/local/tmp/asx
# pkill removed - self-match hazard
kill -9 $(pidof appspawn-x) 2>/dev/null
sleep 3
rm -f asx.err
rm -f /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null
setenforce 0 2>/dev/null
env ASX_KEEP_THEME=1 ASX_DIRECT_LAUNCH=1 \
    ASX_LAUNCH_PKG=io.material.catalog \
    ASX_APK_PATH=/data/local/tmp/asx/catalog.apk \
    ASX_LAUNCH_ACTIVITY=io.material.catalog.main.MainActivity \
    nohup sh /data/local/tmp/asx/run_asx.sh >/dev/null 2>&1 &
i=0; while [ $i -lt 900 ]; do grep -q 'Ready to accept' asx.err 2>/dev/null && break; sleep 0.1; i=$((i+1)); done
./spawn_client /dev/unix/socket/AppSpawnX io.material.catalog >/dev/null 2>&1
j=0; C=""
while [ $j -lt 480 ]; do
  C=$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1)
  if [ -n "$C" ]; then
    grep -aq 'ensureBindApplication FAILED\|handleBindApplication returned OK' "$C" 2>/dev/null && break
  fi
  sleep 1; j=$((j+1))
done
echo "=== verdict after ${j}s, stderr=$C"
echo "nullarr_diag = $(grep -ac 'PFCUT-NPE-ARRAY-LENGTH' "$C" 2>/dev/null)"
echo "npe_diag     = $(grep -ac 'WESTLAKE-NPE-DIAG' "$C" 2>/dev/null)"
grep -a -m2 'PFCUT-NPE-ARRAY-LENGTH' "$C" 2>/dev/null
grep -aE 'ensureBindApplication FAILED|returned OK' "$C" | head -2
P=$(echo "$C" | sed 's|.*adapter_child_||; s|\.stderr||')
echo "child=$P $([ -d /proc/$P ] && echo ALIVE || echo DEAD)"
