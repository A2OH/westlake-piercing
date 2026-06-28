---
name: unity-apk-reachability-probe
description: Empirical test (2026-06-23) of whether a pure Unity APK can run on the Westlake AOSP-on-OHOS board — it cannot; first fatal wall = SurfaceView BLASTBufferQueue.nativeUpdate UnsatisfiedLinkError; audio also dead
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

**VERDICT: a pure Unity APK cannot run on the board today.** Proven empirically with a
layered Unity-shaped probe (SurfaceView + native EGL/GLES via dlopen + AudioTrack),
not just static audit.

**Probe build/deploy harness (reusable for any AOSP APK test):**
- Sources `$HOME/unity-probe/` (AndroidManifest, src, jni/probe.c, build-apk.sh).
  build-apk.sh: aapt/dx/apksigner from `$HOME/aosp-android-11/prebuilts/sdk/...`
  + NDK 25.2 armv7a clang (`$HOME/android-sdk/ndk/25.2.9519653`) for the
  armeabi-v7a native lib. Device is **32-bit ARM** → build armeabi-v7a.
- **`apk_install` on device is BROKEN for new bundles** (v2 = manifest-parse only;
  `apk_install_v1`/`apk_install2` also return 0 but BMS install fails "internal error
  9568260"). So NEW APKs can't be installed via the normal path right now.
- **WORKAROUND that worked: piggyback on an already-registered bundle.** Built the probe
  with catalog's identity (package `io.material.catalog`, activity
  `io.material.catalog.main.MainActivity`, + a no-op stub of the registered Application
  class `io.material.catalog.application.CatalogApplication` — REQUIRED or the runtime
  dies instantiating the missing app class), backed up + overwrote
  `/data/app/el1/bundle/public/io.material.catalog/android/base.apk`, launched via
  `aa start -a io.material.catalog.main.MainActivity -b io.material.catalog`. Restored
  the orig base.apk after (backup `/data/local/tmp/catalog-base.apk.orig`).
- **Logging:** app System.err → hilog as tag `AppSpawnXJava: [stderr]`; android.util.Log
  → hilog tag. Capture with `hilog -x | grep ' <pid> '`. (Child `adapter_child_<pid>.stderr`
  gets native fprintf + bridge `alog:` but NOT Java System.err.) Catalog child uid 16371.
  Flaky-boot ~50%: some boots stall at AESPROBE before app code — reroll.

**Execution trace on a good boot (child 3750):** runtime init OK → AM/PM/WM adapters
installed → CatalogApplication(stub).onCreate OK → **MainActivity.onCreate OK** →
`System.loadLibrary("probe")` **nativeLoaded=true** (custom JNI .so loads + runs fine) →
setContentView(SurfaceView) OK → then TWO walls:

1. **AUDIO — dead.** First `AudioTrack` call threw
   `UnsatisfiedLinkError: android.media.AudioSystem.native_getMaxChannelCount()` — the
   runtime has NO audio-output native impl (matches: no libaudio/OpenSLES/AudioTrack JNI
   in adapter).
2. **SurfaceView — FATAL wall (kills the process).** During SurfaceView surface setup the
   main thread threw `UnsatisfiedLinkError: android.graphics.BLASTBufferQueue.nativeUpdate(JJJJI)`
   → `[CHILD_CK] J_invokeStaticMain_main_threw` → InvocationTargetException → **process death.**
   So `surfaceCreated` NEVER fires → the app never gets a Surface → the native EGL probe
   never ran (eglCreateWindowSurface untested — blocked behind this).

**Why noice/catalog work but Unity won't:** regular Views render via ViewRootImpl → the
adapter's implemented `createSurface`/`getOhNativeWindow` path (hwui/Skia, EGL works for the
SYSTEM). **SurfaceView** (what Unity/NativeActivity/GLSurfaceView use for an app-owned GL
surface) uses **BLASTBufferQueue**, whose native methods are unimplemented → instant crash.

**What works (good news):** custom JNI .so load + execute, Activity lifecycle, window/
InputChannel/OHGraphicBufferProducer creation, Mali GPU driver loads (`rk-debug-maliso
rk_so_ver: v6`). So IL2CPP/C# would run; the blockers are platform surface + audio.

**To make Unity reachable (in order):** (1) implement `BLASTBufferQueue` native methods
(nativeUpdate etc.) so SurfaceView presents → surfaceCreated fires; (2) verify app-owned
EGL/GLES on the SurfaceView's ANativeWindow (wrap ANativeWindow↔OHNativeWindow app-side;
OHOS libEGL/libGLESv2/v3 exist in graphic_2d); (3) implement AudioTrack→OHAudio/libohaudio
(AudioSystem natives). (1) is the first/hardest. Cross-ref [[catalog-2nd-level-canvascontext-wall]]
(SurfaceControl/multi-window is the project's hardest area).

## NEXT-STEP RESULT 2026-06-23 (got past SurfaceView; app-owned EGL pinpointed)
Pushed the probe through the SurfaceView wall and ran the full app-owned GL path on a
clean boot (child 6612). **BLASTBufferQueue was NOT missing — it's a framework.jar↔runtime
ABI skew.** The runtime registers an OLDER-AOSP `kBlastBufferQueueMethods` (incl. a custom
`nativeGetFromBlastBufferQueue` that bridges BBQ→OHNativeWindow + `BBQ_nativeUpdate` that
resolves sessionId→OHNativeWindow via bridge `oh_wm_get_native_window`/`oh_wm_get_last_session`),
but the deployed Android-13 framework.jar declares `nativeUpdate(JJJJI)V` (runtime registered
`(JJJIIIJ)V` → unbound) + transaction/sync methods the runtime lacks (`nativeSetTransactionHangCallback`,
`nativeApplyPendingTransactions`, `nativeClearSyncTransaction`, `nativeStopContinuousSyncTransaction`,
`nativeGatherPendingTransactions`, `nativeIsSameSurfaceControl`, `nativeSyncNextTransaction`).
**FIX used (no boot regen):** from libprobe.so `JNI_OnLoad`, `RegisterNatives` a real
`nativeUpdate(JJJJI)V` (replicating BBQ_nativeUpdate; struct `OhBlastBufferQueue` layout copied
verbatim: magic'OHBQ',name[64],w,h,fmt,void*ohNativeWindow,sessionId) + no-op stubs for the 7
transaction methods. Result: **surfaceCreated FIRED**, Surface→`ANativeWindow_fromSurface`
returned the real OHNativeWindow, app `dlopen libEGL/libGLESv2/libandroid` all OK,
`eglGetDisplay`/`eglInitialize`(EGL 1.4)/`eglChooseConfig`/`eglCreateContext` ALL SUCCEED app-side.
**FINAL WALL: `eglCreateWindowSurface → EGL_NO_SURFACE` (err 0x3000=EGL_SUCCESS, null-on-bad-window)**
— the BBQ-bridged OHNativeWindow has `format=0` (the bridge log `SetDefaultFormat(12) rc=50102000`
FAILED). Same family as the documented system-EGL G3.4b wall (defFormat=0/no HW_RENDER usage),
now on the app-owned path. **So app-owned GL is ~90% there**: display/context/config work; only the
window-surface bind fails on pixel-format config. To finish: apply the G3.4b format/usage fixups
(`SetDefaultUsage|0x100|0x200` + `NativeWindowHandleOpt(SET_FORMAT,12)`) to the BBQ-bridged window
(bridge `oh_wm_get_native_window` path), not just the hwui `getOhNativeWindow` path. Audio remains a
separate unimplemented wall.
**FLAKY-BOOT GOTCHA (cost ~20 rerolls):** post-reboot, app children hard-SPIN at ~98% CPU in
`AESKeyGenProbe`/`OHSecureRandomSpi` and never reach onCreate; force-stop reroll DIDN'T help and
duplicate appspawn-x parents (2703/2059…) collided on the socket. FIX: `pkill -9 -f 'appspawn-x
--socket-name'` (ALL parents+stuck children) → ONE clean `start_asx.sh` → launch. Then good boot
came immediately. Probe sources `$HOME/unity-probe/` (jni/probe.c has the BBQ shim+stubs);
device scripts in `/mnt/c/Users/<user>/Dev/ohos-tools/` (cap2.sh reroll). Catalog base.apk restored
(8cfd28db).
