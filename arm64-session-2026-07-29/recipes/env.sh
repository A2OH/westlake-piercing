#!/bin/bash
# Common environment for every recipe and build script here. Source it: . recipes/env.sh
#
# WLROOT is the one knob: the directory holding every source tree this port builds from
# (bridge-build, bridge-build-arm64, aosp-15, aosp-14-minikin, aosp-android-11, ohos-6.1-src,
# ohos-sdk-6.1, openharmony, art-latest, third_party, android-sdk). On the original host = $HOME.
# REPRODUCE.md section 0 says which of those trees each build script actually needs.
export WLROOT="${WLROOT:-$HOME}"

# hdc.exe is a WINDOWS binary reached through WSL interop and CANNOT read WSL paths, so every device
# transfer stages through a directory Windows can see. Auto-detect the Windows profile that has the
# OHOS tools rather than hardcoding a username.
if [ -z "$WIN_DEV_ROOT" ]; then
  for c in /mnt/c/Users/*/Dev; do
    [ -x "$c/ohos-tools/hdc.exe" ] && { WIN_DEV_ROOT="$c"; break; }
  done
fi
export WIN_DEV_ROOT="${WIN_DEV_ROOT:-/mnt/c/Users/Public/Dev}"
export HDC="${HDC:-$WIN_DEV_ROOT/ohos-tools/hdc.exe}"   # hdc.exe runs on WINDOWS via WSL interop
export WIN_STAGE="${WIN_STAGE:-$WIN_DEV_ROOT/wl}"       # hdc can only read Windows paths
export ASX=/data/local/tmp/asx                          # device staging root

# ⚠️ADAPTER_ROOT must be EXPORTED. The inner build script defaults it to $(dirname $0)/.. which is
# one level too deep, and every path then silently breaks (it tries to regenerate IInputConstants.h
# and hits an absolute aidl path from the original author's machine that does not exist here).
export ADAPTER_ROOT="${ADAPTER_ROOT:-$WLROOT/bridge-build}"
export AOSP_ROOT="${AOSP_ROOT:-$WLROOT/bridge-build/aosp}"
export OH_ROOT="${OH_ROOT:-$WLROOT/openharmony}"

export SDK="${SDK:-$WLROOT/android-sdk}"
export D8="$SDK/build-tools/34.0.0/d8"
export ZIPALIGN="$SDK/build-tools/34.0.0/zipalign"
export ANDROID_JAR="$SDK/platforms/android-34/android.jar"
# dexlib2 for every tool in tools/
EXT="$SDK/cmdline-tools/latest/lib/external"
export DEXLIB_CP="$EXT/com/android/tools/smali/smali-dexlib2/3.0.3/smali-dexlib2-3.0.3.jar:$EXT/com/android/tools/smali/smali-util/3.0.3/smali-util-3.0.3.jar:$EXT/com/google/guava/guava/31.1-jre/guava-31.1-jre.jar"

# --- device helpers -------------------------------------------------------------------------
# ★hdc exits 0 on several real failures, so these VERIFY instead of trusting the exit code.

# board_online — call before any recipe that touches the device.
board_online() {
  "$HDC" list targets 2>/dev/null | grep -qE '[0-9a-f]{8}' \
    || { echo "board NOT reachable over hdc — see REPRODUCE.md section 1" >&2; return 1; }
}

# push <local> <devicepath> — stages through Windows, then confirms arrival by comparing size.
push() {
  [ -f "$1" ] || { echo "push: no such local file: $1" >&2; return 1; }
  mkdir -p "$WIN_STAGE"; cp "$1" "$WIN_STAGE/$(basename "$1")" || return 1
  "$HDC" file send "$(wslpath -w "$WIN_STAGE/$(basename "$1")")" "$2" | tail -1
  local want got; want=$(stat -c%s "$1")
  got=$("$HDC" shell "wc -c < $2" 2>/dev/null | tr -d '\r ')
  [ "$want" = "$got" ] || { echo "push FAILED: $2 is '$got' bytes on device, expected $want" >&2; return 1; }
  echo "push ok: $2 ($want bytes)"
}

# recv <devicepath> <local> — ★without the emptiness check a failed recv silently leaves the PREVIOUS
# run's file in place, and the recipe then patches a stale artifact and reports success.
recv() {
  mkdir -p "$WIN_STAGE"; local b; b=$(basename "$2")
  rm -f "$WIN_STAGE/$b" "$2"
  "$HDC" file recv "$1" "$(wslpath -w "$WIN_STAGE/$b")" >/dev/null 2>&1
  [ -s "$WIN_STAGE/$b" ] || { echo "recv FAILED: $1 — board offline, or the file is not there" >&2; return 1; }
  cp "$WIN_STAGE/$b" "$2"
}

dsh()  { "$HDC" shell "$@"; }
alive(){ "$HDC" shell "ps -A | grep -ac ashu" | tr -d '\r '; }
childlog() { "$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" | tr -d '\r'; }

# Where host drivers drop screenshots, dumps and run logs. Must be a durable dir: the original
# scripts wrote into a per-session scratchpad that does not exist for anyone else.
export WL_OUT="${WL_OUT:-$WLROOT/wl-out}"; mkdir -p "$WL_OUT" 2>/dev/null
