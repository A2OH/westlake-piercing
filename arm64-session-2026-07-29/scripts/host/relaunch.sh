#!/bin/bash
# Relaunch noice and REPORT WHETHER THE LAUNCH IS USABLE.
#
# This board has a launch lottery. A launch can render perfectly yet be unusable: if SceneSession
# adoption fails (`CreateWindow ret!=0` / `V7 err!=0`), oh_window_manager_client never reaches the
# code that registers the session AND starts the tap/text side-channels — so nothing consumes
# /data/local/tmp/noice_tap and every subsequent "tap did nothing" is a launch artifact, not a bug.
# Always confirm `side-channels started` before believing any input result.
#
#   usage: relaunch.sh [max_attempts]
. $WLROOT/westlake-arm64/arm64-session-2026-07-29/recipes/env.sh 2>/dev/null
: "${HDC:?source recipes/env.sh first}"
MAX="${1:-4}"

for attempt in $(seq 1 "$MAX"); do
  echo "=== launch attempt $attempt/$MAX ==="
  for i in 1 2 3 4 5; do
    PIDS=$("$HDC" shell "ps -A -o PID,COMM | grep -e appspawn-x -e ashu" | tr -d '\r' | sed 's/^ *//' | cut -d' ' -f1)
    [ -z "$PIDS" ] && break
    for pid in $PIDS; do "$HDC" shell "kill -9 $pid" >/dev/null 2>&1; done
    sleep 1
  done
  "$HDC" shell "rm -f /data/local/tmp/noice_tap" >/dev/null 2>&1
  "$HDC" shell "cd /data/local/tmp/asx && sh ./testnoice.sh" >/dev/null 2>&1

  C=$("$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" | tr -d '\r')
  CH=$("$HDC" shell "grep -ac 'side-channels started' $C" | tr -d '\r')
  CW=$("$HDC" shell "grep -a 'CreateWindow ret=' $C | head -1" | tr -d '\r')
  PROC=$("$HDC" shell "ps -A -o PID,COMM | grep -c appspawn-x" | tr -d '\r')
  echo "  log=$C"
  echo "  procs=$PROC  side-channels=$CH  $CW"
  if [ "$CH" -ge 1 ]; then
    echo "  ==> USABLE LAUNCH"
    exit 0
  fi
  echo "  ==> unusable (no input channel), retrying"
done
echo "!! no usable launch after $MAX attempts"
exit 1
