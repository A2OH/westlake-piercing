#!/bin/bash
# wl_tap_test.sh <label> <x> <y> [expect] — tap a widget, report liveness, whether the view tree
# actually changed, and any new app-level exception.
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
SP=$WL_OUT
N=$1; X=$2; Y=$3
$WLROOT/wl_tree.sh > $SP/pre_$N.txt 2>&1
B=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac 'J_invokeStaticMain_main_threw' \$C" 2>/dev/null | tr -d '\r')
$HDC shell "echo '$X $Y' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep 5
A=$($HDC shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r')
if [ "$A" = "0" ]; then echo "  [$N] tap($X,$Y) -> DIED"; exit 0; fi
$WLROOT/wl_tree.sh > $SP/post_$N.txt 2>&1
D=$(diff -q $SP/pre_$N.txt $SP/post_$N.txt >/dev/null 2>&1 && echo same || echo CHANGED)
EXC=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a 'J_invokeStaticMain_main_threw' \$C | tail -n +$((B+1)) | tail -1" 2>/dev/null | tr -d '\r' | sed 's/.*main_threw: //' | cut -c1-105)
H=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a 'handled=' \$C | tail -2 | head -1" 2>/dev/null | tr -d '\r' | grep -oE 'handled=[01]')
T=$(grep -A1 'Toolbar id=action_bar' $SP/post_$N.txt | grep -o '"[^"]*"' | head -1)
echo "  [$N] tap($X,$Y) alive=$A $H tree=$D title=$T ${EXC:+exc=$EXC}"
