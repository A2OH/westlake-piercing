#!/bin/bash
# wl_page.sh <name> <x> <y> — tap a widget, then capture the page (tree + screenshot + liveness).
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
SP=$WL_OUT
N=$1; X=$2; Y=$3
if [ -n "$X" ]; then
  $HDC shell "echo '$X $Y' > /data/local/tmp/noice_tap" >/dev/null 2>&1
  sleep 4
fi
ALIVE=$($HDC shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r')
RES=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac 'WESTLAKE-412. Runtime.nativeExit' \$C" 2>/dev/null | tr -d '\r')
$WLROOT/wl_tree.sh > $SP/tree_$N.txt 2>&1
$WLROOT/wl_shot.sh $N > $SP/shot_$N.txt 2>&1
echo "[$N] alive=$ALIVE resumes=$RES shot=$(head -1 $SP/shot_$N.txt) widgets=$(grep -c 'c=1' $SP/tree_$N.txt)"
grep -E 'c=1 ' $SP/tree_$N.txt | sed 's/^ *//' | head -40
