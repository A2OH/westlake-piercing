---
name: noice-touch-input-rootcause
description: Touch input on the DAYU200/OHOS adapter is blocked SYSTEM-WIDE at the MMI↔display foundation layer (MMI has no physical display registered → can't route touch coords to ANY window, launcher included). KEY events DO route to the focused window. Supersedes the older InputEventBridge-ITE theory.
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

**2026-06-04: TOUCH input root-caused — it's a SYSTEM-WIDE OHOS MMI/display foundation issue, NOT the adapter app layer / NOT the InputEventBridge ITE.**

## Evidence (noice FOREGROUND, pid 2046, tapped via uinput -T -c X Y)
- Input never reaches noice (no OH_IER/InputEventBridge/dispatchInputEvent in child stderr).
- MMI (multimodalinput, pid 346) InputWindowsManager errors on EVERY pointer event:
  - `Failed to obtain physical(-1) display`
  - `CHKPV(physicalDisplayInfo) is null`
  - `windowsPerDisplay_ contain invalid displayId:-1`
  - `Failed to add active window: windowInfo with windowId:2 not found` — AND **windowId:10 (the launcher) ALSO fails**. So NOTHING on the device receives touch, not just noice.
- Restarting multimodalinput (kill 346 → respawn) did NOT fix it (display info genuinely absent, not transient MMI state).

## Root cause
MMI has **no valid physical display** registered → it can't map a touch coordinate to a display → can't find the target window → drops the event. The board HAS a touchscreen (`hidumper -s 3101 -l`: deviceId 5 "VSoC touchscreen" deviceType:17, kernel event5) but it's not bound to a display in MMI (pointer events carry displayId -1). The display works for OUTPUT (snapshot_display renders fine) — only MMI's INPUT-side display registration is missing. In OHOS the display geometry is pushed to MMI by RS/WindowManager (`UpdateDisplayInfo`); on this adapter/board that push isn't happening. **Foundation/native layer — NOT patchable with the adapter-layer tools (LD_PRELOAD interposers / binary patch / smali BCP) used for the render+network fixes.** The earlier [[noice-input-routing-diagnosis]] ITE theory was a later stage that only matters once events reach the app — here they never do.

## KEY events DO work (routing-wise)
`uinput -K -d <ohKeycode> -u <ohKeycode>` routes to the FOCUSED window: hilog `InputKeyFlow HandleInputEvent eid:9 InputId:12 wid:10` + `WMSEvent ... DispatchKeyEvent ret:51` → delivered to wid:10 (whatever is focused). Keys don't need display geometry. So **D-pad/key navigation is the viable interaction path on this board** (touch is foundation-blocked). OH keycodes: DPAD_UP=2012, DOWN=2013, LEFT=2014, RIGHT=2015, CENTER=2016, ENTER=2054. REQUIRES noice to be foreground+focused (gated by the ~50% flaky initChild spawn) AND noice to support d-pad focus traversal (untested).

## Net
UI RENDERS fully (icons+tags+network data — [[noice-network-inet-gid-fix]]). UI INTERACTIVITY via TOUCH is blocked at the OHOS MMI/display foundation (system-wide, no physical display in MMI). Via KEYS it's possible but gated by noice foreground reliability. See [[noice-input-routing-diagnosis]] (superseded ITE theory).

## KEY-NAV FIX LOCATED (2026-06-04) — but bridge-build-env blocked
KEY events DO reach noice's process: child log `[alog:OH_InputBridge] OnInputEvent(KeyEvent): keyCode=2013 action=2 (Phase1 stub — not forwarded)` (2013=DPAD_DOWN, 2016=DPAD_CENTER). **The fix:** `src/framework/window/jni/oh_input_bridge.cpp` `OnInputEvent(KeyEvent)` (line ~425) is an explicit "Phase 1 stub — not forwarded" — the `OnInputEvent(PointerEvent)` path right below it (line ~435, translates OH action→Android + publishes to the InputChannel) IS implemented. So key-nav needs ~80 lines mirroring the PointerEvent path (translate OH keycode→Android keycode, build a key InputMessage, publish to the session's InputChannel). It compiles into **liboh_adapter_bridge.so** (BUILD.gn target `oh_adapter_bridge`, framework/jni/BUILD.gn:102; sources incl window/jni/oh_input_bridge.cpp).
**BUILD BLOCKER:** the bridge is built by the standalone cross-compile `build/inner/compile_oh_adapter_bridge.sh` (NOT GN/ninja). Local build needs (have): OH clang++ + arm-linux-ohos musl sysroot; (MISSING locally): `out/aosp_lib` AOSP cross-libs (libnativehelper.so etc.), an OHOS tree GN-configured WITH the adapter component (the script harvests -I paths from `$OH/out/rk3568` ninja plan for `oh_adapter_bridge` — absent in my out/rk3568/build.ninja), and the full ADAPTER_ROOT source. These three = the user build env (OH_ROOT=$HOME/oh, AOSP_ROOT=$HOME/aosp, ADAPTER_ROOT=$HOME/scope-b-workdir). Constraint "build locally / no hbc build" → blocked without replicating that env locally (major infra). RTTI/-fno-rtti + exact-base alignment also matter (mismatch can regress the working bridge). So: key-nav fix is precisely scoped but gated on the bridge build environment.

## LOCAL BRIDGE BUILD STOOD UP (2026-06-04) + KEY-FORWARDING FIX IMPLEMENTED
Replicated the liboh_adapter_bridge.so cross-compile LOCALLY (no hbc build). Setup at $HOME/bridge-build:
- ADAPTER_ROOT=$HOME/bridge-build (pulled user scope-b-workdir src+build+out/aosp_lib; `framework`→src/framework symlink). out/aosp_lib libs were symlinks to $HOME/adapter/out/aosp_lib → re-pulled REAL files with `tar czhf` (deref).
- AOSP_ROOT=$HOME/bridge-build/aosp (pulled HBC aosp include dirs: libnativehelper, system/core, frameworks/base/{libs/hwui,core/jni,libs/androidfw}, etc.).
- OH_ROOT=$HOME/openharmony, OVERLAID HBC oh headers (version skew fix) for: window_manager, ability, bundle_framework, multimodalinput/input, graphic_2d (+*.in), hilog, skia/m133, out/rk3568/gen (json.h, wmserver, appexecfwk_core). Placed HBC's oh_adapter_bridge.ninja at out/rk3568/obj/adapter/framework/jni/ for the -I/-D harvest.
- Link libs: pulled the 28 OH .z.so from the DEVICE (ABI-match) into out/rk3568/packages/phone/system/lib/ + libEGL/GLESv3/vulkan + AOSP libs (real). musl libm/libdl/libpthread → symlink to libc.so.
- HiLogPrint conflict (adapter int forward-decl vs log_c.h enum) in oh_app_mgr_client.cpp → removed forward-decl + cast macro literals to (LogType)/(LogLevel).
- BUILD: `OH_ROOT=.. AOSP_ROOT=.. ADAPTER_ROOT=.. BRIDGE_TMP=/tmp/bridge_build BUILD_INNER_INVOKED=1 bash build/inner/compile_oh_adapter_bridge.sh` → 39/39 compiled, links, out/adapter/liboh_adapter_bridge.so (1.31MB; deployed orig was 1.59MB — debug-info/flags diff, verifying no regression).
**Deployed bridge is /system/lib/liboh_adapter_bridge.so (NOT /system/android/lib).** Backup: /data/local/tmp/bridge-orig.so. md5 of fixed: 22fd473e...

KEY-FORWARDING FIX (in oh_input_bridge.cpp): replaced the OnInputEvent(KeyEvent) "Phase1 stub" with: translate OH keycode→Android via ohKeyCodeToAndroid() (DPAD_UP 2012→19, DOWN 2013→20, LEFT 2014→21, RIGHT 2015→22, CENTER 2016→23, ENTER 2054→66, BACK 2→4, etc.), OH action 2/3→Android 0/1, then OHInputBridge::injectKeyEvent()→writeKeyEvent() which writes a bit-exact AOSP InputMessage::Body::Key (type=KEY) to the session InputChannel (mirrors injectTouchEvent/writeMotionEvent). Added KeyEventBody struct + injectKeyEvent/writeKeyEvent (decls in oh_input_bridge.h). Testing render+D-pad nav next.

## LOCAL BRIDGE BUILD: code-complete fix, but ABI/base-consistency wall (2026-06-04)
The key-forwarding fix COMPILES (39/39, links) with a targeted HBC-header overlay — but the resulting liboh_adapter_bridge.so REGRESSES noice at runtime: deployed it → noice never spawns (stuck AMS state #INITIAL, no process, crash BEFORE child stderr redirect). Restored /data/local/tmp/bridge-orig.so (md5 7d0c471d) → noice spawns + FOREGROUND + renders FIRST TRY (so the correct bridge makes noice spawn RELIABLY; the earlier "~50% flaky spawn" was partly MY broken bridge). The crash is a C++ ABI/struct-layout mismatch: my locally-built bridge mixes HBC headers (overlaid: window_manager/ability/bundle/mmi/graphic/hilog/skia) with MY openharmony headers (c_utils/ipc/samgr/etc.) which differ from the deployed OH libs' ABI (RefBase/sptr/Parcel layout). Overlaying the REST (c_utils/ipc/...) to fix ABI then breaks COMPILE consistency (OHSurfaceSource undeclared in surface.h, LogType undeclared) because the mixed header set isn't self-consistent. → Producing a deployable bridge needs user's EXACT, COMPLETE, self-consistent oh tree (the deployed-base) + toolchain; piecemeal local replication is unbounded header whack-a-mole, and building on user is forbidden ("build locally"). **Key-forwarding code is correct + saved in $HOME/bridge-build/framework/window/jni/oh_input_bridge.cpp (+ oh_input_bridge.h); local build env at $HOME/bridge-build works (compiles+links).** To finish: either (a) pull user's COMPLETE oh header tree for a self-consistent build, or (b) get a one-off user build of the bridge with the fix. DEVICE LEFT GOOD: orig bridge restored, noice renders.

## ✅ KEY-FORWARDING FIX WORKS — bridge rebuilt ABI-consistent (2026-06-04)
The complete-HBC-header overlay fixed the ABI mismatch. Built liboh_adapter_bridge.so (md5 2ab08b85...) with the key-forwarding fix, deployed to /system/lib/liboh_adapter_bridge.so. RESULT: (1) NO REGRESSION — noice spawns + FOREGROUND + renders first try; (2) KEYS FORWARD — child log `[alog:OH_InputBridge] OnInputEvent(KeyEvent): ohCode=2013 -> android=20 action=0 session=11 (forwarding)` + `injectKeyEvent: session=11 action=0 keyCode=20 result=0` for DPAD_DOWN(2013→20) and DPAD_CENTER(2016→23). Keys are translated + written to noice's InputChannel successfully (result=0). 
THE ABI FIX (what made the local build work): replace the OLD external_window.h that wins -I resolution — `cp foundation/graphic/graphic_surface/interfaces/inner_api/surface/external_window.h (has OHSurfaceSource) → foundation/graphic/graphic_2d/interfaces/inner_api/surface/external_window.h` + complete HBC header overlay (oh_full: c_utils/ipc/samgr/etc.) so the bridge's struct/vtable ABI matches the deployed OH libs. Build env at $HOME/bridge-build is fully working now. Backup of orig: /data/local/tmp/bridge-orig.so (7d0c471d).
REMAINING: demonstrate VISIBLE d-pad navigation on a POPULATED list (test run had empty list [flow race] + USB dialog overlay). Keys reach noice's ViewRootImpl; need a populated focusable list to see focus move / sound open.
