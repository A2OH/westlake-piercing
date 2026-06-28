---
name: liboh-android-runtime-dual-path-2026-05-27
description: "REFERENCE — liboh_android_runtime.so exists at BOTH /system/lib/ AND /system/android/lib/ on DAYU200 V7. appspawn-x mmaps from /system/lib/. Deploying only to /system/android/lib/ silently NOOPs (visible as 'startReg entering (27 modules)' instead of 32 after a fix that adds 4 modules). Always update BOTH paths when shipping a new liboh_android_runtime.so. Discovered 2026-05-27 during Fix J.2-G."
metadata:
  node_type: memory
  type: reference
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

When deploying a new `liboh_android_runtime.so` to DAYU200 V7 substrate, push to **BOTH** of:

- `/system/lib/liboh_android_runtime.so` (← this is what appspawn-x actually mmaps)
- `/system/android/lib/liboh_android_runtime.so` (also referenced; keep in sync)

If you only deploy to `/system/android/lib/`, the old `/system/lib/` version stays live silently. Marker logs (e.g., new `register_*` entries in startReg) won't fire. Looks like the deploy succeeded but nothing changed.

## Diagnostic signal

If you expect a fix to add N new modules to `AndroidRuntime::startReg`, look for the log line `startReg entering (M modules)` in the appspawn-x parent zygote stderr. If `M` matches the OLD module count, you hit the dual-path trap.

## How to apply

Standard deploy snippet for liboh_android_runtime.so:

```bash
source $HOME/openharmony/docs/engine/V3-SCOPEB-PHASE1-EVIDENCE/hdc_verify_send.sh
hdc_verify_send_or_abort "C:\\Users\\dspfa\\Dev\\fix-XX-deploy\\liboh_android_runtime.so" /system/lib/liboh_android_runtime.so
hdc_verify_send_or_abort "C:\\Users\\dspfa\\Dev\\fix-XX-deploy\\liboh_android_runtime.so" /system/android/lib/liboh_android_runtime.so
```

Pre-snapshot also needs both:

```bash
$HDC shell "mkdir -p /data/local/tmp/pre-XX && \
    cp /system/lib/liboh_android_runtime.so /data/local/tmp/pre-XX/liboh_android_runtime.so.system-lib && \
    cp /system/android/lib/liboh_android_runtime.so /data/local/tmp/pre-XX/liboh_android_runtime.so.system-android-lib"
```

## Why this exists

OHOS DAYU200 V7 has historical layering where adapter .so files duplicate between `/system/lib/` (legacy AOSP-style path) and `/system/android/lib/` (Westlake adapter convention). appspawn-x's dlopen path uses the legacy `/system/lib/` for liboh_android_runtime specifically. Other adapter .so files may use only `/system/android/lib/` — check per-file with:

```bash
$HDC shell "find /system -name '<libname>.so' 2>/dev/null"
```

## See also

- [[v3-fix-j2-g-landed-2026-05-27]] — first encounter of this trap
- [[reference-local-build-infra-2026-05-25]] — full deploy infrastructure reference
