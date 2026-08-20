#!/bin/bash
# §570 test: mark the Kotlin continuation classes non-compilable, THEN let the JIT go live.
# Ordering matters — the exclusion must land before compilation starts, so the JIT delay is set
# long enough to apply it while the app is still interpreted.
. $WLROOT/westlake-arm64/arm64-session-2026-07-29/recipes/env.sh 2>/dev/null
: "${HDC:?}"
DELAY_MS="${1:-90000}"
shift
CLASSES="${@:-kotlin.coroutines.jvm.internal.ContinuationImpl kotlin.coroutines.jvm.internal.BaseContinuationImpl}"

for i in 1 2 3 4 5; do
  "$HDC" shell "pkill -9 -f appspawn-x" >/dev/null 2>&1; sleep 1
  N=$("$HDC" shell "ps -A -o ARGS | grep -c appspawn-x" | tr -d '\r'); [ "$N" -le 1 ] && break
done
"$HDC" shell "cd /data/local/tmp/asx && APPSPAWNX_FORCE_JIT=1 APPSPAWNX_JIT_VERBOSE=1 APPSPAWNX_JIT_DELAY_MS=$DELAY_MS sh ./testnoice.sh" >/dev/null 2>&1
C=$("$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" | tr -d '\r')
echo "log=$C  jit_delay=${DELAY_MS}ms"

# wait for the input side-channel (needed to deliver the J verb)
for i in $(seq 1 25); do
  sleep 2
  [ "$("$HDC" shell "grep -ac 'side-channels started' $C" | tr -d '\r')" -ge 1 ] && { echo "channel up ~$((i*2))s"; break; }
done
sleep 6

echo "--- applying exclusions ---"
for c in $CLASSES; do
  "$HDC" shell "echo 'J $c' > /data/local/tmp/noice_tap"
  sleep 4
done
"$HDC" shell "grep -a 'JIT-570' $C" | tr -d '\r' | sed 's/.*\[JIT-570\]/  [JIT-570]/'

echo "--- waiting for the JIT ---"
for i in $(seq 1 60); do
  sleep 3
  [ "$("$HDC" shell "grep -ac 'delay elapsed' $C" | tr -d '\r')" -ge 1 ] && { echo "JIT live ~$((i*3))s after exclusions"; break; }
done
sleep 30

echo "=== RESULT ==="
echo "  alive     = $("$HDC" shell 'ps -A -o ARGS | grep -c appspawn-x' | tr -d '\r')  (2 = SURVIVED)"
echo "  compiled  = $("$HDC" shell "grep -ac 'Compiling method' $C" | tr -d '\r')"
echo "  SOE       = $("$HDC" shell "grep -ac StackOverflowError $C" | tr -d '\r')"
echo "  excluded classes still compiled? = $("$HDC" shell "grep -a 'Compiling method' $C | grep -c ContinuationImpl" | tr -d '\r')"
