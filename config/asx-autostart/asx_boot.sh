#!/system/bin/sh
# asx_boot.sh — boot-time AppSpawnX bringup for adapter apps (catalog + noice).
# Started by zz-asx-autostart.cfg (non-critical, oneshot). Mirrors the proven
# manual start_asx.sh + socket relabel + bpfgrant, backgrounded so children
# reparent to init and survive. Safe: failure here only means apps need a manual
# bringup — it cannot bootloop (non-critical oneshot).
exec >/data/local/tmp/asx_boot.log 2>&1
echo "[asx_boot] begin"
setenforce 0 2>/dev/null
# wait for system server (foundation) to be up, max ~90s
i=0
while [ $i -lt 90 ]; do
  if pidof foundation >/dev/null 2>&1; then break; fi
  sleep 1; i=$((i+1))
done
sleep 5
pkill -9 appspawn-x 2>/dev/null
sleep 1
mkdir -p /dev/memcg/perf_sensitive 2>/dev/null
rm -f /data/local/tmp/asx_run.out /data/local/tmp/asx_run.err
export APPSPAWNX_NO_JIT=1
export LD_PRELOAD=/system/android/lib/libsetgidhook.so:/system/android/lib/libw14supp.so:/system/android/lib/libdnshook.so:/system/android/lib/libnetlog.so:/system/android/lib/libjdnshook.so:/system/android/lib/libv4force.so
setsid /system/bin/appspawn-x --socket-name AppSpawnX >/data/local/tmp/asx_run.out 2>/data/local/tmp/asx_run.err &
# wait for the socket, then relabel so AMS can route
i=0
while [ $i -lt 40 ]; do
  if [ -e /dev/unix/socket/AppSpawnX ]; then break; fi
  sleep 1; i=$((i+1))
done
chmod 0666 /dev/unix/socket/AppSpawnX 2>/dev/null
chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX 2>/dev/null
echo "[asx_boot] appspawn-x pid=$(pidof appspawn-x)"
# keep granting INTERNET to noice (uid 13731); netsys may reset the eBPF map
( i=0; while [ $i -lt 150 ]; do /system/bin/bpfgrant 13731 oh_sock_permission_map >/dev/null 2>&1; sleep 2; i=$((i+1)); done ) &
echo "[asx_boot] end"
