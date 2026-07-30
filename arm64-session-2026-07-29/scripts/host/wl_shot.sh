#!/bin/bash
# wl_shot.sh <name> — screenshot the panel and drop it in the scratchpad; prints the size oracle.
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
SHOTS=/tmp/claude-1000/-home-dspfac-openharmony/9adf5c05-1946-4e31-a77a-b6e8688c50b6/scratchpad/shots
N=${1:-shot}
$HDC shell "power-shell wakeup >/dev/null 2>&1; snapshot_display -f /data/local/tmp/s.jpeg >/dev/null 2>&1; stat -c %s /data/local/tmp/s.jpeg" 2>/dev/null | tr -d '\r' | tail -1
$HDC file recv /data/local/tmp/s.jpeg 'C:\Users\dspfa\Dev\wlstage\s.jpeg' >/dev/null 2>&1
mkdir -p $SHOTS; cp /mnt/c/Users/dspfa/Dev/wlstage/s.jpeg "$SHOTS/$N.jpeg" && echo "$SHOTS/$N.jpeg"
