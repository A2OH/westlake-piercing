#!/bin/bash
# Restart until the §436 SEGV lottery fires, then dump the WHOLE crash block plus the last lines of
# normal execution before it (which is where the interface/method being invoked should appear).
# wl_restart.sh deletes old child stderr files, so the dump has to be captured inside the same run.
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
KIT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${WL_OUT:?}"
CL(){ "$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1" 2>/dev/null | tr -d '\r'; }

for round in 1 2 3 4 5 6 7 8; do
  R=$(bash $KIT/scripts/host/wl_restart.sh 2>&1 | tr -d '\r' | tail -1)
  C=$(CL)
  N=$("$HDC" shell "grep -ac WESTLAKE-CHILDSEGV $C" 2>/dev/null | tr -d '\r ')
  A=$("$HDC" shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r ')
  echo "round $round: $R  segv=$N alive=$A"
  if [ "$N" != "0" ]; then
    echo "  CAUGHT — log $C"
    "$HDC" shell "grep -a -B40 'WESTLAKE-CHILDSEGV' $C | head -70" 2>/dev/null | tr -d '\r' > $OUT/segv_before.txt
    "$HDC" shell "grep -a 'WESTLAKE-CHILDSEGV' $C | head -40" 2>/dev/null | tr -d '\r' > $OUT/segv_frames.txt
    echo "$C" > $OUT/segv_log_path.txt
    exit 0
  fi
  # alive but no segv: try to provoke it on the play path
  if [ "$A" != "0" ]; then
    "$HDC" shell "sleep 90" >/dev/null 2>&1
    "$HDC" shell "echo '324 385' > /data/local/tmp/noice_tap" >/dev/null 2>&1
    "$HDC" shell "sleep 55" >/dev/null 2>&1
    N=$("$HDC" shell "grep -ac WESTLAKE-CHILDSEGV $C" 2>/dev/null | tr -d '\r ')
    A=$("$HDC" shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r ')
    echo "         after play: segv=$N alive=$A"
    if [ "$N" != "0" ]; then
      echo "  CAUGHT ON PLAY PATH — log $C"
      "$HDC" shell "grep -a -B40 'WESTLAKE-CHILDSEGV' $C | head -70" 2>/dev/null | tr -d '\r' > $OUT/segv_before.txt
      "$HDC" shell "grep -a 'WESTLAKE-CHILDSEGV' $C | head -40" 2>/dev/null | tr -d '\r' > $OUT/segv_frames.txt
      echo "$C" > $OUT/segv_log_path.txt
      exit 0
    fi
  fi
done
echo "no SEGV caught in 8 rounds"
exit 1
