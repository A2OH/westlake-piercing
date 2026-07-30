#!/bin/bash
# wl_page_widgets.sh <tabX> <label> — tap a tab, dump the widget tree, list interactive widgets.
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
TAB=$1; LABEL=$2
L=$($HDC shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" 2>/dev/null | tr -d '\r')
N=$($HDC shell "wc -l < $L" 2>/dev/null | tr -d '\r ' )
$HDC shell "echo '$TAB 1829' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep 7
$HDC shell "echo 'v' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep 7
echo "===== $LABEL ====="
$HDC shell "tail -n +$N $L | grep -a 'OH_InputBridge: VT' | grep -a 'c=1'" 2>/dev/null | tr -d '\r' \
  | sed -E 's/.*VT +//' | grep -vE 'navigation_bar_item' | head -40
A=$($HDC shell "ps -A | grep -ac ashu" 2>/dev/null | tr -d '\r ')
echo "-- alive=$A --"
