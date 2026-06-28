---
name: Westlake Binder Pivot (V2 substrate + daemons + M7-Step2 breakthrough)
description: 2026-05-12 strategic decision (Binder service boundary substitution) refreshed 2026-05-14 for V2 substrate, M5/M6 daemons functional, M7-Step2 reaches MainActivity.onCreate USER BODY for the first time. See docs/engine/BINDER_PIVOT_DESIGN_V2.md
type: project
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
status: PARTIALLY-SUPERSEDED — OHOS path superseded by V3 (2026-05-16); Android-phone Phase-1 path AUTHORITATIVE
---

> **PARTIAL SUPERSESSION 2026-05-16:** the **OHOS path** content in this
> doc (M9-M13 milestones, Phase-2 OHOS roadmap, Westlake's own libbinder
> on OHOS, M5/M6 OHOS-target daemons) is SUPERSEDED by
> `docs/engine/V3-ARCHITECTURE.md` + `project_v3_hbc_reuse_direction.md`.
> CR61's "no libipc / no samgr" prohibition is amended for V3 by
> `docs/engine/CR61_1_AMENDMENT_LIBIPC_VIA_HBC.md`. Under V3 Westlake
> reuses [build-host]'s `liboh_adapter_bridge.so` (which links real OH ipc_core
> + samgr_proxy innerAPI variants) instead of cross-compiling our own
> libbinder for OHOS.
>
> The **Android-phone Phase-1 path** content in this doc (V2 substrate
> on cfb7c9e3, in-process Option 3, M5 audio daemon AUDIBLE, M6 surface
> daemon at 60 Hz, M7-Step2 MainActivity.onCreate USER BODY, CR59 fix,
> 14/14 regression) is AUTHORITATIVE and UNCHANGED by V3. V2 substrate
> classes (WestlakeActivity, WestlakeApplication, etc.) stay in place
> for phone path. `aosp-libbinder-port/out/{musl,bionic}/` builds are
> kept. M5 + M6 daemons for Android phone are kept.
>
> Content below preserved verbatim. Read `project_v3_hbc_reuse_
> direction.md` for the V3 OHOS path.

Westlake architectural pivot — adopted 2026-05-12 after a week of additive
shimming failed to converge on noice. Codex 2nd-opinion confirmed the
design. 2026-05-13 V2 redesign (BINDER_PIVOT_DESIGN_V2.md) reframed the
in-process substitution boundary at `Activity.attach` /
`Application.attach` / `Resources` *API surface* via classpath shadow,
replacing V1's reflective field-plant chain.

**Why:** the engine's substitution layer was at framework-class level, then
reflectively planting state on framework internals. Each app surfaced ~5
new gaps. Both patterns were unsustainable. The right substitution points
are (a) Binder service boundary — single uniform AOSP-defined interface,
AOSP's same-process `Stub.asInterface` optimization makes Java service
calls zero-cost; and (b) classpath-shadowed Activity/Application/Resources
— our class wins resolution against framework.jar, no Unsafe, no
setAccessible, no plant.

**How to apply:** when an NPE or missing-method shows up during app launch:
- (a) if it's a binder service method, implement the missing method
  behind Binder (AOSP-shaped) — NOT a class-level shim
- (b) if it's a method on Activity/Application/Resources/Window/etc.,
  implement it on the Westlake-shadowed class — NOT via reflection on
  framework internals
- (c) `WestlakeFragmentLifecycle`-style bypasses are explicitly forbidden
- (d) per-app branches forbidden; macro-shim contract enforced via
  `feedback_macro_shim_contract.md`

**Architecture:** dalvikvm + unmodified AOSP framework.jar + Westlake-owned
libbinder.so + servicemanager + Java service impls (extending AOSP
IXxxService.Stub) in-process + classpath-shadowed framework substrate
(WestlakeActivity / WestlakeApplication / thin WestlakeResources +
ResourceArscParser / Window+PhoneWindow+DecorView/WindowManagerImpl) +
native daemons (audio, surface). Kernel binder module shipped with OHOS.

**Validation:** Phase 1 = Android sandbox (no OHOS dependency). Phase 2 =
OHOS port (M9-M13). Side-by-side comparison with native noice/McD as
ground truth.

## Authoritative docs (tree-side; do NOT edit if parallel agent is in flight)

- `docs/engine/BINDER_PIVOT_DESIGN_V2.md` — V2 in-process Java boundary
  design (CR28-architect 2026-05-13; supersedes V1 §3.2/§3.3/§3.4/§3.5)
- `docs/engine/MIGRATION_FROM_V1.md` — V2 file-by-file keep/touch/delete
- `docs/engine/BINDER_PIVOT_DESIGN.md` — V1 design + rationale (historical)
- `docs/engine/BINDER_PIVOT_MILESTONES.md` — milestones M1-M13 + acceptance
- `docs/engine/PHASE_1_STATUS.md` — current progress (14/14 regression PASS)
- `docs/engine/M4_DISCOVERY.md` — discovery transcripts (6500+ LOC; §52 has V2)
- `docs/engine/AGENT_SWARM_PLAYBOOK.md` — coordination model + anti-patterns
- `docs/engine/CR41_PHASE2_OHOS_ROADMAP.md` — Phase 2 costing (13 person-days)
- `aosp-libbinder-port/README.md` — build/test/sandbox layout

## Progress (cumulative through 2026-05-14)

### V1 binder substrate (LANDED, regression-stable)

- **C1-C5 cleanup done** (2026-05-06..2026-05-12). WestlakeFragmentLifecycle
  (3087 LOC), DexLambdaScanner (~600 LOC), MCD constants in fragment shim
  (~340 LOC) deleted; net ~-4000 LOC.
- **M1, M2, M3, M3-finish, M3+, M3++ done**. Stub.asInterface same-process
  elision active via `nativeGetLocalService`.
- **M4 services — all 6 PRODUCTION-SELF-SUFFICIENT** (CR17): M4-power
  (IPowerManager 71m), M4a (IActivityManager 267m), M4b (IWindowManager
  154m), M4c (IPackageManager 223m binder), M4d (IDisplayManager 64m),
  M4e (INotificationManager 167m + IInputMethodManager 37m). Each uses
  PermissionEnforcer-bypass-ctor in production code (no test-only primer
  dependency).
- **M4-PRE family done** (PRE through PRE10). PRE12/13/14 marked
  `[SUPERSEDED-V2]` (replaced by V2 substrate).
- **M5-PRE done.** AudioSystem JNI 105 natives statically linked. Total
  native methods baked into dalvikvm: 167 (= 6 MessageQueue + 56
  AssetManager + 105 AudioSystem; plus 7 ServiceManager + 2 HelloBinder).
- **D2 done.** `scripts/binder-pivot-regression.sh` runs full smoke suite.

### V2 substrate (LANDED 2026-05-13)

User-level observation: M4-PRE12/13/14 + CR23-fix + CR25 were all piecemeal
substitutions inside `system_server`'s ~200-field cold-boot graph — an
architecturally unbounded path. CR28-architect work
(`BINDER_PIVOT_DESIGN_V2.md`) reframed the substitution boundary at
`Activity.attach` / `Application.attach` / `Resources` *API surface* via
`framework_duplicates.txt` classpath-shadowing. V2-Step1..Step8-fix
implemented + verified it.

- **V2-Step1**: API surface — `WESTLAKE_ACTIVITY_API.md` (~500 LOC).
  Classified 324 Activity methods into Implement (87) / fail-loud (178) /
  no-op (14).
- **V2-Step2**: WACT — `shim/java/android/app/Activity.java` (1083→1209
  LOC). Classpath-shadow extends ContextThemeWrapper; 23 fields + ~294
  methods + 6-arg `attach(Context, Application, Intent, ComponentName,
  Window, Instrumentation)` overload. `framework_duplicates.txt` L83
  comment-out so shim wins resolution.
- **V2-Step3**: WAPP — `shim/java/android/app/Application.java` (100→~430
  LOC). Classpath-shadow extends ContextWrapper; AOSP API 30 surface.
- **V2-Step4**: WRES2 — thin `WestlakeResources` (545→332 LOC) +
  `ResourceArscParser.java` (419 LOC NEW) + `WestlakeAssetManager.java`
  (190 LOC NEW). **Deletes V1 plant chain**: M4-PRE12 (~85 LOC),
  M4-PRE13 (~36 LOC), M4-PRE14 (~48 LOC), buildReflective+
  unsafeAllocateInstance (~75 LOC). **Zero Unsafe; zero setAccessible.**
- **V2-Step5**: WWIN — `Window` (876→423 LOC, -51.7%) + PhoneWindow (44
  LOC NEW) + DecorView (22) + WindowControllerCallback (13) +
  WindowManagerImpl (87). Removes 450 LOC of per-app McDonalds shell.
- **V2-Step6**: `WestlakeActivityThread.attachActivity` rewire (-432
  LOC). Direct 6-arg `activity.attach(...)` call. No reflection / no
  try/catch / no field-set fallback.
- **V2-Step7**: Plant residue audit (`V2_STEP7_PLANT_RESIDUE_AUDIT.md`,
  ~400 LOC) + 1 safe deletion (`wireStandaloneActivityResources` + 5
  helpers, 294 LOC across 6 methods).
- **V2-Step8-fix**: Regression FAIL→PASS — root-caused as stale dalvikvm
  artifact path (May 2 build), NOT V2 plumbing. Sync script fixed.
- **CR30-B**: Extended classpath-shadow to `android/content/res/Resources`
  itself (V2 §3.4 decision 11-B, Option 11-B "WestlakeResources owns
  Resources surface, no AOSP machinery").
- **V2-Probe (V2_PROBE_RESULTS.md)**: noice/McD progression analysis post-
  V2 substrate. Honest gap accounting included.

### M5 audio daemon (functional, AUDIBLE)

- Step 1: skeleton + daemon process pattern (2026-05-13).
- Step 2: 17 Tier-1 transactions + AAudio dlopen backend + 7/7 audio_smoke
  PASS.
- Step 3: libbinder libc++ ABI namespace rebuilt to `__1` (AAudio dlopen
  now succeeds, 14/14 regression PASS).
- Step 4: AF descriptor flipped to `IAudioFlingerService` matching Android
  12+ AIDL wire — descriptor-mismatch error eliminated. AAudio-reentry
  hazard exposed.
- **Step 5: Pivoted AAudio out of daemon address space**. Spawn
  `audio_helper` child process via fork+exec with `LD_LIBRARY_PATH`
  scrubbed; its libaudioclient binds to platform `/dev/binder` SM (real
  audioserver). **Audible 440 Hz tone produced** on cfb7c9e3 speaker;
  `m5step2-smoke.sh` 7/7 PASS end-to-end. Phase 1.5 (cblk shared-memory
  ring) deferred.

### M6 surface daemon (functional, vsync 60 Hz)

- Step 1-4: skeleton + ISurfaceComposer/IGraphicBufferProducer Tier-1.
- Step 5: DLST FIFO to host APK + memfd buffers.
- **Step 6: CR35 §7 A15 AIDL-drift mitigation**. Descriptor tolerance +
  84-code A15→A11 translation shim. `m6step5-smoke.sh` 8/8 PASS on phone.

### M7 integration (noice — BREAKTHROUGH)

- **Step 1**: `scripts/run-noice-westlake.sh` + `NoiceLauncher.dex` —
  4/7 PASS at first run.
- **Step 2 (BREAKTHROUGH 2026-05-13)**: production
  `Instrumentation.callActivityOnCreate` path reached MainActivity.onCreate
  USER CODE BODY for the first time. `MainActivity$settingsRepository$2`
  (Hilt lazy delegate) is user-package code, not framework code. New
  blocker observed: ContextWrapper.getApplicationContext() NPE inside
  Hilt's `dagger.Lazy<T>` resolution. CR56 + CR58 wired Activity-side
  plumbing but did NOT close the NPE (artifact 20260514_160506 still
  showed the same 8-frame NPE stack).
- **CR59 (LANDED 2026-05-14)** — root-caused the Hilt-lazy NPE to
  **Application.mBase=null**, not Activity-side. The lazy delegate's
  receiver was the Application (via `mainActivity.getApplication()`),
  not the Activity. Two distinct upstream bugs converged:
  (a) MiniServer.get() auto-init returned a placeholder bare-Application
  with `mBase=null` (shim ctor calls `super(null)`), and
  WAT.performLaunchActivityImpl accepted it, skipping the
  makeApplication branch;
  (b) WAT.attachApplicationBaseContext called `app.attachBaseContext(base)`
  directly — a protected ContextWrapper method, cross-package call from
  WAT silently failed; the reflective fallback used `getDeclaredMethod()`
  which doesn't find inherited methods, also silently failed.
  Fix: use shim Application's existing package-private `attach(Context)`
  helper, and detect the placeholder Application by exact class name
  (`android.app.Application`) and discard it so makeApplication runs.
  Anti-drift compliant: -1 net setAccessible call, ZERO new
  Unsafe/setAccessible/new-WestlakeContextImpl-methods/per-app branches.
  Result: noice MainActivity.onCreate completes cleanly (zero `[NPE]`
  frames in dalvikvm log); McD SplashActivity SIG2 flipped FAIL→PASS;
  14/14 regression intact. Next gates are launcher-side parameter
  plumbing (drive RESUME for Fragment lifecycle; pass appCls via
  `forceMakeApplicationForNextLaunch` for McDMarketApplication.onCreate).
  See `docs/engine/CR59_REPORT.md`.

Discovery harness reaches PHASE G4 (`MainActivity.onCreate` body executing,
NPE on `Configuration.setTo(null)`). Production launch (M7-Step2) goes
STRICTLY DEEPER — into user-package Hilt lazy delegate. McD harness reaches
PHASE G3 (Hilt_SplashActivity.attach throws); McD Application.onCreate
ran further than ever (NewRelicImpl). Real Binder transactions into M4
services: ZERO (all 7 service lookups go through `nativeGetLocalService`
in-process JavaBBinder optimization).

### M8 integration (McDonald's)

- **Step 1**: `scripts/run-mcd-westlake.sh` — 2/2/3 PASS/FAIL/PEND
  (most signals PEND on M5/M6/V2-§8.4 prereqs that have since landed).
- **Step 2 in flight at handoff time** — symmetric McD production launcher
  driving `Hilt_SplashActivity` via `Instrumentation.callActivityOnCreate`.

### Spike + scoping CRs (read-only, architect)

- CR21 — M5/M6 architectural scoping.
- CR33 — M6 memfd feasibility spike (PASS).
- CR34 — M5 AAudio backend spike (PASS; 6.5-day timeline holds).
- CR35 — M6 AIDL transaction discovery (84 codes catalogued; A11→A15 drift).
- CR37 — M5 AIDL transaction discovery.
- CR38 — M7/M8 integration pre-scoping (7 acceptance signals).
- CR41 — Phase 2 OHOS roadmap.
- CR42 — View ctor audit.
- CR43, CR46 — latest reports.

### Codex review history

- **Codex #1** (CR1-CR10 era) — Tier 1/2/3 findings, all resolved CR1-CR10.
- **Codex #2** — service self-sufficiency; resolved by CR17.
- **Codex #3** — V2 substrate review. 3 HIGH (CR47 Application reflection,
  CR48 BinaryXmlParser wiring [later found overscoped], CR49 size caps) +
  2 MED (CR50, CR51 pending).
- **Codex #4** in flight at handoff — reviewing inline fixes.

### Inline fixes during budget block

- **CR47**: fixed `Application.mCallbacks` → `mActivityLifecycleCallbacks`
  reflection mismatch.
- **CR48**: wired `BinaryXmlParser` into `WestlakeResources.getXml/
  getLayout` (XML infra existed since B.5/B.7-B.11).
- **CR49**: `ResourceArscParser` size caps.
- **CR53/54**: discovery harness extended with internal-field dump.
- **CR55**: `LifecycleRegistry` observer-map prime in `Activity.attach`
  (regression PASS but didn't unblock G4 in production; M7-Step2 bypasses
  via Instrumentation path).

## Cumulative source delta (post-V2-Step8-fix)

- `WestlakeLauncher.java`: 22,983 → **12,403** LOC = **-46%**
- `aosp-shim.dex`: ~1.58 MB → **1.45 MB** (~1,453,196 bytes)
- dalvikvm bionic-arm64: 26 MB pre-M3 → ~28 MB now (167 native methods +
  binder JNI + AudioSystem)
- ALL `sun.misc.Unsafe` / `Field.setAccessible` / M4-PRE plant code
  ELIMINATED from `WestlakeResources`
- 4 fully classpath-shadowed framework classes via `framework_duplicates.txt`:
  Activity, Application, Window, Resources

## What works (regression-stable)

- 14/14 regression suite PASS (`scripts/binder-pivot-regression.sh --full`)
- HelloBinder.dex (M3), AsInterfaceTest.dex (M3++), PowerServiceTest,
  ActivityServiceTest, WindowServiceTest, DisplayServiceTest,
  NotificationServiceTest, InputMethodServiceTest, SystemServiceRouteTest,
  PackageServiceTest, NoiceDiscoverWrapper.dex (PHASE G4 reach)
- M5 audio daemon: audible 440 Hz tone end-to-end (`m5step2-smoke.sh` 7/7)
- M6 surface daemon: vsync 60 Hz + memfd + A15 wire compat
  (`m6step5-smoke.sh` 8/8)
- M7-Step2 production launch reaches USER CODE body of MainActivity.onCreate
- **CR59 (2026-05-14)**: MainActivity.onCreate / SplashActivity.onCreate run
  cleanly for BOTH noice and McD; zero `[NPE]` frames in dalvikvm log
  (root cause: Application.mBase=null fixed via plumbing in WAT +
  MiniServer placeholder discard)
- `bcp-sigbus-repro.sh` all 3 modes PASS (PF-arch-053 closed; PF-arch-054
  sidestepped via PF-arch-055 lambda rewrite)

## What doesn't work yet

- **M7 / M8 final acceptance** — CR56 + CR59 close the in-body NPE chain.
  CR56's setAttachedApplication is necessary but not sufficient; CR59
  fixes the Application.mBase=null root cause (MiniServer placeholder +
  cross-package protected method invoke). Remaining gates: launcher-side
  parameter plumbing — drive RESUME for Fragment lifecycle (noice S2);
  pass appCls via `forceMakeApplicationForNextLaunch` so user's real
  Application is built instead of bare Application (McD SIG1
  McDMarketApplication.onCreate). 1-3 person-days each.
- **Real Binder transactions into M4 services** — all current service
  lookups go through local-elision. Once apps exercise actual transact
  paths, Tier-1 method bodies will need real impls (fail-loud surfaces them
  loud).
- **PF-arch-054 underlying substrate bug** remains open (CR26 sidestepped
  via PF-arch-055 lambda rewrite; `env->functions` corruption root cause
  unknown). Not blocking, documented in PHASE_1_STATUS.md headline.
- **OHOS Phase 2 hardware** — no OHOS standard-system device in lab as of
  2026-05-13. Recommend ordering rk3568 dev board (~2 week lead) during
  Phase 1 final integration.

## Phase 2 OHOS roadmap (CR41, costed)

| Milestone | Best | Expected | Worst |
|---|---|---|---|
| M9 (binder kernel verify) | 0.5 | 1.0 | 2.0 |
| M10 (libbinder/SM musl rebuild on OHOS sysroot) | 1.0 | 1.5 | 2.0 |
| M11 (audio daemon AAudio→OHOS AudioRenderer) | 2.5 | 2.75 | 3.0 |
| M12 (surface daemon DLST→OHOS XComponent) | 4.0 | 4.25 | 4.5 |
| M13 (noice on OHOS e2e) | 3.5 | 4.0 | 4.5 |
| Risk reserve | 1.2 | 1.5 | 2.0 |
| **Total Phase 2** | **~13** | **~15** | **~18** person-days |

**M9 is essentially trivial** because OHOS kernel 5.10 already ships
`CONFIG_ANDROID_BINDER_IPC=y` and `CONFIG_ANDROID_BINDER_DEVICES=
"binder,hwbinder,vndbinder"`. M9 is "verify device nodes exist + grant
orchestrator access." V1 milestones-doc estimated 3-5 days with OHOS
team or 2-3 weeks without — both wrong.

**Calendar**: ~7 days with 2 engineers in parallel (M11+M12), ~13-15 days
sequential. Bottleneck is hardware procurement (rk3568, ~2 week lead).

## Anti-drift contract (mandatory)

Every Builder brief now includes `feedback_macro_shim_contract.md` verbatim.
Forbidden patterns: `Unsafe.allocateInstance` on framework.jar classes,
`Field.setAccessible` on framework internals, "planting" state on
ResourcesImpl/AssetManager/Configuration, per-app branches, adding new
methods to `WestlakeContextImpl` (CR22 freeze). Self-audit gate runs before
reporting complete.

Permitted: implementing API methods on classes WE own (V2 shadows) with
(a) AOSP-default verbatim, (b) safe-primitive, or (c) delegation bodies.
`ServiceMethodMissing.fail(...)` for genuinely-unimplementable methods.

## Acceptance gate for Phase 2 entry

M7 (noice e2e green) + M8 (McDonald's regression green). Both required;
no shortcut. M5 + M6 daemons functional (LANDED). CR56 +
getApplicationContext + downstream in-body NPEs (in flight). Real Binder
transaction discovery (gates Tier-1 method body work).

## OHOS scope

Standard-system (Linux-based) only. LiteOS-A is out of scope.

## Operational notes

- **Budget blocks happen** (6:50pm reset cycle). Inline fixes during a
  block are documented to attribute later (CR47-49 pattern). Anti-drift
  contract is the line-of-defense against speculative shimming under time
  pressure.
- **Phone fragility persists** (cfb7c9e3 disconnects mid-run). Always
  assume the phone may be unreachable; deferred-on-device-test plan ready.
- **vndservicemanager rebind race** is a pre-existing infra issue
  (documented CR7/CR19 partial mitigation). `lib-boot.sh` provides
  `wait_for_vndservicemanager_dead` + `stop_vndservicemanager_synchronously`.
- **Phase 1 sprint cadence**: started 2026-05-12 with 61 milestones in one
  day; extended through 2026-05-13 (V2 substrate + daemons + M7/M8 +
  spikes); 121 tasks complete at handoff time. Discovery harness peak:
  PHASE G4 (deeper than ever pre-V2). Production path via Instrumentation
  is the real test — reaches USER CODE.
