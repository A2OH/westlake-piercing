#!/bin/bash
# wl_cmd.sh <cmd...> — write a command to the noice input side-channel and echo new bridge log lines.
#   wl_cmd.sh w                 list windows
#   wl_cmd.sh r 2               aim input at root #2 (-1 = auto)
#   wl_cmd.sh tap X Y
#   wl_cmd.sh drag X1 Y1 X2 Y2
#   wl_cmd.sh back
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
case "$1" in
  w)    C="w" ;;
  r)    C="r$2" ;;
  back) C="back" ;;
  tap)  C="$2 $3" ;;
  drag) C="$2 $3 $4 $5" ;;
  *)    C="$*" ;;
esac
BEFORE=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac OH_InputBridge \$C" 2>/dev/null | tr -d '\r')
$HDC shell "echo '$C' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep ${WL_WAIT:-3}
$HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a 'OH_InputBridge' \$C | tail -n +$((BEFORE+1))" 2>/dev/null | tr -d '\r'
