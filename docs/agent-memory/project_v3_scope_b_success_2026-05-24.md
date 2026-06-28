---
name: v3-scope-b-success-2026-05-24
description: "Scope B Phase 2 (BCP consolidation) + Fix A.ii (PackageManagerAdapter root-cause fix) both landed end-to-end 2026-05-24. First architectural fix day with zero rollbacks, no per-APK patches, McD past <init>, 6 apps benefit from one upstream fix. New BCP: 10 jars; new boot.oat 30 segments. Diag-F H4 confirmed AND resolved."
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Fact

**2026-05-24** — first day Westlake landed end-to-end architectural fixes with zero rollbacks and zero per-APK changes. Three major milestones:

1. **Scope B Phase 2 deploy** — `adapter-runtime-bcp.jar` now in BCP, 3-CL geometry collapsed to 2-CL, McD's `CoroutineScheduler.g==null` wall structurally closed
2. **Fix A.ii root-cause fix** — `PackageManagerAdapter.getApplicationInfo` returning empty `ApplicationInfo` for OH-installed APKs identified + fixed at upstream (~50 LOC backup-fill in `AppSchedulerBridge.ensureBindApplication` + boot image refresh)
3. **6 apps unblocked from one upstream fix** — Netflix/Spotify/Amazon/Maps/Zoom all past original `boot-framework.oat+0xa3ee42` NPE; McD reaches `className=McDMarketApplication` factory loading

## Why this matters

Validates the engine-not-per-APK principle definitively. Before today's reset:
- 6 substrate fixes (QW1-QW6) chained over Diag-F H4 in same architectural family
- QW6 actively regressed QW4
- Multiple per-APK smali patches accumulated

After the reset:
- Diag-F audit identified single upstream root cause (3-CL geometry)
- Scope A generalized Fix A-132 to 30 bridges (validated H4)
- Scope B Phase 2 moved AppSpawnXInit+AppSchedulerBridge into BCP (durable fix)
- Fix A.ii fixed PackageManagerAdapter populating ApplicationInfo properly

**One architectural fix > N symptom patches.**

## Current device state (end of 2026-05-24)

Target `dd011a414436314130101250040eac00` (Windows hdc.exe at `/mnt/c/Users/<user>/Dev/ohos-tools/hdc.exe`):
- `appspawn-x` md5 `fee0053c` (Scope B Phase 2)
- `oh-adapter-runtime.jar` md5 `1d675384` (slim 892B — Scope B)
- `adapter-runtime-bcp.jar` md5 **`6d360101c4e2fc1f196ed847b123ce26`** (Fix A.ii applied — was ed3cd57d at Phase 2)
- `oh-adapter-framework.jar` md5 `ecfb5ac3...` (baseline, untouched — per Phase 2.1 verifier)
- `libart.so` md5 `64bd4ab8d7f5c2c2626e6d4473a65596` (QW4 baseline + 5 libart patches)
- 30 boot segments at `/system/android/framework/arm/` — new for Fix A.ii: `boot-framework.oat` md5 `008a4038...`, `boot-adapter-runtime-bcp.oat` md5 `7e050aa0...`
- McD APK `/data/app/el1/bundle/public/com.mcdonalds.app/android/base.apk` (unchanged Apr-8 stock binary)
- SELinux Permissive

## Architecture summary (per V2 doc)

**Westlake = Zygote-style engine, not container.**

```
appspawn-x ([build-host]'s Zygote)
  ├─ Preloads BCP: 10 jars (core-oj, core-libart, core-icu4j, okhttp, bouncycastle,
  │                apache-xml, adapter-mainline-stubs, framework, oh-adapter-framework,
  │                adapter-runtime-bcp [Scope B])
  ├─ Preloads boot.art (30 segments after Scope B)
  ├─ Runs AppSpawnXInit.preload (Fix A-132 + Scope A: 30 adapter bridges eager-resolved)
  └─ fork() per APK
       └─ child: single CL chain (BootClassLoader ← PathClassLoader for APK)
            └─ ActivityThread.main() → handleBindApplication → Application.onCreate → Activity.onCreate
```

7-layer architecture (per V2 doc):
- L1 Process spawn (appspawn-x)
- L2 Class load + ART runtime (vanilla AOSP-14 + 3 libart patches: J.2.b nterp gate, QW3.b JIT gate, [interim] QW2+QW4 class_linker)
- L3 Application + Activity lifecycle (13 adapter classes)
- L4 System services (OHServiceManager — 5 real + 6 stubs + ~25 pending samgr broker)
- L4b PMS (3 classes — PackageManagerAdapter + PackageInfoBuilder + PermissionMapper)
- L5 Window + Surface (11 Java + 10 native)
- L6 Rendering (12-vfn OHGraphicBufferProducer)
- L7 Input + reverse callbacks (touch DONE, 11 more event types pending)

## Fix A.ii details (the breakthrough)

**Root cause identified**: `PackageManagerAdapter.getApplicationInfo` returns under-populated `ApplicationInfo` for OH-installed Android-compat APKs:
- Empty `packageName`
- `uid = -1`
- Empty `sourceDir`

This causes `LoadedApk.getResources()` to return null deep in `handleBindApplication` chain. AOSP framework code (`ConfigurationController.updateLocaleListFromAppContext`) deref's the null and SEGVs at `boot-framework.oat+0xa3ee42`.

**Fix applied** (~50 LOC, in `adapter-runtime-bcp.jar`):
1. `AppSchedulerBridge.ensureBindApplication` backup-fills empty fields from bundleName arg + probes known APK/data dirs
2. `applyManifestFieldsToAppInfoLocal` lifts path/dataDir/flags overrides out of empty-manifest-JSON early-return
3. `AppSpawnXInit.preload()` adds `Resources.getSystem()` warmup so child inherits initialized class table
4. `AppSpawnXInit.initChild()` top adds direct-nativeHiLog ping + Resources probe for diagnostic

**Per-app outcomes after Fix A.ii**:
| App | Status | Detail |
|---|---|---|
| HelloWorld | RENDERS | no regression |
| Netflix | original SEGV FIXED | loads 12 dex, killed by AMS lifecycle timeout |
| Spotify | original SEGV FIXED | loads 12 dex, no cppcrash, killed by lifecycle |
| Amazon/Maps/Zoom | original SEGV FIXED | NEW SEGV at `boot-framework.oat+0xa3e636` (same @NonNull pattern, different method) — needs Fix A.iii |
| McD | reaches `className=McDMarketApplication` factory loading | hits separate libart codegen crash (Diag-H next) |

## Bisect finding (Phase 2 follow-up)

`boot-framework.oat+0xa3ee42` is **pre-existing pre-Scope-B bug**, not regression. Confirmed by byte-matching identical AOT code in pre-Scope-B baseline + Scope B Phase 2 OATs. Scope A's kotlinx wall was masking it. Scope B exposed it (and Fix A.ii fixed it).

## Key open walls (end of 2026-05-24)

1. **`boot-framework.oat+0xa3e636`** — same @NonNull pattern, different method. Blocks Amazon/Maps/Zoom. Needs Fix A.iii using methodology Fix A.ii established (`oatdump --addr2instr` with offset `file_offset - 0x1000` correction).
2. **McD libart codegen crash** at `ArtInterpreterToCompiledCodeBridge+0x78`. Needs Diag-H — build libart with debug symbols for backtrace.
3. **AMS lifecycle timeout** (~100s) — Netflix/Spotify die here after dex load. Could be class-init cascade in real apps. Likely needs M2 samgr broker to provide more services.

## What to keep, what to revert

**Keep permanently** (architectural infrastructure):
- Fix A-132 + Scope A preloads (defense in depth post-Scope B; harmless if unused)
- J.2.b nterp gate (still useful — VIXL bytecode patterns)
- QW3.b JIT gate (still useful)
- QW1 ServiceManager.sCache wiring
- QW5.a verify-cascade removal
- Scope B BCP consolidation
- **Fix A.ii backup-fill** (the new key piece)

**Revert when validated** (after Scope B stable + 3rd-party apps progressing):
- QW2 class_linker `HasSameSignatureWithDifferentClassLoaders` patch
- QW4 class_linker `FindResolvedMethod` patch
- C.2a class_linker `ValidateSuperClassDescriptors` skip

Once the 3-CL geometry is gone (Scope B), these become no-ops. Validate first, then strip.

## Tooling lessons captured

1. **`hdc.exe file send` from WSL** silently NOOPs on bad paths. Use `hdc_verify_send.sh` wrapper (in `docs/engine/V3-SCOPEB-PHASE1-EVIDENCE/`) for every file send with md5 verify on device.
2. **`pkill -9 appspawn-x`** doesn't always work despite procname under 15-char `/proc/comm` cap. Use `kill -9 <pid>` directly.
3. **`oatdump --addr2instr` returns wrong methods** for OAT v230 format. Correct mapping: `oatdump_addr = file_offset - 0x1000`. Verified via raw-byte matching.
4. **[build-host] build chain `build_aosp_fw.sh --target=<jar>`** auto-triggers boot image regen when BCP jar changes — single command for both adapter + boot.oat rebuild.

## Files / commits

- V2 architecture doc (EN, 1093 lines): `docs/engine/WESTLAKE-ARCHITECTURE-V2-2026-05-24.md`
- V2 architecture doc (CN, 1093 lines): `docs/engine/WESTLAKE-ARCHITECTURE-V2-2026-05-24-zh.md`
- V2 appendix (EN, 1127 lines, function-level): `docs/engine/WESTLAKE-V2-APPENDIX-SHIM-INVENTORY-2026-05-24.md`
- V2 appendix (CN, 1049 lines): `docs/engine/WESTLAKE-V2-APPENDIX-SHIM-INVENTORY-2026-05-24-zh.md`
- Scope B Phase 2 deploy report: `docs/engine/V3-SCOPEB-PHASE2-DEPLOY-2026-05-24.md`
- Fix A.ii report: `docs/engine/V3-SCOPEB-FIX-AII-LOADEDAPK-ROOTFIX-2026-05-24.md`
- Diag-F upstream divergence: `docs/engine/V3-DIAG-F-UPSTREAM-DIVERGENCE-2026-05-24.md`
- Bisect multi-dex: `docs/engine/V3-SCOPEB-BISECT-MULTIDEX-2026-05-24.md`
- All commits on `westlake-engine-2026-05-24` branch in `docs/` repo (no push)

## Cross-references

- [[v3-mcd-chain-2026-05-23]] — yesterday's substrate-fix chain (Fix A through QW6)
- [[v3-helloworld-renders-fix-a-2026-05-22]] — Fix A milestone that Scope A generalized
- [[feedback-additive-shim-vs-architectural-pivot]] — the principle that drove today's pivot
- [[reference-remote-server-access]] — [build-host] + [build-host] credential map
- [[feedback-no-per-app-hacks]] — engine-not-patcher rule reaffirmed
