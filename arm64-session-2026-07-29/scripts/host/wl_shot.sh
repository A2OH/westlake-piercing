#!/bin/bash
# wl_shot.sh <name> — screenshot the panel and drop it in the scratchpad; prints the size oracle.
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
SHOTS=$WL_OUT/shots
N=${1:-shot}
$HDC shell "power-shell wakeup >/dev/null 2>&1; snapshot_display -f /data/local/tmp/s.jpeg >/dev/null 2>&1; stat -c %s /data/local/tmp/s.jpeg" 2>/dev/null | tr -d '\r' | tail -1
$HDC file recv /data/local/tmp/s.jpeg 'C:\Users\dspfa\Dev\wlstage\s.jpeg' >/dev/null 2>&1
mkdir -p $SHOTS; cp $WIN_STAGE/s.jpeg "$SHOTS/$N.jpeg" && echo "$SHOTS/$N.jpeg"
