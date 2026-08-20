#!/bin/bash
# JIT diagnostic run: boot the app INTERPRETED (so startup is healthy and out of the picture),
# then flip the JIT on after a delay and capture ART's own -verbose:jit narration around the death.
#
# Why deferred: enabling at fork kills initChild, which mixes startup complexity into the failure.
# With the delay the app is proven healthy first, so anything that happens after the
# "[JIT-560] delay elapsed" marker is attributable to the JIT alone.
. $WLROOT/westlake-arm64/arm64-session-2026-07-29/recipes/env.sh 2>/dev/null
: "${HDC:?source recipes/env.sh first}"
DELAY_MS="${1:-40000}"

for i in 1 2 3 4 5; do
  "$HDC" shell "pkill -9 -f appspawn-x" >/dev/null 2>&1; sleep 1
  N=$("$HDC" shell "ps -A -o ARGS | grep -c appspawn-x" | tr -d '\r'); [ "$N" -le 1 ] && break
done

"$HDC" shell "cd /data/local/tmp/asx && APPSPAWNX_FORCE_JIT=1 APPSPAWNX_JIT_VERBOSE=1 APPSPAWNX_JIT_DELAY_MS=$DELAY_MS sh ./testnoice.sh" >/dev/null 2>&1

C=$("$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" | tr -d '\r')
echo "log=$C  delay=${DELAY_MS}ms"
echo "alive after startup: $("$HDC" shell 'ps -A -o ARGS | grep -c appspawn-x' | tr -d '\r') appspawn-x procs (2 = healthy)"

# Record the byte offset of the JIT-enable marker, then watch only what comes after it.
echo "--- waiting for the JIT to go live ---"
for i in $(seq 1 40); do
  sleep 3
  HIT=$("$HDC" shell "grep -ac 'delay elapsed' $C" | tr -d '\r')
  [ "$HIT" -ge 1 ] && { echo "JIT enabled at t≈$((i*3))s"; break; }
done
OFF=$("$HDC" shell "grep -ab 'delay elapsed' $C | head -1" | tr -d '\r' | cut -d: -f1)
[ -z "$OFF" ] && { echo "!! JIT never enabled"; exit 1; }

sleep 15
echo "alive after JIT: $("$HDC" shell 'ps -A -o ARGS | grep -c appspawn-x' | tr -d '\r') appspawn-x procs"
echo "=============== ART's own narration from the JIT-enable point ==============="
"$HDC" shell "tail -c +$((OFF+1)) $C" | tr -d '\r' | \
  grep -vE 'getDisplayInfo|VSYNC tick|SQLiteTime|G214|OH_MCShim|OH_ATShim' | head -70
