#!/bin/bash
# wl_ui.sh — drive the noice UI on the arm64 board.
#   wl_ui.sh shot <name>      screenshot -> scratchpad/shots/<name>.jpeg (prints size)
#   wl_ui.sh tap <x> <y>      inject a tap through the bridge side-channel
#   wl_ui.sh awake            keep the panel on
#   wl_ui.sh alive            child/ability/swap status
HDC="/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe"
SHOTS=/tmp/claude-1000/-home-dspfac-openharmony/9adf5c05-1946-4e31-a77a-b6e8688c50b6/scratchpad/shots
WIN=/mnt/c/Users/dspfa/Dev/wlstage
case "$1" in
  awake) $HDC shell "power-shell timeout -o 3600000 >/dev/null 2>&1; power-shell wakeup >/dev/null 2>&1" >/dev/null 2>&1 ;;
  tap)   $HDC shell "echo '$2 $3' > /data/local/tmp/noice_tap" >/dev/null 2>&1; echo "tap $2 $3" ;;
  text)  $HDC shell "echo '$2' > /data/local/tmp/noice_text" >/dev/null 2>&1; echo "text $2" ;;
  shot)  N=${2:-shot}
         $HDC shell "power-shell wakeup >/dev/null 2>&1; snapshot_display -f /data/local/tmp/s.jpeg >/dev/null 2>&1; stat -c %s /data/local/tmp/s.jpeg" 2>/dev/null | tr -d '\r' | tail -1
         $HDC file recv /data/local/tmp/s.jpeg "$WIN\\s.jpeg" >/dev/null 2>&1
         cp "$WIN/s.jpeg" "$SHOTS/$N.jpeg" 2>/dev/null && echo "$SHOTS/$N.jpeg" ;;
  alive) $HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1); echo child=\$(basename \$C); echo -n 'childProc='; ps -A | grep -cE 'ashu|appspawn-x'; echo -n 'swaps='; grep -ac WESTLAKE-HWSWAP \$C; echo -n 'nodes='; hidumper -s RenderService -a RSTree 2>/dev/null | grep -ac ashutoshgngwr" 2>&1 | tr -d '\r' ;;
  log)   $HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1); tail -${3:-40} \$C | grep -a '${2:-.}'" 2>&1 | tr -d '\r' ;;
  grep)  $HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1); grep -a '$2' \$C | tail -${3:-20}" 2>&1 | tr -d '\r' ;;
esac
