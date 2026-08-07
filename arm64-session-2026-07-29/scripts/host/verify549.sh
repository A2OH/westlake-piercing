#!/bin/bash
# §549 verification, fully gated. Every step checks LIVENESS first: this app dies intermittently on
# the play path from the pre-existing §436 invoke-interface SEGV, and a dead app produces exactly the
# same readings as a broken feature (0 streams, taps that "do nothing"). An earlier verdict here was
# invalid for precisely that reason.
#
# PASS  = after a confirmed resume tap the shim logs a SECOND "start" and a stream returns to RUNNING.
# FAIL  = confirmed resume tap, app still alive, but no second "start".
# VOID  = app died, or a tap never dispatched -> retry, conclude nothing.
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
KIT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${WL_OUT:?}"
CL(){ "$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1" 2>/dev/null | tr -d '\r'; }
alive(){ "$HDC" shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r '; }
run(){ "$HDC" shell "hidumper -s AudioDistributed -a '-p' 2>/dev/null | grep -cE 'Status: RUNNING'" 2>/dev/null | tr -d '\r '; }
C=$(CL)
shimlog(){ "$HDC" shell "grep -aE 'OH_ATShim: (start|pause)' $C" 2>/dev/null | tr -d '\r' | paste -sd'|'; }

[ "$(alive)" = "0" ] && { echo "VOID: app not alive at start"; exit 2; }
echo "  t0            alive=$(alive) RUNNING=$(run) shim[$(shimlog)]"

# --- PLAY (retry until a stream appears or app dies) ---
for i in 1 2 3; do
  "$HDC" shell "echo '324 385' > /data/local/tmp/noice_tap" >/dev/null 2>&1
  "$HDC" shell "sleep 50" >/dev/null 2>&1
  [ "$(alive)" = "0" ] && { echo "VOID: app died during PLAY"; exit 2; }
  [ "$(run)" != "0" ] && break
  echo "    (play $i: not started yet)"
done
[ "$(run)" = "0" ] && { echo "VOID: never started"; exit 2; }
echo "  after PLAY    alive=$(alive) RUNNING=$(run) shim[$(shimlog)]"

# --- toggle coords, read LIVE (the mini-player only exists while playing) ---
T=$(bash $KIT/scripts/host/wl_tree.sh 2>&1 | grep -m1 'id=play_toggle')
R=$(echo "$T" | grep -oE 'rect=\[[0-9]+,[0-9]+ [0-9]+x[0-9]+\]' | grep -oE '[0-9]+' | paste -sd' ')
TX=$(echo $R | cut -d' ' -f1); TY=$(echo $R | cut -d' ' -f2)
TW=$(echo $R | cut -d' ' -f3); TH=$(echo $R | cut -d' ' -f4)
[ -z "$TW" ] && { echo "VOID: no play_toggle in tree"; exit 2; }
TX=$((TX + TW/2)); TY=$((TY + TH/2))
echo "  toggle ($TX,$TY)"

# tapgate <x> <y> <label> <sleep> — retry until the bridge confirms dispatch
tapgate(){
  local O n i
  for i in 1 2 3 4 5; do
    O=$("$HDC" shell "wc -c < $C" 2>/dev/null | tr -d '\r ')
    "$HDC" shell "echo '$1 $2' > /data/local/tmp/noice_tap" >/dev/null 2>&1
    "$HDC" shell "sleep $4" >/dev/null 2>&1
    "$HDC" shell "tail -c +$((O+1)) $C > /data/local/tmp/tg.txt" >/dev/null 2>&1
    n=$("$HDC" shell "grep -ac 'in-process tap' /data/local/tmp/tg.txt" 2>/dev/null | tr -d '\r ')
    [ "$n" != "0" ] && return 0
    [ "$(alive)" = "0" ] && return 2
  done
  return 1
}

tapgate $TX $TY PAUSE 25;  rc=$?
[ $rc = 2 ] && { echo "VOID: app died at PAUSE"; exit 2; }
[ $rc = 1 ] && { echo "VOID: PAUSE tap never dispatched"; exit 2; }
echo "  after PAUSE   alive=$(alive) RUNNING=$(run) shim[$(shimlog)]"

tapgate $TX $TY RESUME 30; rc=$?
[ $rc = 2 ] && { echo "VOID: app died at RESUME"; exit 2; }
[ $rc = 1 ] && { echo "VOID: RESUME tap never dispatched"; exit 2; }
echo "  after RESUME  alive=$(alive) RUNNING=$(run) shim[$(shimlog)]"

STARTS=$("$HDC" shell "grep -ac 'OH_ATShim: start' $C" 2>/dev/null | tr -d '\r ')
echo "  ==> total shim 'start' events = $STARTS  (>=2 means the §549 latch fix worked)"
