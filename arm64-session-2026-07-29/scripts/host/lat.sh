#!/bin/bash
# Decompose tap latency: harness sleeps vs. app work.
#   t_poll     write the tap file -> bridge logs "in-process tap"      (the 300 ms poller)
#   t_deliver  "in-process tap"   -> "TAP ... delivered"               (DOWN/UP pacing: 150+100 ms)
#   t_visible  "TAP delivered"    -> the view tree actually changes    (real app work)
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
KIT="$(cd "$(dirname "$0")/../.." && pwd)"
CL(){ "$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1" 2>/dev/null | tr -d '\r'; }
C=$(CL)
X=$1; Y=$2; MARK=$3     # MARK = a string that appears in the tree ONLY after the tap lands

now(){ date +%s.%N; }
# count marker lines before
b_tap=$("$HDC" shell "grep -ac 'in-process tap' $C" 2>/dev/null | tr -d '\r ')
b_del=$("$HDC" shell "grep -ac 'TAP x=' $C" 2>/dev/null | tr -d '\r ')

T0=$(now)
"$HDC" shell "echo '$X $Y' > /data/local/tmp/noice_tap" >/dev/null 2>&1

# wait for the bridge to notice (poller)
T1=""
for i in $(seq 1 200); do
  n=$("$HDC" shell "grep -ac 'in-process tap' $C" 2>/dev/null | tr -d '\r ')
  [ "$n" -gt "$b_tap" ] && { T1=$(now); break; }
done
# wait for delivery (DOWN/UP pacing done)
T2=""
for i in $(seq 1 200); do
  n=$("$HDC" shell "grep -ac 'TAP x=' $C" 2>/dev/null | tr -d '\r ')
  [ "$n" -gt "$b_del" ] && { T2=$(now); break; }
done
# wait for the UI to actually reflect it
T3=""
for i in $(seq 1 120); do
  if bash $KIT/scripts/host/wl_tree.sh 2>/dev/null | grep -q "$MARK"; then T3=$(now); break; fi
done

d(){ [ -n "$2" ] && echo "$2 - $1" | bc || echo "n/a"; }
echo "  t_poll    (file -> bridge saw it) : $(d $T0 $T1) s"
echo "  t_deliver (bridge -> TAP done)    : $(d $T1 $T2) s"
echo "  t_visible (TAP -> UI shows '$MARK'): $(d $T2 $T3) s"
echo "  TOTAL                              : $(d $T0 $T3) s"
