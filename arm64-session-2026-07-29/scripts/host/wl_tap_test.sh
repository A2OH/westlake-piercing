#!/bin/bash
# wl_tap_test.sh <label> <x> <y> [expect] — tap a widget, report liveness, whether the view tree
# actually changed, and any new app-level exception.
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
SP=/tmp/claude-1000/-home-dspfac-openharmony/9adf5c05-1946-4e31-a77a-b6e8688c50b6/scratchpad
N=$1; X=$2; Y=$3
/home/dspfac/wl_tree.sh > $SP/pre_$N.txt 2>&1
B=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac 'J_invokeStaticMain_main_threw' \$C" 2>/dev/null | tr -d '\r')
$HDC shell "echo '$X $Y' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep 5
A=$($HDC shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r')
if [ "$A" = "0" ]; then echo "  [$N] tap($X,$Y) -> DIED"; exit 0; fi
/home/dspfac/wl_tree.sh > $SP/post_$N.txt 2>&1
D=$(diff -q $SP/pre_$N.txt $SP/post_$N.txt >/dev/null 2>&1 && echo same || echo CHANGED)
EXC=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a 'J_invokeStaticMain_main_threw' \$C | tail -n +$((B+1)) | tail -1" 2>/dev/null | tr -d '\r' | sed 's/.*main_threw: //' | cut -c1-105)
H=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a 'handled=' \$C | tail -2 | head -1" 2>/dev/null | tr -d '\r' | grep -oE 'handled=[01]')
T=$(grep -A1 'Toolbar id=action_bar' $SP/post_$N.txt | grep -o '"[^"]*"' | head -1)
echo "  [$N] tap($X,$Y) alive=$A $H tree=$D title=$T ${EXC:+exc=$EXC}"
