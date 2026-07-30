#!/bin/bash
# wl_tree.sh [rootIdx] — dump the live view hierarchy of a window via the bridge side-channel.
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
R=${1:-}
BEFORE=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac '^OH_InputBridge: VT ' \$C" 2>/dev/null | tr -d '\r')
$HDC shell "echo 'v$R' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep 4
$HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a '^OH_InputBridge: VT ' \$C | tail -n +$((BEFORE+1))" 2>/dev/null | tr -d '\r' | sed 's/^OH_InputBridge: VT //'
