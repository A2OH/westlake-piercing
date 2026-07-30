#!/bin/bash
# wl_sweep.sh — walk a list of "name:x:y" widgets on the Account tab, returning via BACK each time.
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
for item in "$@"; do
  N=${item%%:*}; R=${item#*:}; X=${R%%:*}; Y=${R##*:}
  /home/dspfac/wl_walk.sh "$N" 1080 1830 "$X" "$Y" 2>&1 | head -3
  $HDC shell "echo back > /data/local/tmp/noice_tap" >/dev/null 2>&1
  sleep 3
done
