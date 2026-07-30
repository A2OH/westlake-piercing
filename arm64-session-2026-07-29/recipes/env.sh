#!/bin/bash
# Common environment for every recipe here. Source it: . recipes/env.sh
export HDC="${HDC:-/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe}"   # hdc.exe runs on WINDOWS via WSL interop
export WIN_STAGE="${WIN_STAGE:-/mnt/c/Users/dspfa/Dev/wl}"       # hdc can only read Windows paths
export ASX=/data/local/tmp/asx                                   # device staging root

# ⚠️ADAPTER_ROOT must be EXPORTED. The inner build script defaults it to $(dirname $0)/.. which is
# one level too deep, and every path then silently breaks (it tries to regenerate IInputConstants.h
# and hits a hardcoded /home/HanBingChen/aosp aidl).
export ADAPTER_ROOT="${ADAPTER_ROOT:-/home/dspfac/bridge-build}"
export AOSP_ROOT="${AOSP_ROOT:-/home/dspfac/bridge-build/aosp}"
export OH_ROOT="${OH_ROOT:-/home/dspfac/openharmony}"

export SDK="${SDK:-/home/dspfac/android-sdk}"
export D8="$SDK/build-tools/34.0.0/d8"
export ZIPALIGN="$SDK/build-tools/34.0.0/zipalign"
export ANDROID_JAR="$SDK/platforms/android-34/android.jar"
# dexlib2 for every tool in tools/
EXT="$SDK/cmdline-tools/latest/lib/external"
export DEXLIB_CP="$EXT/com/android/tools/smali/smali-dexlib2/3.0.3/smali-dexlib2-3.0.3.jar:$EXT/com/android/tools/smali/smali-util/3.0.3/smali-util-3.0.3.jar:$EXT/com/google/guava/guava/31.1-jre/guava-31.1-jre.jar"

# push <local> <devicepath>  — routes through the Windows staging dir, since hdc cannot see WSL paths
push() { mkdir -p "$WIN_STAGE"; cp "$1" "$WIN_STAGE/$(basename "$1")"; \
         "$HDC" file send "$(wslpath -w "$WIN_STAGE/$(basename "$1")")" "$2" | tail -1; }
dsh()  { "$HDC" shell "$@"; }
alive(){ "$HDC" shell "ps -A | grep -ac ashu" | tr -d '\r '; }
childlog() { "$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" | tr -d '\r'; }
