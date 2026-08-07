#!/bin/bash
# Measure the §436 launch-SEGV rate over N rounds. Reports, per round: whether the app came up,
# whether it survived a play tap, and how many WESTLAKE-CHILDSEGV entries the child logged.
# Usage: segv_rate.sh <rounds> <label>
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
KIT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${WL_OUT:?}"
N=${1:-6}; LBL=${2:-run}
CL(){ "$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1" 2>/dev/null | tr -d '\r'; }
up=0; segv=0; played=0
for r in $(seq 1 $N); do
  R=$(bash $KIT/scripts/host/wl_restart.sh 2>&1 | tr -d '\r' | tail -1)
  C=$(CL)
  S=$("$HDC" shell "grep -ac WESTLAKE-CHILDSEGV $C" 2>/dev/null | tr -d '\r ')
  A=$("$HDC" shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r ')
  P="-"
  case "$R" in UP:*) up=$((up+1))
      "$HDC" shell "sleep 85" >/dev/null 2>&1
      "$HDC" shell "echo '324 385' > /data/local/tmp/noice_tap" >/dev/null 2>&1
      "$HDC" shell "sleep 55" >/dev/null 2>&1
      A2=$("$HDC" shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r ')
      RUN=$("$HDC" shell "hidumper -s AudioDistributed -a '-p' 2>/dev/null | grep -cE 'Status: RUNNING'" 2>/dev/null | tr -d '\r ')
      S=$("$HDC" shell "grep -ac WESTLAKE-CHILDSEGV $C" 2>/dev/null | tr -d '\r ')
      [ "$A2" != "0" ] && { P="alive"; played=$((played+1)); } || P="DIED"
      P="$P/RUNNING=$RUN" ;;
  esac
  [ "$S" != "0" ] && segv=$((segv+1))
  echo "  [$LBL] round $r: ${R%% *} launchAlive=$A segv=$S play=$P"
done
echo "  [$LBL] SUMMARY: launched_ok=$up/$N  rounds_with_segv=$segv/$N  survived_play=$played"
