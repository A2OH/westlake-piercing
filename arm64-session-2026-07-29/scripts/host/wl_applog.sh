#!/bin/bash
# applog.sh <pid> [grep-extra] — the board's equivalent of `adb logcat` for the ported app.
# Paths come from recipes/env.sh — source it first (or set HDC).
#
# android.util.Log.println_native is routed by the bridge to HiLogPrint with domain 0xD000F00,
# which hilog renders as "C00f00/<TAG>". So every Log.d/i/w/e the app makes is already on the
# device — it just never reaches the child's stderr, which is where all the westlake markers go.
# That split is why the app looked silent for so long: two sinks, and only one was being read.
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
H=${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}
PID=${1:?usage: applog.sh <pid> [extra-egrep]}
EXTRA=${2:-}

# OH's own graphics libs share domain 0xD000F00, so drop the render/vsync chatter that would
# otherwise bury the app's lines at roughly a thousand to one.
NOISE='OH_DER_VSync|skia|Choreographer|RSRenderThread|OH_DER_|Ace|RenderService'

timeout 200 $H shell "hilog -x -P $PID" 2>/dev/null \
  | tr -d '\r' \
  | grep -a 'C00f00/' \
  | grep -avE "C00f00/($NOISE)" \
  | { [ -n "$EXTRA" ] && grep -aE "$EXTRA" || cat; }
