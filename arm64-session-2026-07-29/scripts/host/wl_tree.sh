#!/bin/bash
# wl_tree.sh [rootIdx] — dump the live view hierarchy of a window via the bridge side-channel.
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
R=${1:-}
BEFORE=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac '^OH_InputBridge: VT ' \$C" 2>/dev/null | tr -d '\r')
$HDC shell "echo 'v$R' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep 4
$HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a '^OH_InputBridge: VT ' \$C | tail -n +$((BEFORE+1))" 2>/dev/null | tr -d '\r' | sed 's/^OH_InputBridge: VT //'
