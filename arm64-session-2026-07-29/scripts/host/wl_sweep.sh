#!/bin/bash
# wl_sweep.sh — walk a list of "name:x:y" widgets on the Account tab, returning via BACK each time.
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
for item in "$@"; do
  N=${item%%:*}; R=${item#*:}; X=${R%%:*}; Y=${R##*:}
  $WLROOT/wl_walk.sh "$N" 1080 1830 "$X" "$Y" 2>&1 | head -3
  $HDC shell "echo back > /data/local/tmp/noice_tap" >/dev/null 2>&1
  sleep 3
done
