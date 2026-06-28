---
name: noice-input-routing-diagnosis
description: Touch input reaches noice but delivery throws — root-caused to the reflective InputEventReceiver.dispatchInputEvent path in the adapter. Exact break point found; fix blocked on missing smali assembler for BCP instrumentation.
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

**2026-06-02: noice renders (G3.8, [[noice-g38-renders-stable]]) but TOUCH INPUT doesn't reach its handlers** — can't navigate (stuck on AppIntro, can't Skip/swipe/tab). Diagnosed the exact break.

## Input chain trace (tap at 360,640 via `uinput -T -d X Y -i 80 -u X Y`)
1. `InputManagerImpl: Pointer event action:2` (down) — OHOS MMI receives ✅
2. `ServerMsgHandler: PointerEvent Id=444` ✅
3. ⚠️ `InputWindowsManager: Failed to add active window: windowInfo with windowId:16 not found` — MMI doesn't have noice's OH window registered as touchable (possible secondary issue)
4. `OH_IER: dispatchInputEvent ... src=TOUCHSCREEN (posting to main looper)` — the bridge native input receiver (C++, liboh_adapter_bridge.so) GETS the touch + posts to noice's main looper ✅
5. ❌ `OH_InputEventBridge: dispatchOnMainThread post: java.lang.reflect.InvocationTargetException` — delivering into noice's Android pipeline THROWS. **THE BREAK.**

So input reaches noice; the reflective dispatch into the view tree fails. (KEY events `InputKeyFlow recv` did reach the process earlier; touch is the failing path.)

## Architecture (from baksmali of oh-adapter-framework.jar `adapter/window/InputEventBridge`)
- `createInputChannelPair(binder, session, name)`: `InputChannel.openInputChannelPair()` → channels[0]=SERVER stored in mServerChannels[session] + `nativeRegisterInputChannel(session, server)`; channels[1]=CLIENT returned to ViewRootImpl (which makes its WindowInputEventReceiver). So the PROPER AOSP transport = native writes to server socket → ViewRootImpl reads client socket (NO reflection).
- BUT delivery actually uses `dispatchOnMainThread(InputEventReceiver receiver, int seq, InputEvent event)` (static): resolves `android.view.InputEventReceiver.getDeclaredMethod("dispatchInputEvent", int, InputEvent)` (cached sDispatchMethod, setAccessible), posts `InputEventBridge$1` runnable to main Handler → `m.invoke(receiver, [seq, event])` → throws ITE → catch logs ONLY the wrapper (`Log.w(tag, "dispatchOnMainThread post: "+e)`), NOT `e.getCause()`.
- `onOHTouchEvent(I I F F J J)` is a LOG-ONLY STUB (builds no MotionEvent) — so the real event+receiver are built in NATIVE (the bridge) and passed to dispatchOnMainThread via JNI.
- Inference: the AOSP InputChannel socket transport likely isn't wired through on OHOS, so the reflective dispatchOnMainThread is the real delivery mechanism — and it's failing. The cause is inside `InputEventReceiver.dispatchInputEvent` → `onInputEvent` (likely a malformed native-built MotionEvent, or wrong/!ViewRootImpl receiver). FIX is probably BRIDGE-side (native event/receiver construction) which is BUILDABLE (no boot-regen) — but need the ITE cause to confirm.

## BLOCKER: can't get the ITE cause cheaply
Getting the cause = instrument `InputEventBridge$1.run()` catch to `Log.w(tag, msg, throwable)` (3-arg prints full "Caused by:" stack). That's a BCP-jar (oh-adapter-framework.jar) change → needs smali ASSEMBLER + dex2oat boot-regen. **No smali assembler present locally** (only baksmali/dexlib2/util 3.0.3 in android-sdk + baksmali 2.2.4 in aosp tree); **maven central + bitbucket are BLOCKED** (curl 404/empty; only pip's mirror works). pip `smali` 0.2.5 is broken (missing VERSION) + disassembler-only. dex2oat64 toolchain IS present (`$HOME/tools/dex2oat64` + lib64/libsigchain.so; regen pattern in `docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh`; BCP order: core-oj,core-libart,core-icu4j,okhttp,bouncycastle,apache-xml,adapter-mainline-stubs,framework,adapter-runtime-bcp,oh-adapter-framework).

## Paths forward (unblock options)
1. **dexlib2 patcher** (HAVE dexlib2 3.0.3): write a small Java prog to rewrite InputEventBridge$1.run() to 3-arg Log.w → boot-regen → read cause. Cleanest unblock for ALL future BCP work. Moderate effort.
2. **Get smali assembler** another way (build from `$HOME/aosp-android-11/external/smali` gradle, or a reachable mirror).
3. **Bridge-side instrumentation**: change the bridge's native dispatch to do the reflective call itself with JNI `ExceptionDescribe()` (prints full Java stack to log) — but the post is async so needs synchronous dispatch. Bridge is buildable (no boot-regen) → faster iteration once chosen.

Tooling: baksmali works = `java -cp smali-baksmali-3.0.3.jar:smali-util-3.0.3.jar:smali-dexlib2-3.0.3.jar:guava-31.1-jre.jar:jcommander-1.78.jar com.android.tools.smali.baksmali.Main d classes.dex -o out` (all under android-sdk/cmdline-tools/latest/lib/external/...). Decompiled ohaf at /tmp/ohaf_dis/out.

## DEEPER (2026-06-02 cont): failing code is in liboh_android_runtime.so, NOT the bridge
The `OH_IER`/`dispatchOnMainThread` path is in **liboh_android_runtime.so** (`register_android_view_InputEventReceiver`), NOT liboh_adapter_bridge.so (bridge only has `writeMotionEvent`+`nativeRegisterInputChannel`, the socket SERVER side). So the "speculative BRIDGE fix" premise is moot — the break is runtime-side. Pulled the **deployed** source (remote read-only OK): `[user]@[REDACTED-HOST]:58222` pw=`[REDACTED-CRED]` (NOT [REDACTED-CRED]), via paramiko (`/tmp/fetch_src.py`); files at `/home/[user]/scope-b-workdir/src/framework/android-runtime/src/android_view_InputEventReceiver.cpp` + `.../window/jni/oh_input_bridge.cpp` (saved /tmp/dep_IER.cpp, /tmp/dep_oh_input_bridge.cpp). Flow (`dispatchMotionFromWorker`, IER.cpp:310): worker polls clientFd → reads InputMessage (bridge wrote via writeMotionEvent to serverFd) → `MotionEvent.obtain(downMs,evtMs,action,x,y,metaState)` (6-arg "(JJIFFI)") + `setSource(0x1002 TOUCHSCREEN)` → resolves receiver from WeakReference → `InputEventBridge.dispatchOnMainThread(receiver,seq,event)` (posts to main looper; they post deliberately — direct worker-thread dispatch crashes on RippleDrawable ValueAnimator Looper-thread check, disproven-safe 2026-05-19). MotionEvent looks WELL-FORMED, receiver is the live ViewRootImpl WindowInputEventReceiver. Deployed `InputEventReceiver.dispatchInputEvent(int seq, InputEvent event)` (framework.jar classes4) = `mSeqMap.put(event.getSequenceNumber(), seq); onInputEvent(event)`. So the ITE cause is inside `onInputEvent`→ViewRootImpl input stages (runtime-state-dependent). **CONSTRAINT REALITY: liboh_android_runtime.so is non-reproducible (no SQLite locally) + remote builds forbidden → only BINARY-PATCH vector; can't meaningfully speculative-patch complex native dispatch without the cause.** Real fix needs the ITE cause → instrument InputEventBridge (BCP). NOW FEASIBLE locally: pull InputEventBridge.java from remote, edit catch→3-arg Log.w(tag,msg,throwable), `javac --release 17` + `d8` (build-tools/34.0.0/d8 present) → graft class into ohaf via dexlib2 (have 3.0.3) → dex2oat64 boot-regen (`$HOME/tools/dex2oat64`, regen pattern docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh) → reboot → read cause. (Heavier than hoped but the only correct route.)

See [[noice-g38-renders-stable]], [[westlake-wall-map]].
