#!/bin/bash
# wl_page.sh <name> <x> <y> — tap a widget, then capture the page (tree + screenshot + liveness).
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
SP=/tmp/claude-1000/-home-dspfac-openharmony/9adf5c05-1946-4e31-a77a-b6e8688c50b6/scratchpad
N=$1; X=$2; Y=$3
if [ -n "$X" ]; then
  $HDC shell "echo '$X $Y' > /data/local/tmp/noice_tap" >/dev/null 2>&1
  sleep 4
fi
ALIVE=$($HDC shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r')
RES=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac 'WESTLAKE-412. Runtime.nativeExit' \$C" 2>/dev/null | tr -d '\r')
/home/dspfac/wl_tree.sh > $SP/tree_$N.txt 2>&1
/home/dspfac/wl_shot.sh $N > $SP/shot_$N.txt 2>&1
echo "[$N] alive=$ALIVE resumes=$RES shot=$(head -1 $SP/shot_$N.txt) widgets=$(grep -c 'c=1' $SP/tree_$N.txt)"
grep -E 'c=1 ' $SP/tree_$N.txt | sed 's/^ *//' | head -40
