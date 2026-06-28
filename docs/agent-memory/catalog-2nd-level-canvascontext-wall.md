---
name: catalog-2nd-level-canvascontext-wall
description: 2nd-level demo Activity crash (Material Catalog) — FIXED+DEPLOYED+VALIDATED 2026-06-23 = createHardwareBitmap uninitialized-window SIGBUS; libhwui 0c82b1db (off-screen OHOS readback) + bridge 20ab65a6 (oh_imagereader_*); AdaptiveListViewDemoActivity now renders
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

> **CONSOLIDATED MILESTONE DOC:** `docs/engine/V3-CATALOG-UI-PIPELINE-VALIDATION-2026-06-24.md` — Catalog = full-UI-pipeline validation; 7 walls / 9 root-cause fixes (metaData NPE, touch dispatch, createHardwareBitmap SIGBUS, transition morph L1/L2/L3, Date Picker W9 vtable, launcher entry.hap, board stability); every fix UNIVERSAL; all walls ABOVE libc (companion to `V3-BIONIC-MUSL-ANALYSIS-2026-06-24.md`). Both docs PUSHED to A2OH/westlake main (catalog 1c767f37, bionic fff81239).

## ✅✅✅ FIXED + DEPLOYED + VALIDATED (2026-06-23 cont.) — the createHardwareBitmap fix shipped; demo Activities render

**The fix written below ("REAL ROOT CAUSE FOUND") was built, deployed, and proven on device.**
Approach was upgraded from "just `return nullptr`" to a REAL off-screen readback so hardware
bitmaps actually work (shared-element/RenderEffect snapshots), with graceful null fallback.

- **libhwui.so `0c82b1db`** (deployed `/system/android/lib/libhwui.so`): `createHardwareBitmap`
  now renders the RenderNode into an off-screen OHOS `IConsumerSurface` (producer ANativeWindow
  wrapped via `oh_anw_wrap`, like the on-screen path), `AcquireBuffer`+fence-wait+CPU-map the
  dmabuf, copy pixels into a heap `Bitmap`. dlsym's 4 helpers from the bridge; falls back to
  null (software bitmap) if absent. Built from `$HOME/bridge-build` via
  `build_aosp_lib.sh --target=libhwui.so`. **Phase-4 UND gate FALSE-FAILED** (its whitelist
  `build/inner/libhwui_und_whitelist.txt` is stale+UNSORTED → `comm: not in sorted order`,
  exit 1) — REAL audit = `nm -D` UND diff vs deployed known-good 21daf5e9 = **0 new UND**
  (brick-safe; same 1006-symbol UND set). NOT a BCP jar → **no boot regen needed**.
- **liboh_adapter_bridge.so `20ab65a6`** (deployed `/system/lib/`): adds `extern "C"`
  `oh_imagereader_{create,get_window,acquire,destroy}` in `surface/jni/surface_oh_helper.cpp`
  (OHOS-primitive AImageReader equiv). Built via `build_adapter.sh --target=liboh_adapter_bridge.so`.
  **BUILD-FIX needed:** the new file had a local `OH_NativeWindow_NativeWindowHandleOpt(void*,…)`
  forward-decl that conflicts with `external_window.h` (already #included) + called it with `void*`
  → removed the decl, cast `nw`→`reinterpret_cast<OHNativeWindow*>(nw)` at the 2 call sites.
  Superset of deployed 42ab7575 — all input/scroll/key/drag/control-channel symbols preserved
  (verified via `nm -D`).

**VALIDATED on device** (reboot to reload .so under fresh appspawn-x prefork; catalog child
pid 2469 uid **16371** — NOT 13731 like noice; `aa start -a io.material.catalog.main.MainActivity
-b io.material.catalog`; nav via tap channel `echo "X Y" > /data/local/tmp/noice_tap`):
grid → Adaptive(180,400) → **List View Demo(300,520) = AdaptiveListViewDemoActivity RENDERS**
(Inbox email list); deeper nav to email detail also works. stderr:
`[OH-HWBMP] OK req=720x144 buf=720x144 stride=2880` (readback succeeded, stride=720*4 RGBA_8888);
**SIGBUS=0 SIGSEGV=0 FATAL=0 setupPipelineSurface=0**, process survived. Evidence
`docs/engine/V3-CATALOG-L2FIX-EVIDENCE/` (01 grid, 02 adaptive page, 03 ListView RENDERED,
04 deeper nav). Backups: `/data/local/tmp/{libhwui.pre-hwbmp(21daf5e9), bridge.pre-hwbmp(42ab7575)}`.
Local staged: `bridge-build/out/{aosp_lib/libhwui.so, adapter/liboh_adapter_bridge.so}` +
`/mnt/c/Users/<user>/Dev/ohos-tools/new_{libhwui_0c82b1db,bridge_20ab65a6}.so`. Launcher
`/data/local/tmp/launch_catalog.sh` (note: detects uid 13731 — patch to 16371 for catalog).
**LESSON: any app calling ThreadedRenderer.createHardwareBitmap (activity transitions, RenderEffect)
hit this; the fix is universal, not catalog-specific.**

**WATERMARK EXPERIMENT (honest scope of the fix).** To answer "is the snapshot actually painted
on screen?" a diagnostic libhwui overwrote the returned bitmap with OPAQUE MAGENTA. Result:
returned bitmap IS magenta (`min=max=0xffff00ff`, 103680/103680 px — readback code runs + hands a
valid bitmap to MaterialContainerTransform), but a 60-frame burst across the transition shows
**0.00% magenta on screen in ALL frames** (incl. the mid frames, which are byte-identical to the
non-watermark run = the destination Activity drawing in FAB+nav-first, NOT a morph overlay).
**So: createHardwareBitmap/AImageReader WORKS (renders→off-screen OHOS surface→correct readback,
proven 2 ways: real-content dump + watermark-in-returned-bitmap), but the OHOS adapter does NOT
composite the Android activity shared-element/MaterialContainerTransform overlay** → the snapshot
is captured-but-never-displayed; the transition is a plain cut. **The fix's concrete value is
CRASH-PREVENTION / REACHABILITY (kills the uninitialized-window SIGBUS so demo Activities open at
all), NOT a visible transition.** Production libhwui restored to `0c82b1db` (clean readback, no
watermark/dump). Evidence `docs/engine/V3-CATALOG-L2FIX-EVIDENCE/07-watermark-midframe-NO-magenta.jpeg`.

**REAL AImageReader NDK SHIM + Codex review (2026-06-23, supersedes the custom readback above).**
Replaced the one-off createHardwareBitmap patch with a genuine `AImageReader_*`/`AImage_*` shim
(in bridge `surface/jni/surface_oh_helper.cpp`, exported, backed by OHOS IConsumerSurface — CPU
readback; GPU AHardwareBuffer import is stubbed on OHOS so `AImage_getHardwareBuffer` reports
unsupported, callers use `AImage_getPlaneData`). createHardwareBitmap rewritten to use the real
AImageReader ABI via dlsym (the sanctioned cross-.so pattern; avoids DT_NEEDED surgery).
**Codex (codex-cli 0.128.0) reviewed it; ALL findings fixed:** per-image buffer ownership (each
AImage holds its own consumer sptr + SurfaceBuffer, releases in AImage_delete — multi-acquire
safe) [HIGH]; native-window wrapper leak in oh_imagereader_destroy now freed via oh_anw_destroy +
OH_NativeWindow_DestroyNativeWindow [MED, hit every call]; typed dlsym fn-ptrs (no void* punning)
[MED]; thread-safe symbol init via function-local static [MED]; validate acquired buf >= requested
(bw/bh/stride/srcLen) [MED]; reject non-RGBA_8888 + planeIdx!=0 [LOW]. **DEPLOYED: bridge
`0a18c72b` + libhwui `1d04a56e`** (UND diff vs known-good 21daf5e9 = 0, brick-safe). VALIDATED:
`[OH-HWBMP] OK(AImageReader)` real content, SIGBUS/SIGSEGV/FATAL/setupPipe all 0, AdaptiveListView
DemoActivity launches+onCreate+AMS-foregrounds+renders Inbox (evidence 08-*.jpeg, review in
codex-review-findings.md). Backups `/data/local/tmp/{libhwui.pre-hwbmp(21daf5e9), bridge.pre-hwbmp(42ab7575)}`.
**Transition root cause (Part 2) — MAJOR PIVOT 2026-06-23: it is TRACTABLE adapter plumbing, NOT framework surgery.**
Three Explore agents concluded Piece 2 needed ~1200 lines reproducing the transition framework
in framework.jar (BCP) + boot regen + was dual-window-blocked. **THAT WAS WRONG — the agents read
the abandoned shim SOURCE tree (`android-to-openharmony-migration/shim/java/...`), NOT the deployed
jar.** Verified against the DEPLOYED `framework.jar` (8524dc56, pulled + probed with dexlib2):
`android.app.ActivityOptions` is the FULL REAL AOSP class (mSharedElementNames, mTransitionReceiver,
real makeSceneTransitionAnimation(Activity,View,String) + (Activity,Pair[]), startSharedElementAnimation,
real toBundle) AND ALL coordinators are REAL: ExitTransitionCoordinator, EnterTransitionCoordinator,
ActivityTransitionCoordinator, ActivityTransitionState, SharedElementCallback (all DEFINED in
framework.jar/classes.dex). So the transition framework is COMPLETE on-device; framework.jar needs
NO change.
**The real gap = adapter plumbing (the ONE thing Agent A got right):** (a) `ActivityManagerAdapter.startActivity`
(in **oh-adapter-framework.jar** `Ladapter/activity/ActivityManagerAdapter;`) drops the `Bundle options`
→ `bridgeStartAbility(intent)` ignores ActivityOptions; (b) `AppSchedulerBridge` (in **adapter-runtime-bcp.jar**
`Ladapter/activity/AppSchedulerBridge;`, has `$OhTokenRegistry` side-channel) sets the destination
LaunchActivityItem `activityOptions = null`. Carry the options Bundle A→B and the REAL
EnterTransitionCoordinator runs the morph. **Catalog is ONE process (uid 16371)** → the shared-element
ResultReceiver handoff is IN-PROCESS (no IPC) and the morph is drawn single-window in the destination
overlay (Android's own enter side) → the dual-window wall only blocks the source *fade*, not the morph.
**PLAN (L1):** patch ohaf ActivityManagerAdapter.startActivity to stash the options Bundle (static
handoff, keyed to correlate with the launch — reuse AppSchedulerBridge$OhTokenRegistry pattern) +
patch arb AppSchedulerBridge to set it on LaunchActivityItem instead of null → boot regen (ohaf+arb are
BCP, small jars, proven pipeline) → deploy → validate the destination's EnterTransitionCoordinator fires.
**TOOLING NOTE:** smali toolchain was wiped by reboot. Reconstructed dexlib2 helpers in `/tmp/fwktools/`
(DexProbe/DexList/DexClasses + Baksmali2 work; compiled vs `$HOME/apktool.jar`). **Baksmali works;
the smali ASSEMBLER convenience class is NOT in apktool.jar** (only the antlr parser classes
smaliParser/smaliFlexLexer/smaliTreeWalker) — must reconstruct the assemble pipeline (lexer→parser→
treewalker→DexPool→FileDataStore) OR rebuild adapter jars from `adapter-src` (RISK: deployed ohaf/arb
carry accumulated smali patches — chooser efd3f740, PIB 6e32a253 — that may not be in source; prefer
smali-patching the DEPLOYED jars). framework.jar pulled to `/tmp/fwkpatch/`, adapter jars to `/tmp/adjars/`.
Device unchanged (shim 0a18c72b+1d04a56e still live, demos render). Use `apktool.jar` for d8-merge of any NEW classes.

**P2.L1 BUILT + DEPLOYED (boot-safe, no brick) but UNVERIFIED (2026-06-23 cont.).**
Smali toolchain reconstructed in `/tmp/fwktools/` (compile vs `$HOME/apktool.jar`): `Baksmali2`
(disassemble), `SmaliAssemble` (assemble: smaliFlexLexer→smaliParser→smaliTreeWalker→DexBuilder→
FileDataStore — apktool.jar lacks the convenience Smali class so this wrapper IS the assembler;
round-trips arb 33→33 cleanly), `DexProbe`/`DexList`/`DexClasses` (dexlib2 helpers). API 30.
**L1 implementation:** new class `adapter/activity/TransitionOptionsHolder` (Java→d8→smali, src
`/tmp/toh/`) with static ConcurrentHashMap; `stashFromIntent(Intent,Bundle)` keys the options
Bundle by intent component className; `resolve(Intent)` pops it + `ActivityOptions.fromBundle()`
(via reflection — @hide). Added the class to **arb**; smali-edited **arb** `AppSchedulerBridge`
LaunchActivityItem.obtain site (`const v32,0x0` → `invoke-static/range {v19..v19} resolve; move-result-object v32`
— v19=Intent, v32=the ActivityOptions arg14); smali-edited **ohaf** `ActivityManagerAdapter` ALL 4
launch variants (startActivity .line202 p3/p10; startActivityWithFeature .line212 p4/p11;
startActivityAsUser .line222 p3/p10; startActivityAsUserWithFeature .line232 p4/p11) to
`invoke-static {intent,opts} stashFromIntent` after logBridged. Repackaged jars: **ohaf 56e9b98e,
arb a06bb7f3** (zip -q replace classes.dex). **BOOT REGEN** via `docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh`
(WORK=/tmp/p2regen; ABSOLUTE path — cd-in-compound breaks the relative path → rc=127). **BRICK-SAFE
PROOF (refined):** an all-deployed-jars regen reproduced the deployed boot byte-for-byte
(boot-framework.oat ad790fe9 + vdex e00ded75) → my dex2oat == the deployed-boot builder → patched
boot is compatible. (boot-framework.oat differs as 9da3c42f in the patched build = benign relocation
from the larger arb segment; vdex stays e00ded75.) Deployed boot via tar→/system/android/framework/arm/ +
jars→/system/android/framework/; **rebooted, NO BRICK, catalog+demos render (drew>0, SIGBUS=0).**
Backup `/data/local/tmp/p2-backup/` (orig ohaf a567689d + arb 6e32a253 + 30 boot files) = clean rollback.
**BLOCKER (validation):** the `[OHTransition]` stash/resolve traces appear NOWHERE — not in the child
stderr, not in hilog. **Java Log.i/System.err sinks are effectively DEAD for the catalog app process**
(only native fprintf like [OH-HWBMP] reaches the stderr file), so I CANNOT observe whether stash fires
or whether `options` is null at launch. No class-load error (TransitionOptionsHolder linked OK). So L1
is deployed-but-unconfirmed. **NEXT:** (1) make TransitionOptionsHolder write to a FILE
(/data/local/tmp/ohtransition.log via FileOutputStream) instead of Log — definitively shows if stash
fires + if opts!=null (one more build/regen/reboot cycle); (2) if opts IS null at launch, the catalog
may not pass makeSceneTransitionAnimation for the grid→demo launch (test other nav), OR a deployed-
framework Instrumentation path drops options; (3) even with options carried, the cross-activity morph
needs the SOURCE ExitTransitionCoordinator to run + send the shared-element bitmap via the in-process
ResultReceiver BEFORE the source window is torn down (single-mission teardown) = likely an L2 timing
fix (defer source teardown ~300ms). Staged artifacts: /tmp/adjars/{ohaf,arb}_patched.jar, /tmp/p2regen/.

**P2.L1 VALIDATION via file-log (2026-06-23 cont.) — B-side WORKS, A-side stash is a PUZZLE.**
Switched TransitionOptionsHolder logging to a FILE (`/data/local/tmp/ohtransition.log`, pre-created
0666; FileWriter append) since Java Log/System.err are dead for the app process. Rebuilt arb
(a17406fe) + boot regen + redeploy (ohaf unchanged 56e9b98e). **RESULT (definitive):**
```
resolve called comp=io.material.catalog.main.MainActivity -> resolve MISS
resolve called comp=io.material.catalog.adaptive.AdaptiveListViewDemoActivity -> resolve MISS
```
So **B-side resolve (arb AppSchedulerBridge, the LaunchActivityItem delivery hook) FIRES correctly**
for every launch with the right component — my arb edit works + the file-log works from the catalog
process. **But NO `stash called` line EVER** → the A-side capture in ohaf
ActivityManagerAdapter.startActivity* never runs. **The puzzle:** stash IS in the deployed code
(deployed boot-oh-adapter-framework.vdex contains `stashFromIntent`; deployed boot-ohaf.oat 2a672502 ==
my regen) AND the A-side path provably executes (child stderr shows `[alog:OH_ATMJNI] nativeStartAbility:
ability=AdaptiveListViewDemoActivity` in the catalog process — and nativeStartAbility is only called by
bridgeStartAbility(204) [4 startActivity* callers, all stash-patched] + startService(5542)). The stash
invoke-static sits immediately before the bridgeStartAbility call with no branch between, so if
bridgeStartAbility ran, stash should too — yet it didn't log. Both stash + resolve run in the same
catalog process (uid 16371) with the same FileWriter, and resolve logs fine. **Leading hypothesis:**
the catalog's in-app startActivity does NOT route through the Java ActivityManagerAdapter.startActivity*
methods — the A→OH activity launch is likely handled NATIVELY (OH_ATMJNI) bypassing the Java methods,
so nativeStartAbility fires from a native path, not via bridgeStartAbility's Java callers. **NEXT
DIAGNOSTIC (1 regen cycle):** add a file-log directly inside `bridgeStartAbility` (provably-run if Java
path) — if it logs, the A-side Java runs + isolate the stash invoke; if it does NOT log, the launch is
native (OH_ATMJNI) and options must be captured in the bridge .so JNI (nativeStartAbility impl, or the
Instrumentation/Activity layer) rather than ActivityManagerAdapter. THEN even once A-side capture works,
L2 (source ExitTransitionCoordinator timing vs single-mission teardown) likely remains. Device: full-L1
deployed (ohaf 56e9b98e, arb a17406fe, patched boot), NO BRICK, catalog+demos render; rollback
/data/local/tmp/p2-backup/. ohtransition.log live (0666).

**P2.L1 ✅ COMPLETE + PROVEN (2026-06-23 cont.) — root cause was the WRONG ADAPTER CLASS.**
The catalog's startActivity goes through **`ActivityTaskManagerAdapter`** (IActivityTaskManager, JNI tag
`OH_ATMJNI`), NOT `ActivityManagerAdapter` (IActivityManager) which I'd been patching — that's why stash
never fired. BOTH classes are in ohaf. Fixed by stashing in **ActivityTaskManagerAdapter** startActivity
(2225: .registers 12, p4=Intent p11=opts, calls bridgeStartActivityWithStack), startActivityAsUser
(p4/p11), startActivityAsCaller (p3/p10) — insert `invoke-static {intent,opts} stashFromIntent` after
logBridged. **PROVEN via /data/local/tmp/ohtransition.log (FileWriter; Java Log/System.err are dead for
the app process; file must be pre-created 0666):**
```
stash comp=...AdaptiveListViewDemoActivity opts=sz=8   <- options NON-NULL (8 keys), captured
stash STORED ...AdaptiveListViewDemoActivity
resolve comp=...AdaptiveListViewDemoActivity mapsz=1 -> resolve HIT   <- delivered to LaunchActivityItem
```
all in one pid (same-process static handoff works). So the REAL ActivityOptions (shared-element scene
transition) now reach the destination's EnterTransitionCoordinator. Deployed ohaf **e1ae51d3** + arb
**fda6948c** + patched boot (vdex e00ded75, brick-safe). **DEPLOY/VALIDATE GOTCHAS LEARNED:** catalog
render is ~50% flaky (drew=0/nativeStartAbility=0 ⇒ launch didn't activate; loop aa force-stop+start
until drew=1 before navigating, else the log is empty and looks like a regression); the demo nav is
grid→Adaptive(180,400)→ListViewDemo(300,520) via /data/local/tmp/noice_tap.
**NEW WALL (L2/L3):** once options are delivered the real cross-activity transition machinery ENGAGES
(`MaterialContainerTransformSharedElementCallback.onCreateSnapshotView/onSharedElementEnd` routed,
createHardwareBitmap fires) — but the catalog process then DIES (alive=0, screen=launcher), no SIGBUS/
FATAL. hilog shows AMS then re-launches MainActivity and `NotifyStartProcessFailed`(realpath entry.hap) —
ambiguous: could be the cross-activity transition failing (source ExitTransitionCoordinator + in-process
ResultReceiver + dual-window NOT supported → EnterCoordinator can't complete) OR the known flaky
appspawn-x respawn. **So delivering options engaged the morph machinery but it can't complete →
need L2: keep the source activity's ExitTransitionCoordinator alive + the in-process ResultReceiver
shared-element handoff before single-mission teardown, OR make the EnterTransitionCoordinator degrade
gracefully (fade-in w/o shared element) instead of leaving the app in a bad state.** L1 capture/delivery
is the solved hard plumbing; L2 is the remaining transition-execution work. Device: ohaf e1ae51d3 + arb
fda6948c deployed (tapping a demo now engages the transition — may destabilize the catalog vs the prior
plain render; rollback /data/local/tmp/p2-backup/ to a567689d+6e32a253+orig boot if a clean state is wanted).

**P2.L2 STATUS (2026-06-23 cont.) — BLOCKED by device environment + likely-architectural; could not get a clean validation.**
Re-deployed L1 (ohaf e1ae51d3 + arb fda6948c) and tried repeatedly to observe what the cross-activity
transition actually does on a clean render. COULDN'T: the device is in a bad environment — **11% battery
→ aggressive auto-lock (上滑解锁 lockscreen)**, the **~50% flaky blank-render** (drew=1 but white screen,
shot~19K; persists even on the STABLE build, so it's environmental NOT L1/L2), the **displayId:-1
compositing quirk** (catalog drew=1 but launcher shows, shot~78K = not foregrounding), and during a
heavy reroll+burst test the **device spontaneously REBOOTED under load** (uptime reset; recovered fine,
fs intact, not bricked). So I never got one clean frame of the demo transition. The "white screen" the
user saw = blank-render + lockscreen, NOT the L2 build. Unlock via `uinput -T -m 360 1050 360 250 200`
(swipe-up, no PIN). **L2 verdict:** the faithful cross-activity morph needs the source MainActivity's
ExitTransitionCoordinator to hand the shared-element snapshot to B via the in-process ResultReceiver
before single-mission teardown (theory: MainActivity is the back-stack launcher so it SHOULD stay alive
→ single-window-in-B morph possible WITHOUT dual-window) — BUT engaging the coordinator (run 2182) was
followed by app death + AMS NotifyStartProcessFailed, ambiguous between transition-failure and the flaky
appspawn-x respawn. **Cannot resolve without a stable/charged device.** L1 (deliver options) is the solid
done milestone; L2 needs (a) the device CHARGED + stable to even validate, then (b) likely either confirm
MainActivity stays alive for the in-process handshake (maybe tractable) or solve dual-window (the project's
hardest wall, blocked). Device currently on the L1 build (demo-taps engage the transition → may
destabilize the catalog); revert to stable via /data/local/tmp/p2-backup/ for a clean UX. DON'T do heavy
flash/reboot cycles until charged (spontaneous-reboot-during-/system-write = brick risk). Staged:
/tmp/adjars/{ohaf_patched=e1ae51d3, arb_patched=fda6948c}.jar, /tmp/p2regen/out/boot-image/.

**P2.L2 ✅ CRASH RESOLVED + transition works; ❌ visible morph blocked by OVERLAY COMPOSITING (2026-06-23 cont., device charging via USB).**
Got clean GENTLE runs (battery overridden via `hidumper -s BatteryService -a "--capacity 95"`; device
plugged USB pluggedType=2 chargingStatus=1 but stuck ~11%; NO pkill/reboot — those triggered the earlier
spontaneous reboot). **KEY RESULT: the earlier "death" was ENVIRONMENTAL (flaky appspawn-x respawn /
low-battery), NOT the transition.** On a clean run: L1 delivers options (stash STORED→resolve HIT), the
real EnterTransitionCoordinator engages (createHardwareBitmap fires = morph snapshot created,
MaterialContainerTransformSharedElementCallback routed), the catalog **SURVIVES (alive=1 throughout)**,
and AdaptiveListViewDemoActivity (the Inbox) **OPENS + renders fully** (evidence frame mm7=65092). So
the cross-activity transition is NOT architecturally blocked / does not crash — single-mission is fine
because MainActivity (back-stack launcher) stays alive for the in-process ResultReceiver handshake, and
the morph is single-window in B. **REMAINING (the visible morph):** the EnterTransitionCoordinator
POSTPONES the enter (~several sec) waiting for the shared-element handshake — sometimes it completes +
the demo opens (mm7), sometimes it stays postponed on the SOURCE within the ~8s window (16 tight-burst
frames all 69994=Adaptive, demo not yet shown). And the shared-element OVERLAY never composites: across
all bursts the source frame is unchanged (no morph intermediate) — same displayId:-1 / window-overlay
compositing limit the WATERMARK experiment proved (the snapshot is created but the transition overlay is
not painted to the glass). So: **demo opens via the transition path + no crash (L2 functional), but NO
smooth visible morph (overlay not composited + slow postpone).** Final visible morph needs the
window-overlay/displayId compositing fixed (a rendering-layer wall, same family as the broader
multi-window issues) — that's the true L3 remainder, NOT the transition logic (which works). Device on
L1 build (e1ae51d3/fda6948c), catalog survives + demos open (slowly); revert /data/local/tmp/p2-backup/
for instant-open stable UX. Battery is the operational constraint (keep USB-charged; avoid reboots).

**P2.L3 (visible morph) — ✅✅✅ SOLVED 2026-06-23: the MaterialContainerTransform morph now VISIBLY
ANIMATES on OHOS** (Container Transform "View" demo: card morphs frame-by-frame, 13 distinct
intermediate frames vs the prior 2-state snap). Evidence `docs/engine/V3-CATALOG-L3-MORPH-EVIDENCE/`
(morph-01-collapsed → 02/03-expanding → 04-expanded). **The earlier "compositing wall" verdict was
WRONG** — compositing was never the L3 blocker (the catalog window composites fine; full touch-nav works
via the bridge control channel `echo 'x y' > /data/local/tmp/noice_tap`, tap + 4-num drag). **REAL ROOT
CAUSE: `android.animation.ValueAnimator.sDurationScale == 0` in OHOS app processes (animations globally
DISABLED).** AOSP default is 1.0f (framework classes.dex ValueAnimator `<clinit>`), but a caller in
framework **classes4.dex** sets it to 0 at app init; the adapter's `animator_duration_scale` prime
(AppSpawnXInit.preInitSettingsCache value "1") is NOT wired to it (proved: deploying scale="10" via full
boot-regen did nothing). With scale 0, every one-shot animator's scaledDuration = dur×0 = 0 → jumps to
end on frame 1 (SNAP). Continuous/infinite animators (indeterminate ProgressIndicator) only LOOK
animated because they cycle instantly+repeat (burst sizes oscillate wildly, not smooth). **FIX (app-level,
no system rebuild): call `ValueAnimator.setDurationScale(1.0f)` from the catalog's
`io.material.catalog.transition.ContainerTransformConfigurationHelper.configure(...)`** (runs right
before each morph via buildContainerTransform→configure→TransitionManager.beginDelayedTransition). Smali:
insert at configure() top `const/high16 v0, 0x3f800000` + `invoke-static {v0},
Landroid/animation/ValueAnimator;->setDurationScale(F)V`. Optional: patch getEnterDuration/getReturnDuration
to `const-wide/16 v1, 0x2710` (10000ms) to slow the morph for easy filming. **★ CRITICAL DEPLOY LESSON
(cost hours): the catalog APK loads from `/data/app/el1/bundle/public/io.material.catalog/android/base.apk`
(+ `oat/arm/` cache), NOT `/data/app/android/io.material.catalog/base.apk`** — patching the latter has
ZERO effect (4 patch attempts behaved identically because none loaded). Deploy to the el1/bundle path,
`chmod 0644`, clear `oat/arm/*`, relaunch. Verify a patch is live via a marker file written in the
patched method (write to a *pre-created 0666* file — app uid 16371 can WRITE but not CREATE in
/data/local/tmp). APK build: baksmali base.apk classes3.dex → edit → SmaliAssemble (apktool.jar smali +
/tmp/fwktools) → `zip base.apk classes3.dex`. **NET ARC: L1 ✅ (ActivityOptions A→B), L2 ✅ (transition
engages, app survives, demo opens), L3 ✅ (morph VISIBLY ANIMATES).**
**FINAL CLEAN STATE (deployed):** clean catalog `a9df5518` at the el1/bundle path = setDurationScale(1.0f)
fix ONLY, NATURAL Material duration (the 10000ms exaggeration + the FileOutputStream marker were just for
filming; both reverted/removed). System reverted to L1: arb `fda6948c` + boot `095c9f79` (scale=10
undone via /data/local/tmp/pre-durscale/ + reboot), /data/app/android/.../base.apk restored from
/data/local/tmp/base.apk.pre-durpatch, marker file deleted. Real original catalog backup:
/data/local/tmp/realbase.apk.orig. **NOTE on filming the NATURAL morph:** at default ~300ms it animates
but is too fast for `snapshot_display` (which is ~10ms idle but slow during active rendering) — every
burst gives only collapsed→expanded. The animation is PROVEN by the slowed (10000ms) run (13 distinct
intermediate frames, evidence dir); the natural build uses the identical fix on the same path, and the
slow run showed the morph starting from fraction~0 (no cold-pump jump), so it animates at natural speed
too — just unfilmable with this still-capture tool. To re-film, temporarily re-apply the
getEnterDuration/getReturnDuration `const-wide/16 v1, 0x2710` patch.

---

After the BCP OHTouchInjector fix made the Material Catalog clickable (top-level
nav works: grid → category page), going DEEPER crashes: tapping a "Demo" item
(e.g. List View Demo → `io.material.catalog.adaptive.AdaptiveListViewDemoActivity`,
a NEW activity/window) **kills the whole catalog process**. AMS: `startActivity:
creating new Mission for ...AdaptiveListViewDemoActivity` → `Ability on scheduler
died` → `RSSurfaceNode::~RSSurfaceNode`. Screen falls back to launcher (~78k). This
is a CRASH, not a compositing issue.

**ROOT CAUSE (symbolized):** `Fatal signal 7 SIGBUS (BUS_ADRALN) fault addr
0x684b4a`, thread `RenderThread`. Crash is in **libhwui
`android::uirenderer::renderthread::CanvasContext::setupPipelineSurface()`** (elf
vaddr `0x117fd4`), at `blx r3` (vaddr `0x11801a`, the `lr=0xe601801d` return site):
```
11800e ldr r0,[r4,#104]   ; OBJ = this->mSurface/pipeline (offset 104)
118014 ldr r6,[r0]        ; r6 = OBJ->vtable
118016 ldr r3,[r6,#16]    ; r3 = vtable[4]
118018 mov r0,r6
11801a blx r3             ; CRASH: r6/vtable = 0xe60230c5 (UNALIGNED, points into
                          ;  libhwui .text) -> OBJ is corrupt/freed -> r3 garbage
```
i.e. on the **2nd window's pipeline-surface setup**, the surface object at
`CanvasContext+104` is corrupt (vtable = garbage code address) → virtual call →
SIGBUS. Same CLASS as the G3.8 ASurfaceControl_release UAF (multi-window/2nd-surface
RenderThread teardown), different function.

**libhwui symbolization GOTCHA (cost me a wrong patch):** load_bias = the R LOAD
segment runtime base **`0xe5f00000`** (NOT the page-rounded `/proc/maps` r-xp
offset 0xa6000). elf_vaddr = runtime_addr − 0xe5f00000. To patch: file_offset =
elf_vaddr − 0x1000 (exec LOAD: p_offset 0xa6ccc, p_vaddr 0xa7ccc → vaddr−offset =
0x1000). objdump -d addresses are elf vaddr. (I first mis-mapped lr to
`CacheManager::scheduleDestroyContext` 0x11701a, patched it to `bx lr` = md5
9e0df567, REVERTED — wrong function, crash unchanged.)

**FIX NOT LANDED.** Can't no-op `setupPipelineSurface` (essential — sets up
rendering). Options: (a) targeted binary-patch GUARD before the `blx r3` (skip the
virtual call if r6 unaligned/bad → graceful fail) — needs a thumb trampoline/code
cave, intricate; (b) fix the surface-object lifetime (real bug — why CanvasContext+104
is corrupt on the 2nd window) — deep hwui; (c) rebuild libhwui — BLOCKED locally
(bionic_compat wall, needs [build-host]). This is the **multi-window wall**, historically the
project's hardest area. Device reverted to clean libhwui **8b8f84ec**; 1st-level
(noice full UI, catalog top-level nav) works. Patched-but-wrong libhwui banked
`$HOME/hwui-l2fix/`. Cross-ref [[material-catalog-metadata-fix]],
[[noice-g38-renders-stable]] (G3.8 ASurfaceControl UAF), [[westlake-wall-map]].

## ✅ REAL ROOT CAUSE FOUND 2026-06-23 (the "multi-window wall" framing below was WRONG — it is createHardwareBitmap)

**The multi-window/2nd-surface theory (everything below) is REFUTED by data.** A diagnostic
`fprintf` in `CanvasContext::setupPipelineSurface` (logging this/mRenderPipeline/mRenderPipeline-vtbl/
mNativeSurface/window) + `fprintf` in the bridge `getOhNativeWindow` (logging session→aospAnw) PROVED:
- `mRenderPipeline` vtable is ALWAYS valid (constant rpVtbl) — it is NEVER corrupt. The corrupt thing
  is the **native window pointer** (`mNativeSurface->mWindow`).
- The adapter's multi-window path WORKS PERFECTLY: MainActivity = session 10 → window 0xf7541da0;
  the demo Activity = session 12 (routed as SUB_WINDOW parented to 10) → `getOhNativeWindow` FRESH-WRAP
  returns a clean aligned 0xe705f7a0, and its CanvasContext gets exactly that. Both render fine.
- The crash is a **3rd CanvasContext** whose window = a FIXED libhwui code address (deterministic across
  children — appspawn preforks so libs share ASLR; same elf offset 0x123134 every boot). Symbolized via
  load_bias (= runtime SkiaOpenGLPipeline-vtable 0x..6da4 − elf 0x136da4): the value resolves EXACTLY to
  `RenderProxy::setSwapBehavior(...)::$_2 std::function::destroy()` thunk. That value never came from the
  adapter (getOhNativeWindow only ever returned the 2 clean windows).

**ROOT CAUSE:** `android_view_ThreadedRenderer_createHardwareBitmapFromRenderNode`
(`HardwareRenderer.nCreateHardwareBitmap (JII)Landroid/graphics/Bitmap;`, in
`aosp/frameworks/base/libs/hwui/jni/android_graphics_HardwareRenderer.cpp`):
`ANativeWindow* window;` is **uninitialized**, then `AImageReader_getWindow(rawReader,&window)` —
but **OH has no functional AImageReader (libmediandk ImageReader/BufferItemConsumer)** so `window`
stays = stack garbage (the `&setSwapBehavior::$_2 destroy` thunk left on the stack by the
`proxy.setSwapBehavior(kSwap_discardBuffer)` call on the very next line). Then `proxy.setSurface(window)`
→ `setupPipelineSurface` derefs garbage → **SIGBUS BUS_ADRALN** on the RenderThread → whole process dies.
The Material Catalog calls `ThreadedRenderer.createHardwareBitmap()` when a demo Activity opens
(shared-element / RenderEffect snapshot). Same crash hits ANY app that calls createHardwareBitmap.

**FIX (local, libhwui rebuild — builds locally now, bionic_compat wall was stale):** the function
ALREADY discards its result (returns a null Bitmap at the bottom, `sk_sp<Bitmap>(nullptr) /* M133 */`),
so the whole AImageReader+RenderProxy+syncAndDrawFrame path is dead work that can only crash →
**`return nullptr;` right after the width/height validation.** createHardwareBitmap is contractually
nullable (callers fall back to software). Build flags use `-Wno-error` so the unreachable code is fine.
Cross-ref [[material-catalog-metadata-fix]]. **LESSON: a deterministic "corrupt pointer" that lands on a
named code symbol is an UNINITIALIZED-stack read of a leftover, not heap corruption — symbolize it
(load_bias from a known runtime vtable) and the leftover names the culprit. The whole "multi-window wall"
framing below cost weeks and was wrong; the adapter multi-window path was fine all along.**

---
(HISTORICAL — REFUTED multi-window/EGL theory below, kept for the symbolization technique)

**TRAMPOLINE GUARD ATTEMPTED 2026-06-22 (partial — SIGBUS fixed, deeper EGL wall hit, reverted).**
No zero code-cave exists in libhwui's packed exec seg, so: no-op'd `scheduleDestroyContext`
entry (`bx lr` @0x116f54) to free ITS BODY as a cave, then redirected `setupPipelineSurface`
0x118018 (`mov r0,r6; blx r3`) via `b.w` to a trampoline there:
`tst.w r6,#3; bne skip; mov r0,r6; blx r3; b back; skip: movs r0,#0; back: b.w 0x11801c` —
skip the virtual call when the vtable r6 is unaligned (the corruption signature), return 0
(the null path the fn handles). Encodings via clang integrated-as (`tst.w r6,#3`=f0160f03 etc.);
B.W T4 in Python; all regions objdump-verified. Guarded libhwui md5 **`00e307c8`** (banked
`$HOME/hwui-l2fix/libhwui_guard.00e307c8.so`). **RESULT: SIGBUS ELIMINATED** — demo
Activity now reaches AMS `ForegroundLifecycle AdaptiveListViewDemoActivity`. **BUT aborts deeper:**
`ASSERT FAILED [skia] mEglSurface == EGL_NO_SURFACE: drawRenderNode called on a context with no
surface` — the multi-window **EGL-surface wall**. Guard's skip-path returns NULL surface → 2nd
window never gets an EGL surface → draw aborts. So guarding is the WRONG fix (skip→no surface);
real fix = the surface-object lifetime (why CanvasContext+104 is corrupt on the 2nd window) OR
the EGL re-create path (operator-gated libhwui, needs [build-host]). Reverted to clean 8b8f84ec.
LESSON: libhwui has no zero code-caves (packed) — repurpose a no-op'd function's body as the cave.
