#!/bin/bash
# build_boot_image_arm64.sh — cross-build an aarch64 ART boot image on the host
# with art-latest's x86 dex2oat (has ARM64 codegen). AOSP-standard cross approach.
#
# ★ THE TWO FIXES that make it build (found 2026-07-08 via lldb backtrace — the
#   "UnstartedRuntime clinit crash" was a RED HERRING; the real crashes were):
#   1. --inline-max-code-units=0  — the optimizing compiler's inliner null-derefs
#      in HInliner::SubstituteArguments; disabling inlining avoids it (x86 + arm64).
#   2. -j1                        — the multi-threaded compiler races on arm64
#      target (SIGSEGV at -j4); single-thread builds cleanly (~4s).
#
# Result: boot.art (~5MB, arm64) + boot-core-libart.art + boot-core-icu4j.art
#   (+ .oat/.vdex each). Verified: art-latest arm64 dalvikvm LOADS it and runs
#   Runtime::Start through native registration + root clinits (then hits a
#   separate daemon-thread DetachCurrentThread abort — the next frontier).
set -e
D2O=$WLROOT/art-latest/build/bin/dex2oat            # x86-64 host, HAS ARM64 codegen
CJ=$WLROOT/art-latest/core-jars                     # a15 core jars (match art-latest ART)
ART=$WLROOT/art-latest
OUT=${1:-$WLROOT/bridge-build-arm64/bootimg}
mkdir -p "$OUT"

ANDROID_ROOT=$ART "$D2O" \
  --dex-file=$CJ/core-oj.jar --dex-file=$CJ/core-libart.jar --dex-file=$CJ/core-icu4j.jar \
  --oat-file=$OUT/boot.oat --image=$OUT/boot.art \
  --instruction-set=arm64 --compiler-filter=speed --base=0x70000000 \
  --inline-max-code-units=0 \
  --android-root=$ART --runtime-arg -Xverify:none -j1
echo "arm64 boot image -> $OUT/boot.art"

# On-board run (art-latest arm64 dalvikvm, art-latest/build-ohos-arm64/bin/dalvikvm):
#   ANDROID_ROOT=/system dalvikvm -Xint -Xverify:none -Ximage:<dir>/boot.art \
#     -Xbootclasspath:core-oj.jar:core-libart.jar:core-icu4j.jar -cp app.dex Main
# Loads image + inits runtime; current blocker = daemon thread exits without
# DetachCurrentThread (thread.cc:2460) during Runtime::Start.
