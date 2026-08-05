#!/bin/bash
# audio-run.sh — drive noice to audio, asserting the app is ALIVE at every step.
#
# ★Why the liveness assert: this app dies intermittently from the §436 invoke-interface SEGV.
# A dead process makes every downstream measurement look like a hang — static SQL counts, a DAO
# query that "never resumes", unbalanced BEGIN/COMMIT. All of those are artifacts of death, not
# evidence about Room. Never report a stall without proving the process was alive when measured.
set -u
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
H=${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}
KIT="$(cd "$(dirname "$0")/../.." && pwd)"

alive() { $H shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r' | tail -1; }
require_alive() {
  local a; a=$(alive)
  [ "$a" = "1" ] || { echo "*** APP DEAD at step: $1 (count=$a) — measurements below would be junk"; return 1; }
  echo "    [alive ok] $1"; return 0
}

echo "=== restart ==="
bash $KIT/scripts/host/wl_restart.sh 2>&1 | tail -2
PID=$($H shell "ps -A | grep '[a]shu'" 2>/dev/null | tr -d '\r' | awk '{print $1}' | head -1)
C=/data/service/el1/public/appspawnx/adapter_child_$PID.stderr
echo "pid=$PID"
require_alive "after restart" || exit 1

echo "=== wait for library sync ==="
prev=-1
for i in $(seq 1 24); do
  require_alive "sync poll $i" || exit 1
  n=$(timeout 120 $H shell "grep -ac 'txn: before dao' $C" 2>/dev/null | tr -d '\r'); n=${n:-0}
  [ "$n" = "$prev" ] && [ "$n" != "0" ] && { echo "  sync quiet at $n"; break; }
  prev=$n; sleep 15
done

echo "=== tap play ==="
$H shell "echo '323 384' > /data/local/tmp/noice_tap"
for i in 1 2 3 4 5 6; do
  sleep 20
  require_alive "post-play +$((i*20))s" || break
  seg=$(timeout 120 $H shell "grep -ac 'RESUMED' $C" 2>/dev/null | tr -d '\r')
  cod=$(timeout 120 $H shell "grep -ac 'native_setup' $C" 2>/dev/null | tr -d '\r')
  exo=$(timeout 90 $H shell "for t in /proc/$PID/task/*; do cat \$t/comm 2>/dev/null; done" 2>/dev/null | tr -d '\r' | grep -ci exoplayer)
  echo "      resumed=$seg codec=$cod exoThreads=$exo"
  [ "${cod:-0}" != "0" ] && { echo "  *** CODEC CREATED ***"; break; }
done

echo "=== final state ==="
require_alive "final"
echo "--- play-path trace ---"
timeout 150 $H shell "grep -a 'WESTLAKE-480' $C | grep -av 'txn:' | tail -10" 2>/dev/null | tr -d '\r'
echo "--- SEGV? ---"; timeout 120 $H shell "grep -ac 'CHILDSEGV. #0' $C" 2>/dev/null | tr -d '\r'
echo "--- codec/audio ---"
timeout 120 $H shell "grep -a OH_MCShim $C | tail -6" 2>/dev/null | tr -d '\r'
echo "PID=$PID"
