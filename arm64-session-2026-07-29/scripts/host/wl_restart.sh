#!/bin/bash
# wl_restart.sh — guarantee a SINGLE noice child, then relaunch and wait for a frame.
# Multiple stale children all poll /data/local/tmp/noice_tap, so any extra one silently steals
# taps and corrupts measurements — kill and VERIFY before launching.
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
LOG=/data/local/tmp/run_$$.log
$HDC shell 'pkill -9 -f run406.sh 2>/dev/null; pkill -9 appspawn-x 2>/dev/null; aa force-stop com.github.ashutoshgngwr.noice 2>/dev/null' >/dev/null 2>&1
for i in 1 2 3 4 5 6 7 8; do
  N=$($HDC shell 'ps -A | grep "[a]shu" > /data/local/tmp/pl.txt; while read L; do set -- $L; kill -9 $1 2>/dev/null; done < /data/local/tmp/pl.txt; sleep 1; ps -A | grep -c "[a]shu"' 2>/dev/null | tr -d '\r' | tail -1)
  [ "$N" = "0" ] && break
done
$HDC shell "rm -f /data/service/el1/public/appspawnx/adapter_child_*.stderr /data/local/tmp/run_*.log" >/dev/null 2>&1
# ★setsid, not just nohup: run406.sh launched from an hdc shell stays in that
# session's process group, so a dropped hdc connection (WSL vsock errors are
# common here) kills appspawn-x and the app with it -- the log just stops
# mid-render with no crash. setsid detaches it so it survives.
$HDC shell "setsid nohup sh /data/local/tmp/run406.sh > $LOG 2>&1 &" >/dev/null 2>&1
echo "$LOG" > $WL_OUT/runlog
for i in $(seq 1 60); do
  L=$($HDC shell "tail -1 $LOG" 2>/dev/null | tr -d '\r')
  case "$L" in *swaps=[1-9]*) 
    C=$($HDC shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r')
    echo "UP: $L  children=$C"; exit 0;; esac
  sleep 3
done
echo "TIMEOUT: $($HDC shell "tail -1 $LOG" | tr -d '\r')"; exit 1
