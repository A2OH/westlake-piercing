---
name: unity-run-attempt-plan
description: Active plan + assets to run a real Unity sample APK on the OHOS board (goal 2026-06-23). Have the APK + exact platform fixes mapped; implementing them.
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

GOAL (2026-06-23, /goal hook active): run a Unity sample APK on the OHOS board. Building on
[[unity-apk-reachability-probe]] (the empirical probe that mapped every wall).

**WORKTREE:** all source-of-record edits in `$HOME/wt-unity` (git worktree of the
migration repo, branch `unity-ohos`, based on engine HEAD 3c698587 — has adapter-src +
westlake-deploy-ohos). Builds run in `$HOME/bridge-build` (NOT git).

**APK obtained (task done):** `$HOME/unity-apks/bt.apk` = `com.JCxYIS.UnityBluetooth`
(17.8MB), MONOLITHIC armeabi-v7a, **Mono** backend (lib/armeabi-v7a/{libunity.so,libmain.so,
libmonobdwgc-2.0.so,libMonoPosixHelper.so}), built bin/Data INSIDE (87 entries), MainActivity =
`com.unity3d.player.UnityPlayerActivity` (canonical), targetSdk33. Backup candidate
`$HOME/unity-apks/sr.apk` (Unity 6.0/2017, targetSdk23). Downloaded via `gh release
download` (raw curl to github 443 times out; gh API path works; release-assets.githubusercontent.com
host also flaky — retry). Between-two-worlds (58MB) was an IL2CPP split/asset-pack bundle — rejected.

**THREE platform fixes needed (from the probe):**
1. **BBQ binding (platform, for ALL apps):** framework.jar BLASTBufferQueue declares
   `nativeUpdate(JJJJI)V` + 7 transaction methods the runtime doesn't bind (runtime registers an
   older `nativeUpdate(JJJIIIJ)V` → unbound → SurfaceView crash). FIX PLAN: add
   `register_android_graphics_BLASTBufferQueue_fixup(JNIEnv*)` to appspawn-x
   `src/framework/appspawn-x/src/AndroidRuntime.cpp` kRegFns table (runs per child at startReg,
   REBUILDABLE via `build_appspawn_x.sh`, NO boot regen, NO runtime-blob rebuild). Replica of
   BBQ_nativeUpdate (struct OhBlastBufferQueue {int32 magic'OHBQ';char name[64];int32 w,h,fmt;
   void* ohNativeWindow;int32 sessionId;}) resolving session via bridge `oh_wm_get_native_window`/
   `oh_wm_get_last_session` + no-op stubs for nativeSetTransactionHangCallback/ApplyPending/
   ClearSync/StopContinuousSync/GatherPending/IsSameSurfaceControl/SyncNextTransaction. Exact sigs
   from `dexdump` of framework.jar (see probe memory). Last-wins over the runtime's failing reg.
2. **EGL window-unwrap (LD_PRELOAD interposer):** OHOS `eglCreateWindowSurface` needs the raw
   OHNativeWindow but the app passes the `oh_anw_wrap`'d AOSP AdapterAnw. FIX: tiny `libeglshim.so`
   interposing `eglCreateWindowSurface` → call bridge `oh_anw_get_oh(win)` (EXPORTED, returns the
   underlying OHNativeWindow or null) → real eglCreateWindowSurface(dpy,cfg,ohnw,attrs) via
   RTLD_NEXT. Add to `start_asx.sh` LD_PRELOAD list (LD_PRELOAD works via start_asx; AT_SECURE
   strips it from init cfg). The OHNativeWindow already gets the G3.4b format/usage fix inside
   bridge getOhNativeWindow, so format should be OK once the right window reaches EGL.
3. **Audio:** AudioSystem natives unimplemented (AudioTrack UnsatisfiedLinkError). DEFER; Unity may
   tolerate or needs stubbing later.

**INSTALL WORKAROUND (apk_install BROKEN — BMS "install internal error"):** piggyback on catalog's
registration. Repackage bt.apk: add shim classes `io.material.catalog.main.MainActivity extends
com.unity3d.player.UnityPlayerActivity {}` + `io.material.catalog.application.CatalogApplication
extends android.app.Application {}` (catalog's registered app class), keep Unity libs/dex/bin/Data,
re-sign, deploy as `/data/app/el1/bundle/public/io.material.catalog/android/base.apk` (backup at
/data/local/tmp/catalog-base.apk.orig = md5 8cfd28db), launch `aa start -a
io.material.catalog.main.MainActivity -b io.material.catalog`. Unity's UnityPlayer reads bin/Data
from applicationInfo.sourceDir (=the base.apk) → finds its data inside.

**DEVICE FLAKY-BOOT drill (critical):** post-reboot children hard-spin in AESKeyGenProbe; dup
appspawn-x parents collide on socket. Always: `pkill -9 -f 'appspawn-x --socket-name'` → ONE
`start_asx.sh` → wait Phase 4 → launch. Reroll for good boot (probe onCreate marker). Tools in
/mnt/c/Users/<user>/Dev/ohos-tools/ (cap2.sh reroll, /tmp/h hdc wrapper).

**STATUS (2026-06-23 cont.): ALL FIXES BUILT + DEPLOYED; blocked on device bring-up, NOT Unity.**

IMPLEMENTED + DEPLOYED:
1. **BBQ binding done IN THE BRIDGE** (not appspawn-x): added `register_BLASTBufferQueue_fixup`
   to `adapter_bridge.cpp` JNI_OnLoad (loaded per child) — binds nativeUpdate(JJJJI)V (real,
   resolves session→OHNativeWindow) + 7 transaction no-op stubs. Bridge rebuilt **`7624ccd9`**
   (build needs `OH_ROOT=$HOME/openharmony AOSP_ROOT=$HOME/bridge-build/aosp
   ADAPTER_ROOT=$HOME/bridge-build bash build/build_adapter.sh --target=liboh_adapter_bridge.so`
   — the default OH_ROOT=$HOME/oh is WRONG; clang is under openharmony/prebuilts). Deployed to
   /system/lib/. Backup `/data/local/tmp/bridge.pre-unity` (=0a18c72b).
2. **EGL interposer built+deployed:** `libeglshim.so` (`4f164123`, src
   `$HOME/bridge-build/unity-port/libeglshim.c`) interposes eglCreateWindowSurface,
   unwraps via bridge `oh_anw_get_oh()`, forwards RTLD_NEXT. Built with OHOS clang
   `openharmony/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang --target=arm-linux-ohos
   -march=armv7-a -mfloat-abi=softfp -mfpu=neon -mthumb --sysroot=openharmony/out/rk3568/obj/
   third_party/musl/usr -isystem <sysroot>/include/arm-linux-ohos`. Deployed /system/android/lib/.
   Added to LD_PRELOAD in `/data/local/tmp/start_asx_unity.sh` (= start_asx + libeglshim).
3. **Unity APK wrapped:** `$HOME/unity-port-build/unity-wrap-signed.apk` (md5 `9652920746`)
   = bt.apk + classes2.dex shim (io.material.catalog.main.MainActivity extends
   com.unity3d.player.UnityPlayerActivity; io.material.catalog.application.CatalogApplication
   stub). Deployed as catalog base.apk (backup catalog-base.apk.orig=8cfd28db). Staged at
   /mnt/c/Users/<user>/Dev/ohos-tools/unity-wrap.apk. Build: javac shim + d8 (exclude the
   UnityPlayerActivity compile-stub) → classes2.dex → zip into bt.apk copy → zipalign + apksigner.

**BLOCKER (environmental, NOT Unity):** appspawn-x reaches "Phase 4: Ready to accept spawn
requests" then **EXITS SILENTLY** (last log = a Thread::Init "after Register (done)", no
SIGSEGV/error) → AMS `aa start` succeeds but **NO child forks** → can't launch ANY app. Confirmed
across 3+ fresh reboots, with BOTH my bridge AND the reverted original bridge (0a18c72b), with/
without libeglshim, CPU-hog killed — so it's device bring-up degradation, not my changes. EARLIER
THIS SESSION appspawn-x persisted fine (probe child 6612 + catalog + noice all forked). Also hit:
post-reboot entropy=256 AESKeyGenProbe spin; device drops `awk`/`tr`; setsid backgrounding
swallows hdc-shell output (check state in a SEPARATE command). 

**APPSPAWN-X PERSISTENCE SOLVED:** the killer was splitting bringup and launch across SEPARATE
hdc sessions — appspawn-x reaped/timed-out between them. FIX: do bringup + launch in ONE
long-running hdc command. Script `/data/local/tmp/run_unity_once.sh` (also on Win staging):
pkill appspawn → setsid start_asx_unity → wait Phase4 → aa start → sleep 40 (single session).
appspawn-x then persists + forks. (Earlier "appspawn dies after Phase 4" across reboots was this,
NOT a real crash.) NOTE: the app child DIES on hdc disconnect — snapshot WITHIN the same session.

**UNITY APK LAUNCHES + INITIALIZES (2026-06-23) — got to the multi-surface frontier:**
Instrumented MainActivity (wrap APK md5 `5cb0c448`, src in unity-port-build) PROVED:
`MainActivity.onCreate ENTER` → `super.onCreate(UnityPlayerActivity) returned OK` (NO exception)
→ `mUnityPlayer = com.unity3d.player.UnityPlayer{...}` — **UnityPlayer IS constructed**, activity
FOREGROUNDS (AbilityTransitionDone ohState=5, LC-HEARTBEAT FOREGROUND). TWO OHOS surfaces created
(`createSurface session=14` = activity window, `session=15` = Unity's child SurfaceView). Unity's
SurfaceHolder.Callback classes loaded (`UnityPlayer$19.surfaceDestroyed`, `$5.run`). Runtime does
heavy `[FIX-VTABLE-A-RELOC]` on UnityPlayer classes (vtable fixups, succeed).

**THE WALL (final, characterized):** Unity DEFERS native-engine load (libunity/libmain NOT mapped
in /proc/PID/maps — confirmed) until `surfaceCreated`/`surfaceChanged` fires on its child
SurfaceView (session 15) → starts the GL/render thread → loadLibrary(unity) → UnityMain. That
callback→native chain does NOT complete: app sits IDLE in the Looper (main thread `do_epoll_wait`,
not crashed), UnityPlayer view stays 0,0, **screen shows the OHOS launcher (no Unity frame)**
(evidence `$HOME/us.jpeg`, 78148B = launcher). So the blocker is the **2nd/child SurfaceView
surface → surfaceCreated callback delivery** (BLASTBufferQueue→ViewRootImpl→SurfaceView.updateSurface
→ app SurfaceHolder.Callback) — the project's historically hardest MULTI-SURFACE area (same family
as catalog 2nd-level / createHardwareBitmap). My BBQ nativeUpdate binding stops the crash but the
Java-side surfaceCreated callback to UnityPlayer$19 isn't reaching it / not triggering the render
thread. EGLSHIM/eglCreateWindowSurface never reached (gated behind this).

**REAL ROOT CAUSE FOUND 2026-06-23 (off-device decompile of bt.apk UnityPlayer — supersedes the
surface-callback theory as the FIRST wall):** `UnityPlayer.loadNative()` (dexdump of classes.dex)
resolves the engine lib from `getApplicationInfo().nativeLibraryDir + "/libmain.so"` (System.load
absolute), fallback `System.loadLibrary("main")` (same dir); on failure calls
`logLoadLibMainError("NativeLoader.load failure, Unity libraries were not loaded.")` and CONTINUES
(UnityPlayer constructs anyway — that's why mUnityPlayer non-null but libunity/libmain NOT mapped).
For the piggybacked catalog APK the adapter sets `nativeLibraryDir=/system/app/io.material.catalog/
lib/armeabi-v7a` (seen in the B43-BIND log) — but the wrapped APK's libs live INSIDE base.apk and
are NOT extracted there (broken installer). So both load attempts fail → native never loads → no
render thread → no frame. **NOT the multi-surface callback (that's a LATER potential wall, not
reached yet).**
**FIX (staged, run when device free):** push Unity's 4 native libs (libmain.so 18K, libunity.so
17M, libmonobdwgc-2.0.so 5.5M, libMonoPosixHelper.so) to `/system/app/io.material.catalog/lib/
armeabi-v7a/`. Libs staged at `/mnt/c/Users/<user>/Dev/ohos-tools/unitylibs/`. Runner staged:
`deploy_unity_libs_and_run.sh` (needs libs pushed to /data/local/tmp/unitylibs/ first) → creates
the NLD dir, copies libs, then `run_unity_diag.sh`. EXPECT: loadNative succeeds → initJni →
libunity maps → render thread → THEN watch for the next wall (EGLSHIM eglCreateWindowSurface via
the BBQ surface, GLES, or the surface-callback). Verify nativeLibraryDir is exactly that path from
a fresh B43-BIND log before assuming.
**FIX APPLIED + BREAKTHROUGH 2026-06-23: Unity native engine NOW LOADS + RUNS on the board.**
Installed Unity's 4 native libs to `/system/app/io.material.catalog/lib/armeabi-v7a/` (the
nativeLibraryDir; /system IS writable — `mount -o remount,rw /system` warns "not in /proc/mounts"
but writes work; do NOT use `set -e` around it). On a GOOD boot (child 2908): `grep -c libunity
/proc/PID/maps` = 4, libmain = 3 → **libunity/libmain MAPPED**; threads include **RenderThread**
+ OS_IPC_2/3/4 + HeapTaskDaemon etc. → Unity's native engine initialized + render thread started.
So loadNative now SUCCEEDS. This is past every prior wall.

**REMAINING WALL (now precisely isolated, post-native-load): Unity's SurfaceView surface delivery.**
On good boots Unity loads native + RenderThread but the app goes IDLE (stderr stops growing,
foreground heartbeat only) — Unity's RenderThread waits for its child SurfaceView's surface; that
surface isn't delivered (no `createSurface` for Unity's SurfaceView, no `surfaceChanged`/EGLSHIM/
`eglCreateWindowSurface`/`nativeRecreateGfxState` in the trace; UnityPlayer view stays 0,0 = not
laid out). So Unity's SurfaceView never gets laid out/sized → no surface created → surfaceCreated
never fires → render thread idle → no frame. Screen = launcher (78K). This IS the multi-surface /
SurfaceView-layout-and-callback wall (project's hardest area). NOTE: the other agent's catalog
launcher prefixes `power-shell wakeup` + `uinput -T -m 360 1050 360 250 200` (swipe) before launch
— focus/traversal nudge; a wake+tap run to trigger Unity's layout/surface was attempted but the
boot was flaky (uinput step may hang). Bad-boot rate is HIGH (many children stick at initChild,
libunity=0) — reroll for a good boot (libunity mapped) before judging the surface wall.

**DEFINITIVE WALL CONFIRMED 2026-06-23 (good-boot capture, child 3443):** on a good boot
(libunity=4 mapped), `unitylog` shows ONLY "onCreate ENTER" — **super.onCreate (UnityPlayer ctor)
HANGS**: main thread wchan=`futex_wait_queue` (blocked inside UnityPlayer construction, after
loadNative succeeded). EGLSHIM=0 eglCWS=0 swap=0 nativeRecreateGfxState=0 surfaceChanged=0
**createSurface(probe)=0** → Unity's child SurfaceView surface is NEVER created. (In an earlier
build/run 2908 super.onCreate RETURNED but app still idled with createSurface=0 — same end: no
child surface.) So BOTH modes = the adapter's ViewRootImpl/SurfaceView traversal does NOT create
Unity's CHILD SurfaceView surface → render thread blocks → main thread blocks in ctor (futex) →
no frame. ROOT WALL = **child-SurfaceView (2nd) surface creation in the adapter's window/traversal
path** = the project's hardest unsolved MULTI-SURFACE area. Likely a main↔render deadlock: ctor
waits (futex) for render thread, render thread waits for the SurfaceView surface, surface needs
onCreate to return → traversal — circular when ctor blocks. Screen=launcher (78K). reroll_good.sh
(Win staging) reliably gets a good boot fast (att 1 here) — use it.

**NEXT to get a frame (multi-surface engineering, deep):** make the adapter create Unity's CHILD
SurfaceView surface. Options: (a) fix the adapter ViewRootImpl/SurfaceView 2nd-surface flow so the
SurfaceView gets laid out + its BLASTBufferQueue surface created → surfaceCreated → render thread;
(b) break the ctor deadlock by feeding a surface to the render thread out-of-band
(UnityPlayer.nativeRecreateGfxState(displayId, Surface) via reflection from a SEPARATE thread, with
a Surface from the BBQ binding / an OHNativeWindow) so the render thread unblocks the ctor;
(c) avoid SurfaceView — render Unity to the activity's main window surface. (a)/(b) are the real
work. Device is severely flaky (most boots stick at initChild; runs hang to 5-min timeouts; shared
w/ another agent) — needs a healthier board to iterate. ORIGINAL pre-superseded note below:
**NEXT to get a frame:** make Unity's SurfaceView actually lay out + create its surface →
surfaceCreated → render thread → eglCreateWindowSurface (libeglshim) → frame. Options: (a) trigger
a real ViewRootImpl traversal/relayout (window focus/resize/wakeup+tap) so the SurfaceView gets
sized; (b) instrument/fix the child-SurfaceView surface-creation path (the adapter's ViewRootImpl/
SurfaceView 2nd-surface flow); (c) force-feed the surface to Unity's render thread reflectively
(UnityPlayer.access$1100(player, displayId, surface) / nativeRecreateGfxState). Unity libs already
at the NLD on device. Catalog base.apk RESTORED to real (8cfd28db) after each Unity run.

**PAUSE note:** another agent intermittently uses catalog on the same board — ALWAYS restore
catalog base.apk (8cfd28db) after Unity runs; check for their live `aa start io.material.catalog`
loop before launching. All my fixes deployed (bridge 7624ccd9, libeglshim, Unity libs at NLD).

**(superseded as first-wall) surface-callback NEXT:** instrument/trace the SurfaceView surface-lifecycle callback path for
session 15 — does ViewRootImpl perform layout/relayout (UnityPlayer was 0,0)? does
SurfaceView.updateSurface call the holder's surfaceCreated? Likely need to fix the child-SurfaceView
surface delivery (multi-surface) so UnityPlayer$19.surfaceCreated fires → Unity render thread starts.
Consider: force layout, or verify BBQ.getSurface returns a valid Surface for session 15, or the
ViewRootImpl 2nd-surface path. All platform fixes (bridge 7624ccd9 BBQ, libeglshim, wrapped APK)
on device. Worktree `$HOME/wt-unity`. Honest status: Unity APK RUNS + UnityPlayer inits on
the board, but no rendered frame yet — blocked at the multi-surface child-SurfaceView callback.

## FINAL (2026-06-23): deadlock-break attempted + DEFEATED → wall is PLATFORM, not app-side
Built SurfaceFeeder thread (wrap APK ba45693c): grabs activity window surface
(WindowManagerGlobal.mRoots→ViewRootImpl.mSurface) + feeds Unity render thread via reflection
nativeRecreateGfxState(0,surface)+nativeResume+nativeFocusChanged(true). DEFEATED by CATCH-22:
super.onCreate (UnityPlayer ctor) consistently HANGS (6/6 good boots, libunity=4, main thread on
futex) → UnityPlayerActivity.mUnityPlayer assigned only AFTER ctor returns → NULL while hung →
feeder player=null → can't feed (nativeRecreateGfxState is a JNI instance method needing the obj).
Render thread waits for surface; ctor waits (futex) for render thread; surface needs the
SurfaceView's child surface the adapter never creates → UNBREAKABLE from app side.
CONCLUSION: rendering a Unity frame requires implementing the ADAPTER child-SurfaceView (2nd
surface) creation + surfaceCreated delivery in ViewRootImpl/SurfaceView/RenderService — a platform
feature (same multi-surface wall the project worked around for catalog 2nd-level), NOT app-side
fixable. Max progress reached: Unity native engine RUNS on OHOS (libunity/libmain mapped,
RenderThread up). Frame blocked on that platform gap + needs a healthy exclusive board (current
board: most boots ctor-hang/initChild-spin, 5-min timeouts, shared). Catalog restored 8cfd28db.

## ROOT CAUSE CORRECTED 2026-06-23 (the multi-surface theory was WRONG — it's a musl linker deadlock)
dumpcatcher of the hung main thread (definitive native stack) shows the REAL wall:
  #00 __timedwait_cp / #01 __pthread_mutex_timedlock_inner (musl)   <- blocked on a mutex
  #02-#05 libunity.so  (STATIC INITIALIZER)
  #06 do_init_fini / #07 dlopen_impl / #08 dlopen (musl)            <- inside dlopen, linker lock held
  #09-#10 libmain.so / #11 art_quick_generic_jni_trampoline         <- System.loadLibrary("main")
So UnityPlayer ctor -> loadLibrary(main) -> dlopen(libunity) -> libunity C++ static init blocks on a
pthread mutex WHILE the musl dynamic-linker lock is held (do_init_fini). Classic: static init spawns
a thread + waits (mutex); that thread needs the linker lock (musl TLS setup __tls_get_new / lock) ->
deadlock. Works on bionic, deadlocks on musl. RACE-FLAKY (run 2908 won once; now ~deterministic hang,
~24/24 across rerolls). This is a musl-vs-bionic dynamic-linker incompatibility at libunity LOAD time
— NOT the SurfaceView/multi-surface wall (that was never even reached; ctor never returns).
TRIED + FAILED: LD_BIND_NOW=1 (musl is already eager-bind; no-op). Preload libunity in appspawn-x
parent (libunity static init COMPLETES at parent startup -> Phase 4 reached! but children fork
BROKEN — libunity parent threads don't survive fork -> 5/5 children miss/die). 
REAL FIX = deep: patch musl ldso to not hold the linker lock across do_init_fini thread-spawn, OR
make libunity's static-init thread not need the linker lock (TLS/relocation), OR a musl recursive
linker lock / TLS-under-dlopen fix. Affects all apps; risky. libunity is prebuilt (can't change).
Reverted start_asx_unity (removed libunity preload; kept eglshim). Catalog restored 8cfd28db.
NET: corrected, precise root cause. Unity frame blocked at libunity dlopen static-init deadlock on musl.

## CONCLUSIVE 2026-06-23: deterministic libunity static-init mutex deadlock (prebuilt-opaque)
KEY correction: the earlier "2908 ctor returned" was BEFORE the native-lib-path fix — libunity
WASN'T found, loadNative failed silently (logLoadLibMainError), ctor returned WITHOUT loading
libunity (no engine, no deadlock). Once libunity actually loads (native-lib fix), the static-init
deadlock is DETERMINISTIC (24/24+), NOT a race. Fixed native stack every time:
  libunity+0x8bb1f (static init) -> +0x2446dd -> +0x2a5f81 -> +0x2a6465 -> pthread_mutex_timedlock
  via musl do_init_fini<-dlopen<-libmain<-System.loadLibrary("main")<-UnityPlayer ctor.
RULED OUT (all tested): musl linker lock (musl releases lock+init_fini_lock BEFORE running each
ctor per ldso/dynlink.c:1532,2145 — not held during ctor); TLS (libunity has NO PT_TLS segment);
lazy binding (musl is eager; LD_BIND_NOW no-op); audio natives (deadlock is at static init, BEFORE
any audio; stubbed AudioSystem.native_getMaxChannelCount/MaxSampleRate/MinSampleRate/getPrimaryOutput*/
getOutputLatency + AudioTrack.native_get_min_buff_size/output_sample_rate in bridge 28bad040 — stack
UNCHANGED); worker-thread-crash (NO faultlog/tombstone/SIGSYS); self-relock (would also deadlock on
bionic). No extra libunity worker thread in the child. => libunity-INTERNAL init mutex deadlock
specific to the OHOS appspawn-x/musl child runtime; opaque (prebuilt stripped libunity, no symbols).
FIX needs libunity internals (unavailable) OR the exact musl-vs-bionic pthread/runtime semantics
diff at libunity+0x2a6465. NET: Unity APK launches + native engine LOADS on OHOS; frame blocked at
this deterministic libunity static-init deadlock. Bridge 28bad040 (BBQ+audio stubs) deployed; eglshim
deployed; unity libs at NLD; start_asx_unity reverted (no preload); catalog base.apk restored 8cfd28db.

## VERSION-INDEPENDENT 2026-06-23: tried Unity 5.x (sr.apk, 2015 libunity) too — SAME 12/12 ctor-hang
Wrapped sr.apk (Unity 5.x/2015, Mono, libunity 14MB dated 2015, MainActivity=
com.example.speechassist.UnityPlayerActivity) with catalog identity (md5 1e556767), installed its
libmain/libmono/libunity to the NLD, launched: SAME deterministic ctor-hang (super.onCreate never
returns) as the 2023 bt.apk. => the libunity static-init deadlock is FUNDAMENTAL + VERSION-INDEPENDENT:
ANY Unity libunity deadlocks in its static initializer when dlopen'd in the OHOS appspawn-x/musl
child. Not fixable by app/version/bridge-stub means. Requires deep OHOS-musl-runtime work (the
musl/bionic pthread-or-loader semantics diff that hangs libunity's init mutex) or Unity engine source
(unavailable). CONCLUSIVE: Unity APK launches + native engine LOADS on OHOS; visible frame blocked by
this fundamental libunity-static-init deadlock. Board restored (catalog 8cfd28db). Exhausted tractable
levers: BBQ-bind, EGL-unwrap, native-lib-path (all real fixes, deployed), audio-stub, LD_BIND_NOW,
parent-preload, two Unity versions.

## STANDALONE TEST 2026-06-23: dlopen libunity outside appspawn-x -> SIGSEGV (exit 139), not deadlock
Built uload (OHOS clang arm32 executable, /data/local/tmp/uload) that dlopens libunity directly in a
non-forked process. Result: SIGSEGV during libunity static init (no ART/Android/JNI env present →
libunity init derefs null env). So the standalone path can't isolate fork-vs-fundamental (it crashes
for a different reason: missing Android runtime env). The mutex deadlock only reproduces in the valid
context = the appspawn-x ZYGOTE-forked child (which has the env). Net: deadlock is in libunity's
static-init synchronization (likely a __cxa_guard / function-local-static init guard or an internal
mutex/condvar at libunity+0x2a6465), deterministic + version-independent, reproducible only in the
forked child, opaque (prebuilt stripped libunity). EXHAUSTED tractable levers. Remaining = reverse-
engineer libunity+0x2a6465 (disassemble the 14-17MB stripped lib) or deep OHOS-musl pthread/__cxa_guard
runtime work — major research, uncertain payoff, needs engine internals. CONCLUSIVE WALL.
