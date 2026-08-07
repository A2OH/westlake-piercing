#!/bin/bash
# On-device tap-latency probe. lat.sh polled from the host, and each hdc round trip costs
# 200-400 ms, which swamped exactly the millisecond-scale numbers being measured. This runs the
# whole stopwatch on the board (/proc/uptime, centisecond resolution) and only reports the result.
#
#   usage: lat2.sh <x> <y> <log-marker-that-appears-after-the-tap>
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first}"
X=$1; Y=$2; MARK=$3
C=$("$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1" 2>/dev/null | tr -d '\r')

"$HDC" shell "
C=$C
u(){ read a b < /proc/uptime; echo \$a; }
n0=\$(grep -ac 'in-process tap' \$C)
d0=\$(grep -ac 'TAP x=' \$C)
m0=\$(grep -ac '$MARK' \$C)
t0=\$(u)
echo '$X $Y' > /data/local/tmp/noice_tap
t1=''; t2=''; t3=''
i=0
while [ \$i -lt 4000 ]; do
  [ -z \"\$t1\" ] && [ \$(grep -ac 'in-process tap' \$C) -gt \$n0 ] && t1=\$(u)
  [ -z \"\$t2\" ] && [ \$(grep -ac 'TAP x=' \$C) -gt \$d0 ] && t2=\$(u)
  [ -z \"\$t3\" ] && [ \$(grep -ac '$MARK' \$C) -gt \$m0 ] && { t3=\$(u); break; }
  i=\$((i+1))
done
echo \"t0=\$t0 t1=\$t1 t2=\$t2 t3=\$t3\"
" 2>/dev/null | tr -d '\r' | while read line; do
  echo "$line" | grep -q '^t0=' || { echo "$line"; continue; }
  eval "$line"
  d(){ [ -n "$2" ] && echo "scale=2; $2 - $1" | bc || echo "n/a"; }
  echo "  poll   (file -> bridge saw it)     : $(d $t0 $t1) s"
  echo "  pace   (bridge -> TAP delivered)   : $(d $t1 $t2) s"
  echo "  app    (TAP -> '$MARK' in log)     : $(d $t2 $t3) s"
  echo "  TOTAL                              : $(d $t0 $t3) s"
done
