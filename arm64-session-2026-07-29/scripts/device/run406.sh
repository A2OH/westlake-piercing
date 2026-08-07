#!/system/bin/sh
# run406.sh — launch noice (host ability + appspawn-x child) and KEEP IT AWAKE for an hour
# so the UI can be driven widget-by-widget through /data/local/tmp/noice_tap.
awake() {
  power-shell timeout -o 3600000 >/dev/null 2>&1
  power-shell wakeup >/dev/null 2>&1
}
setenforce 0 2>/dev/null
awake; sleep 1; awake
PARENT=""
j=0
while [ $j -lt 3 ]; do
  aa start -b com.github.ashutoshgngwr.noice -a EntryAbility >/dev/null 2>&1
  k=0
  while [ $k -lt 15 ]; do
    LINE=$(hidumper -s WindowManagerService -a '-a' 2>/dev/null | grep -i 'noice0' | head -1)
    set -- $LINE
    PARENT=$4
    [ -n "$PARENT" ] && break
    sleep 1; k=$((k+1))
  done
  [ -n "$PARENT" ] && break
  aa force-stop com.github.ashutoshgngwr.noice >/dev/null 2>&1; sleep 3; j=$((j+1))
done
echo "PARENT=$PARENT"
[ -z "$PARENT" ] && { echo abort; exit 1; }
cd /data/local/tmp/asx || exit 1
# ⚠️`pkill -9 appspawn-x` SILENTLY MATCHES NOTHING on this board and left one 1%-CPU daemon behind
# on every single restart — 34 of them had accumulated (~34% of a core, plus they hold the binary
# open so `hdc file send` fails with "text file is busy"). This board's `ps -o COMM` prints the
# basename of /proc/PID/exe, but pkill matches /proc/PID/comm, and those two are NOT the same here.
# Kill by explicit PID instead, and verify.
ps -A -o PID,COMM | grep appspawn-x > /data/local/tmp/axp.txt 2>/dev/null
while read P C; do kill -9 $P 2>/dev/null; done < /data/local/tmp/axp.txt
sleep 2
ps -A -o PID,COMM | grep appspawn-x > /data/local/tmp/axp.txt 2>/dev/null
while read P C; do kill -9 $P 2>/dev/null; done < /data/local/tmp/axp.txt
echo "appspawn-x left alive: $(ps -A -o PID,COMM | grep -c appspawn-x)"
rm -f asx.err /data/service/el1/public/appspawnx/adapter_child_*.stderr
# §544: opt-in Java-level probe. libart runs ~100 strstr predicates per interpreted invoke and we
# own strstr (§532), so sampling its HAYSTACK — the method name / class descriptor of whatever is
# executing — names the hot loop that native profiling cannot see. Toggled by a marker file so a
# run can switch it on without re-pushing this script. Histogram lands in
# /data/local/tmp/wl_strstr_hist.txt, rewritten every 200k samples (each dump is a fresh window).
HIST=""
[ -f /data/local/tmp/wl_hist_on ] && { HIST="WL_STRSTR_HIST=1"; echo "STRSTR HISTOGRAM: ON"; }
E="$HIST WL_FOCUSABLE=1 WL_CPU_FILL=1 WL_SELFDRAW_WINDOW=1 WL_NO_CONTENT_NODE=1 WL_PIN_NODES=1 WL_WINDOW_ISWINDOW=1 WL_DRAWLOG=1 WL_SUB_WINDOW=1 WL_PARENT_ID=$PARENT ASX_CHILD_RECLAIMER=1 APPSPAWNX_FAST_DEV=1 ASX_NO_DAEMON_INIT=1 ASX_NO_START_DAEMONS=1 ANDROID_ROOT=/system ICU_DATA=/data/local/tmp/asx LD_LIBRARY_PATH=/data/local/tmp/asx:/system/lib64:/system/lib64/platformsdk:/system/lib64/chipset-sdk:/system/lib64/chipset-sdk-sp:/system/lib64/ndk:/system/lib64/module/data:/system/lib64/module"
env $E setsid ./appspawn-x >asx.err 2>&1 &
i=0
while [ $i -lt 300 ]; do grep -q "Ready to accept" asx.err 2>/dev/null && break; sleep 0.1; i=$((i+1)); done
./spawn_client >/dev/null 2>&1

# WESTLAKE: forward PHYSICAL touches into the in-process tap channel.
# Real events go to OHOS's MMI input manager, which never routes them to the child's windows
# (the WMS/MMI focus wall), so without this the panel looks completely dead to a human even though
# the app is fine -- every interaction otherwise has to be injected via /data/local/tmp/noice_tap.
# This board exposes TWO touch nodes (see /proc/bus/input/devices):
#   event2 = gsl680_tp        event5 = VSoC touchscreen
# Start one forwarder per node; whichever the panel actually drives will produce the taps.
pkill -f touchfwd 2>/dev/null
if [ -x /data/local/tmp/touchfwd ]; then
  for ev in /dev/input/event5 /dev/input/event2; do
    [ -e "$ev" ] && WL_TOUCH_STREAM=1 nohup /data/local/tmp/touchfwd "$ev" >>/data/local/tmp/touchfwd.log 2>&1 &
  done
  sleep 1
  echo "touchfwd started: $(ps -A | grep -c touchfwd) proc(s)"
else
  echo "touchfwd MISSING at /data/local/tmp/touchfwd - physical touch will not work"
fi
# keep the panel awake for an hour; report only state changes
i=0
while [ $i -lt 5400 ]; do   # 3h (was 1800 = 1h; the app vanishing mid-session was just this loop ending)
  awake
  C=$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1)
  W=$(grep -ac WESTLAKE-HWSWAP $C 2>/dev/null)
  AL=$(ps -A | grep -c '[a]shu')
  echo "t=$((i*2))s swaps=$W alive=$AL"
  sleep 2; i=$((i+1))
done
