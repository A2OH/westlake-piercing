#!/bin/bash
# Tab-switch benchmark. Reports CPU ticks AND wall time per interaction.
#
# Why it is built this way — the previous attempt produced garbage:
#  * It measured launches that had NO input channel, so no tap was ever delivered and the "cost of a
#    tab switch" was really just 8 s of background work. => REFUSE to run unless
#    'side-channels started' is present, and COUNT delivered taps; a run with missing taps is void.
#  * It used a fixed sleep, which dominated the number. => settle ADAPTIVELY: poll the process's own
#    CPU counter and call the interaction finished once it goes quiet, then report what it actually
#    burned. That is independent of any arbitrary wait.
# Everything runs on-device: an hdc round trip is 200-400 ms and would swamp the measurement.
#
#   usage: bench.sh <label> [taps]
. $WLROOT/westlake-arm64/arm64-session-2026-07-29/recipes/env.sh 2>/dev/null
: "${HDC:?}"
LABEL="${1:-run}"; TAPS="${2:-6}"

C=$("$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" | tr -d '\r')
CH=$("$HDC" shell "grep -ac 'side-channels started' $C" | tr -d '\r')
[ "${CH:-0}" -lt 1 ] && { echo "VOID: no input side-channel in $C — nothing would be delivered"; exit 2; }
P=$("$HDC" shell "ps -A -o PID,PPID,ARGS | grep appspawn-x | grep -v grep" | tr -d '\r' | tail -1 | sed 's/^ *//' | cut -d' ' -f1)
echo "=== $LABEL === child=$P log=$C"

"$HDC" shell "
P=$P; C=$C
ticks(){ s=\$(cat /proc/\$P/stat); echo \$(( \$(echo \$s|cut -d' ' -f14) + \$(echo \$s|cut -d' ' -f15) )); }
now(){ read a b < /proc/uptime; echo \$a; }
d0=\$(grep -ac 'TAP x=' \$C)
i=0
while [ \$i -lt $TAPS ]; do
  i=\$((i+1))
  if [ \$((i % 2)) -eq 1 ]; then X='360 1830'; else X='120 1830'; fi
  # let the process go quiet before starting the clock
  q=0
  while [ \$q -lt 40 ]; do
    a=\$(ticks); sleep 0.5; b=\$(ticks)
    [ \$((b-a)) -lt 6 ] && break
    q=\$((q+1))
  done
  c0=\$(ticks); t0=\$(now)
  echo \"\$X\" > /data/local/tmp/noice_tap
  # adaptive settle: finished once CPU use over a 0.5 s window drops back to idle
  busy=0; k=0
  while [ \$k -lt 120 ]; do
    a=\$(ticks); sleep 0.5; b=\$(ticks)
    k=\$((k+1))
    if [ \$((b-a)) -ge 6 ]; then busy=1; else [ \$busy -eq 1 ] && break; fi
  done
  c1=\$(ticks); t1=\$(now)
  echo \"  tap\$i  cpu_ticks=\$((c1-c0))  wall=\$(echo \"\$t1 - \$t0\" | bc)s\"
done
d1=\$(grep -ac 'TAP x=' \$C)
echo \"  taps_delivered=\$((d1-d0)) of $TAPS\"
" 2>&1 | tr -d '\r'
