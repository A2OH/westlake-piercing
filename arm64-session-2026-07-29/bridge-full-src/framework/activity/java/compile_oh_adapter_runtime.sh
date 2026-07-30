#!/bin/bash
# ============================================================================
# compile_oh_adapter_runtime.sh — SLIM VARIANT (Scope B Phase 2)
# ============================================================================
#
# Scope B (2026-05-24): AppSpawnXInit + AppSchedulerBridge moved to
# adapter-runtime-bcp.jar (BCP). This jar is now slim — contains only
# ScopeBSlimMarker — so the JNI PathClassLoader Step 1 probe in
# appspawnx_runtime.cpp succeeds (resolves the marker), then Step 2
# fallback FindClass() against bootstrap finds the BCP AppSpawnXInit.
#
# This script delegates to build_oh_adapter_runtime_slim.sh in workdir
# and copies the produced jar to the canonical ADAPTER_OUT path expected
# by build_aosp_fw.sh / gen_boot_image.sh.
# ============================================================================
set -o pipefail
ADAPTER_ROOT="${ADAPTER_ROOT:-$HOME/adapter}"
AOSP_ROOT="${AOSP_ROOT:-$HOME/aosp}"
WORKDIR="${WORKDIR:-/home/chenyue/scope-b-workdir}"

SLIM_BUILDER="$WORKDIR/build/build_oh_adapter_runtime_slim.sh"
if [ ! -x "$SLIM_BUILDER" ]; then
    echo "ERROR: slim builder missing: $SLIM_BUILDER" >&2
    exit 1
fi

echo "compile_oh_adapter_runtime.sh: delegating to slim builder"
ADAPTER_ROOT="$ADAPTER_ROOT" AOSP_ROOT="$AOSP_ROOT" WORKDIR="$WORKDIR" \
    bash "$SLIM_BUILDER" "$@" || exit $?

SLIM_JAR="$WORKDIR/out/oh-adapter-runtime.slim.jar"
OUT_DIR="$ADAPTER_ROOT/out/adapter"
OUT_JAR="$OUT_DIR/oh-adapter-runtime.jar"
mkdir -p "$OUT_DIR"
cp -f "$SLIM_JAR" "$OUT_JAR"
JAR_MD5=$(md5sum "$OUT_JAR" | cut -d " " -f 1)
JAR_SIZE=$(stat -c%s "$OUT_JAR")
echo "OK: oh-adapter-runtime.jar (slim) installed at $OUT_JAR"
echo "    size=$JAR_SIZE bytes  md5=$JAR_MD5"
