---
name: v3-mcd-2026-05-26
description: "2026-05-26 — McD passes scheduleTransaction for first time and reaches Activity init. Wall family shifted from native SEGV (boot-framework.oat +0xa3e636 DexLoadReporter NPE) to Java NPE in R8-Kotlin <clinit> code. WS-H landed (libart broad bypass, 32-bit ARM local cross-compile infra permanent). 2 new walls: androidx.startup.AppInitializer.b null, androidx.savedstate.SavedStateRegistryController.a null. Same family as 2026-05-23 J.2.b. McD pid 31764 lifespan 10.4s; exits OnRemoteDied (no SIGSEGV)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Fact

2026-05-26 was the day McD's wall family shifted from native AOT-SEGV to Java NPE. Three architectural fixes landed (Tier 3 yesterday + Fix I + WS-H today) combined to push McD past `bindApplication → scheduleTransaction → LaunchActivity scheduled → SplashActivity ctor invoked` for first time.

## Substrate state (end of 2026-05-26)

| File | md5 | Source |
|---|---|---|
| libart.so | **`ab3bfbd1`** | **WS-H + J.2.b reapplied — local 32-bit ARM cross-compile, broad signature-drift bypass + CanMethodUseNterp gate** |
| adapter-runtime-bcp.jar | `6d00bf5b` | Fix I (CoreComponentFactory `appComponentFactory = null` in PIB + AppSchedulerBridge — 2-line edit) |
| oh-adapter-framework.jar | `ecfb5ac3` | untouched, drift-immune |
| appspawn-x | `3abe3bde` | Scope C BCP reorder |
| boot-framework.oat | `a8fa38e6` | Fix I local regen |
| boot.oat | `dad24139` | Fix I local regen |
| boot-adapter-runtime-bcp.oat | `553c2e9a` | Fix I local regen |

## Key WS-H architectural win

Pulled [build-host]'s 32-bit ARM libart build infrastructure ONCE (306MB zstd bundle: clang + sysroot + art source + aosp_lib + .o cache + bionic_compat). Added `build-libart-32arm` Makefile target to `$HOME/art-universal-build/`. **Local 32-bit ARM libart cross-compile is now PERMANENT** — future libart edits fully local.

WS-H patch itself: 17-line gate in `class_linker.cc::HasSameSignatureWithDifferentClassLoaders` returning `true` for app-classloader klasses without strict return-type drift check. Mirrors J.2.b 2026-05-23 archetype but at signature-drift site, not nterp-gate site.

## McD progression observed (pid 31764, lifespan 10.4s)

```
✓ spawn succeeds
✓ AppSpawnXInit.initChild runs
✓ classes 1..15+ dex registered
✓ McDMarketApplication className resolved
✓ ApplicationInfo populated (pkg/uid/process/sourceDir/dataDir/nativeLibraryDir all set; flags=0x20802044)
✓ [B43-BIND] resolved ApplicationInfo
✓ [B43-BIND] appInfo enriched
✓ [B47-SLA] BEFORE scheduleTransaction
✓ [B47-SLA] AFTER scheduleTransaction OK  ← NEW PROGRESSION POINT
✓ installContentProviders begins
✗ WALL-1: androidx.startup.AppInitializer.g(Class) NPE on b (HashSet field null)
  ↓ bridge silently catches; schedules LaunchActivity anyway (G2 silent-fail anti-pattern still present)
✓ +289ms: Instrumentation.newActivity → SplashActivity.<init> chain
✗ WALL-2: androidx.savedstate.SavedStateRegistryController.e(Bundle) NPE on a (SavedStateRegistryImpl field null)
✗ OnRemoteDied (no SIGSEGV, no cppcrash)
```

Both walls are inside McD's bundled-AAR R8-stripped classes where Kotlin-generated `<clinit>` code silently fails upstream and leaves fields null. Same family as 2026-05-23 J.2.b walls (`CombinedContext.g`, `JobSupport.plus`).

## What's NOT needed (per fresh 2026-05-26 diag of +0xa3e636 on current substrate)

- More ApplicationInfo backup-fill in PIB or AppSchedulerBridge (info IS fully populated)
- Per-APK smali NOP patches (rejected by engine-not-patcher per `feedback_no_per_app_hacks`)
- Netflix fix (separate wall family — Netflix hits `+0xa3ee42` ConfigurationController.updateLocaleListFromAppContext NPE, different cause)

## What MAY be needed

- J.2.b libart patch verification in WS-H libart `bbbd7ed4`. If absent, add CanMethodUseNterp gate (returns false for non-boot CL methods) on top of WS-H. Same archetype as 2026-05-23.
- OR novel libart approach to handle Kotlin `<clinit>` field-null pattern gracefully (e.g., re-attempt class init on NPE, or skip silent failures)

## J.2.b APPLIED LATE-DAY (2026-05-26 + commit `4cfb9c50c0b`) — necessary-but-not-sufficient

**Verified J.2.b absent in WS-H libart `bbbd7ed4`** (only WSH-RELAX + Fix I.§5 + Fix H-RANGE markers present).

**Applied `[FIX-J2B-2026-05-26]` gate** in `CanMethodUseNterp` at `nterp_helpers.cc`. Rebuilt locally via WS-H pipeline, new libart `ab3bfbd1` deployed.

**HW: PASS** (regression-clean). **McD: UNCHANGED** — same two NPE walls (AppInitializer.b, SavedStateRegistryController.a) at same point in execution.

**Critical insight:** J.2.b reroutes app code through type-checking interpreter rather than nterp. But McD's walls are **Java field NPEs from FAILED `<clinit>`** — the static fields were never set in the first place. The interpreter gate can't help when the upstream `<clinit>` silently failed before any method execution.

**The actual failure mechanism (per JVM spec analysis):** `<clinit>` is supposed to either complete successfully, throw ExceptionInInitializerError, OR mark the class as "erroneous" (subsequent access throws NoClassDefFoundError). McD sees plain NPE on field access, meaning **`<clinit>` DID complete BUT didn't set the field**. Only possible if:
- Field assignment was in a try/catch that swallowed
- R8 optimizer reordered statements to access-before-init
- libart's `<clinit>` execution silently swallows some exception family (LinkageError from a different verifier check?)
- WS-H gates ONE signature check (return-type drift) but other checks (arg-type drift, throws drift) may still fire and get swallowed during class init

## Next architectural question (DEEP — multi-day)

What silently swallows the exception that causes `<clinit>` to leave fields null? Candidates:
- libart's class init implementation (`class_linker.cc` InitializeClass*)
- AOSP-side ExceptionInInitializerError swallow path in handleBindApplication
- A different verifier check not gated by WS-H

Diagnosing this would require:
- Adding LOG markers around `<clinit>` execution in libart
- Tracing what exception (if any) fires inside AppInitializer's `<clinit>`
- Understanding R8's bytecode patterns vs AOSP-14 verifier expectations

This is multi-day libart class-init machinery work. Not in scope for a single-session pursuit.

## CRITICAL UPDATE (2026-05-26 end-of-day) — `<clinit>` hypothesis DISPROVEN; REAL root cause is VTABLE CORRUPTION

Per `docs/engine/V3-A1-A2-RESOLUTION-2026-05-26.md` (commit `671fbb525ff`):

A1 diagnostic agent instrumented libart's class init for McD. Findings:
- **108 CLINIT-ENTER + 38 CLINIT-INVOKE** for McD's process
- **ALL 38 returned `post_exc=N`** — no exception fired during any `<clinit>`
- **ZERO exceptions wrapped/swallowed** at the class init layer
- `<clinit>` completes NORMALLY with whatever values the code computed (which apparently isn't enough)

The previous "silent swallow" theory was wrong.

### Actual root cause: VTABLE INDEX CORRUPTION

The diagnostic uncovered: WS-H's broad signature-drift bypass enables **vtable misalignment** between McD's R8-stripped classes and the boot-framework parent classes.

**Smoking gun** from CLINIT-EXC enumeration: 560 WSH-RELAX bypasses fired, 25 of which were FUNDAMENTAL different-name drifts at the same vtable slot. Example:
- `AppInitializer.g(Class) → boolean` at vtable slot N in McD's class
- `Object.clone() → Object` at same vtable slot N in parent Object class

These aren't "renamed-but-equivalent" methods — they're completely different methods at the same slot. McD's invoke-virtual to `.g(Class)` actually dispatches against `clone()` (or whatever happens to be at slot N), the wrong-method-wrong-args call returns garbage, the caller treats garbage as field reference → NPE in downstream `.b` access.

### The fundamental trade-off (Phase 1B trial confirmed)

Tried tightening WSH-RELAX to "same-name only" (FIX-A1-STRICT, libart `42776df9`):
- Closed AppInitializer.b + SavedStateRegistry.a NPE family ✓
- BUT immediately exposed original `boot-framework.oat+0xa3e636` AOT-SEGV (kotlinx <clinit> fails because LinkageError on JobSupportKt now fires) ✗
- Net wall progression: zero. McD 5 contract criteria: 0/5.
- Rolled back to libart `ab3bfbd1`.

**There is NO tunable WSH-RELAX setting that works.** Broad → field-null-NPE family. Narrow → AOT-SEGV family. Off → bind never starts.

### Real fix space

This is multi-week substrate-engineering work in:
- **dex2oat vtable emission** for the BCP — ensure vtable slot indices match what R8-stripped Kotlin code expects when McD's classes link against boot-framework classes
- **OR libart `Class::PopulateEmbeddedVTable`** — match vtable slots by method signature equivalence (handling R8 renaming gracefully)
- **OR fundamentally different McD-on-OHOS architecture** — e.g., link McD's classes against AOSP-14 BCP, not OHOS-shimmed BCP

### Diag-Vtable infrastructure ready (2026-05-26 end-of-day, commit `c3300d3e1b4`)

Vtable layout instrumentation built into libart at `LinkMethodsHelper<kPointerSize>::LinkMethods` (AOSP-15-style, `class_linker.cc:8795`):
- `[VT-LAYOUT]` markers dump klass + super vtable slot-by-slot
- `[VT-PARENT]` markers dump parent class vtable at same indices
- `[VT-BYPASS-DIFFNAME]` markers fire when WSH-RELAX skips a fundamental-different-name check
- Filtered to `AppInitializer / SavedStateRegistry / startup / Startup` descriptor matches
- `fprintf(stderr) + fflush` (LOG(WARNING) is filtered on OHOS substrate)

Instrumented libart preserved at `docs/engine/V3-DIAG-VTABLE-EVIDENCE/libart.so.vt-diag-5adc5182`. Ready to redeploy after device reboot (HW was broken at baseline due to 4+ day uptime — not caused by patch).

### Device state caveat — REBOOT NEEDED

DAYU200 has been up 4+ days. End-of-day baseline HW retests (3 separate launches with pristine `ab3bfbd1` libart) all hit identical `CK_BEFORE_initChild_call` SEGV. Pre-existing device state issue, not patch-induced. **Next session must start with reboot + HW baseline reverification before any deploy.**

## A2 fix: 4-LOC body-only fix designed; BUILD DEFERRED

Patched source preserved at `docs/engine/V3-A1-A2-EVIDENCE/AppSchedulerBridge.java.fix-a2-patched`. The fix checks `sBindAppDone` before `scheduleTransaction` in `nativeOnScheduleLaunchAbility` — prevents schedule-LaunchActivity-after-failed-bind anti-pattern.

Build deferred: local pipeline needs `framework-minus-apex.jar` from [build-host], system constraint prevents the SCP. A2 alone is cosmetic without A1 anyway (would only convert SEGV → clean exception, doesn't render McD).

## Honest McD distance estimate (revised end-of-26)

Per `docs/engine/V3-MCD-WALL-DEPTH-ANALYSIS-2026-05-26.md` median estimate (1 week / 2-4 walls; ~7-8h focused work for ~60-70% probability of splash render) — we are deeper in the median scenario than the start. McD has cleared the BIND family but is now in the Activity-init family (G6 from contract, was estimated "post-render not splash-blocking" — but it IS blocking since SplashActivity.onCreate never starts).

Path to render:
1. Add J.2.b to WS-H libart (if missing) — likely closes WALL-1 and WALL-2 by routing app code through type-checking interpreter that handles `<clinit>` differently
2. Validate with McD launch — does SplashActivity.onCreate complete?
3. If yes → next wall is window-attach / first-paint territory
4. If still walling on R8-Kotlin `<clinit>` → need novel approach

## Cumulative session arc 2026-05-25 → 2026-05-26

| Day | Wins |
|---|---|
| 2026-05-25 morning | Audit + probe + Scope C landed (PIB → BCP, drift-immune, BCP first-jar-wins rule discovered) |
| 2026-05-25 evening | Tier 3 PIB bundle-name hint landed via local pipeline (Pass 3 — McD past scheduleTransaction first time) |
| 2026-05-26 morning | Fix I (CoreComponentFactory factory-skip, 2-line edit) — McD 15 dex registrations |
| 2026-05-26 mid-day | Wall-depth + non-API-gap analyses (32 categories audited, 23,951 LinkageError sites bounded by 1 libart fix) |
| 2026-05-26 evening | WS-H libart broad signature-drift bypass landed via [build-host] source pull + local 32-bit ARM cross-compile |
| 2026-05-26 end-of-day | Fresh McD diag: NEW walls in R8-Kotlin `<clinit>` family (AppInitializer.b, SavedStateRegistryController.a) — pid 31764 reaches SplashActivity ctor before exiting |

## 12 docs landed today (commits on `westlake-engine-2026-05-24` branch)

- McD render contract (V3-MCD-RENDER-CONTRACT-2026-05-26)
- McD wall-depth analysis (V3-MCD-WALL-DEPTH-ANALYSIS-2026-05-26)
- Non-API bridging gaps (V3-NON-API-BRIDGING-GAPS-2026-05-26 EN + CN)
- Diag-G OAT-shift correction (V3-DIAG-G-FRAMEWORK-OAT-NPE-PATTERN-2026-05-24 §3 + §4 update)
- Local oatdump build (V3-LOCAL-OATDUMP-BUILD-2026-05-26)
- Local build feasibility (V3-LOCAL-BUILD-FEASIBILITY-2026-05-25)
- Tier 3 pass 3 landed (V3-TIER3-PASS3-LANDED-2026-05-25)
- McD pursuit (V3-MCD-PURSUIT-2026-05-26)
- Top-3 walls run (V3-MCD-TOP3-WALLS-2026-05-26)
- Path A WS-H landed (V3-PATHA-WSH-LANDED-2026-05-26)
- Diag a3ee42 method ID (V3-DIAG-A3EE42-2026-05-26)
- Diag McD a3e636 current (V3-DIAG-MCD-A3E636-CURRENT-2026-05-26)
- Bionic-musl §12-14 supplement (media/storage/camera per-API EN+CN)
- API coverage §15-20 + §21-23 + §24-29 (wifi/cell/eth/gnss/health/watch + inventory + AI-feasibility EN+CN)
- API coverage update with wall-depth + non-API-gap cross-references

## Key memory rules adopted today

- [[feedback-no-builds-on-hbc-or-alex-2026-05-25]] — read-only SSH for source/baselines only
- [[feedback-bcp-first-jar-wins-2026-05-25]] — BCP shadowing requires shadowing jar BEFORE shadowed jar in BOTH runtime kBootClasspath AND build-time gen_boot_image.sh JARS
- [[feedback-appsched-bytecode-breaks-preload-2026-05-26]] — AppSchedulerBridge changes can regress McD preload via Fix A eager-resolution stale; no new methods/try-catch; body-only edits
- BCP jar byte change → ALWAYS regen boot image (per `reference_local_build_infra_2026-05-25`)

## See also

- [[v3-scope-c-pib-relocation-2026-05-25]] — Scope C + Tier 3 landing yesterday
- [[v3-mcd-chain-2026-05-23]] — 2026-05-23 J.2.b libart work (same wall family as today's WALL-1/WALL-2)
- [[reference-local-build-infra-2026-05-25]] — local build pipeline including 32-bit ARM libart cross-compile (PERMANENT post-2026-05-26)
- [[feedback-engine-principle-validation-2026-05-24]] — engine-not-patcher discipline (still holding)
