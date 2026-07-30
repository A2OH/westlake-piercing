#!/bin/bash
# wl_deploy.sh <tag> — stage the freshly-built bridge and deploy it, verifying it actually landed.
#
# ⚠️This does NOT restart the app — the old name said "restart" but it never did. The child loads the
# bridge at startup, so nothing pushed here takes effect until wl_restart.sh runs. See REPRODUCE.md §2.
set -eo pipefail
TAG=${1:-dev}
# Paths come from recipes/env.sh — sourced relative to this script so it works from anywhere,
# whether run in place or copied to ~ (see REPRODUCE.md section 0.5).
for _e in "$(dirname "$0")/../../recipes/env.sh" "$HOME/wl-kit/recipes/env.sh" "$WL_KIT/recipes/env.sh"; do [ -f "$_e" ] && { . "$_e"; break; }; done
: "${HDC:?source recipes/env.sh first, or set WL_KIT to the kit directory}"
board_online || exit 1

SRC=$WLROOT/bridge-build-arm64/out/liboh_adapter_bridge.so
[ -f "$SRC" ] || { echo "no bridge at $SRC — build it first"; exit 1; }
STAGED=$WLROOT/bridge-build-arm64/bridge_$TAG.so
cp "$SRC" "$STAGED"

# ★The previous version staged into $WIN_STAGE but SENT from a different Windows directory
# (…/Dev/wlstage), so it could ship a stale .so and still report success. push() sends exactly what
# was just staged and checks the size on the device.
push "$STAGED" "/data/local/tmp/bridge_$TAG.so" || exit 1
"$HDC" shell "cp /data/local/tmp/bridge_$TAG.so $ASX/liboh_adapter_bridge.so && chmod 755 $ASX/liboh_adapter_bridge.so" >/dev/null

want=$(md5sum "$STAGED" | cut -d' ' -f1)
got=$("$HDC" shell "md5sum $ASX/liboh_adapter_bridge.so" | tr -d '\r' | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "DEPLOY FAILED: device md5 '$got' != local '$want'"; exit 1; }
echo "deployed $TAG  md5=$got"
echo "★ now run wl_restart.sh — the running child is still on the OLD bridge"
