---
name: v3-scope-c-pib-relocation-2026-05-25
description: "2026-05-25 — Scope C + Tier 3 BOTH LANDED end-of-day. PIB relocated to adapter-runtime-bcp.jar (drift-immune); Tier 3 process-wide bundle-name hint closes BMS empty-packageName gap. McD progressed PAST scheduleTransaction for first time (was blocked at bindApplication entry). New downstream wall: same +0xa3e636 in SplashActivity exec context. Substrate: libart b71e46a7, adapter-runtime-bcp bbea92f9 (Tier 3), appspawn-x 3abe3bde, boot.art via local pipeline. Local WSL build pipeline VALIDATED end-to-end (zero [build-host] build invocations on Pass 3)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## What landed (still in production after restore)

**Scope C — PIB relocation** (commit `68b0696f` on `westlake-engine-2026-05-24`):
- `PackageInfoBuilder` source moved from `oh-adapter-framework.jar` to `adapter-runtime-bcp.jar`
- Old PIB in `oh-adapter-framework.jar` shadowed (dead code, but jar untouched on device)
- **BCP reorder**: `adapter-runtime-bcp` now BEFORE `oh-adapter-framework` (was after in Scope B). Patched in `framework/appspawn-x/src/main.cpp` and `build/inner/gen_boot_image.sh`. New appspawn-x md5 `3abe3bde`.
- Tier 2 PIB filesystem probes + AOSP-safe defaults active (proven by Netflix's PIB-TIER2 marker firing)
- `oh-adapter-framework.jar` STAYS at baseline `ecfb5ac3` — drift exposure permanently closed

## Tier 3 attempt history (4 attempts; 4th succeeded)

1. **First** (ae9c0453, evening): blocked when [build-host] SSH was unreachable
2. **Second** (a40c585f, evening): [build-host] disk hit 100% full (westlake-dev 273G + AlexYang 293G) — build aborted silently
3. **Third** (ac5847e5, late evening, first local-build attempt): javac+d8+jar worked but local x86_64 dex2oat `$HOME/art-universal-build/build/bin/dex2oat` SIGSEGV'd on BCP jars; [build-host] dex2oat64 (pulled locally) worked but BCP ORDER MISMATCH in build vs runtime kBootClasspath caused `runtime.cc:699 Class mismatch for Ljava/lang/String;` abort → manual Scope C restore from staged Windows artifacts.
4. **Fourth** (a6d55cbb, Pass 3 with all 4 fixes): **LANDED**. Local WSL build, [build-host] dex2oat64 cached locally, BCP order matched, all 32 segments snapshotted, single-password scp. McD progressed past scheduleTransaction.

## What Tier 3 actually changed in production

- `adapter-runtime-bcp.jar` md5 `bbea92f9` — now contains Tier 3 PIB + WestlakeProcessBundleName
- New boot image: boot.oat `7c70151f`, boot-framework.oat `d9ebf221`, boot-adapter-runtime-bcp.oat `8cb4fcb8`
- libart.so unchanged (still Fix H + Fix I.§5 `b71e46a7`)
- oh-adapter-framework.jar unchanged (still `ecfb5ac3` — drift-immune)
- appspawn-x unchanged (still Scope C BCP reorder `3abe3bde`)

## Current device state (end of 2026-05-25 session — Tier 3 live)

| File | md5 | Source |
|---|---|---|
| libart.so | `b71e46a7` | Fix H + Fix I.§5 (2026-05-24) |
| adapter-runtime-bcp.jar | **`bbea92f9`** | **Tier 3 (PIB + WestlakeProcessBundleName)** |
| oh-adapter-framework.jar | `ecfb5ac3` | Untouched (drift-immune) |
| appspawn-x | `3abe3bde` | Scope C BCP reorder |
| boot.oat | **`7c70151f`** | **Tier 3 (local pipeline regen)** |
| boot-framework.oat | **`d9ebf221`** | **Tier 3 (cross-jar regen)** |
| boot-adapter-runtime-bcp.oat | **`8cb4fcb8`** | **Tier 3** |
| McD APK | unchanged | stock binary |

Pre-Tier-3 rollback artifacts on device: `/data/local/tmp/pre-tier3-pass3/` (32 files — adapter-runtime-bcp.jar + appspawn-x + 30 boot segments).

## McD progression chain (Tier 3 LIVE)

For the first time today:

```
[B43-BIND] ensureBindApplication start bundle=com.mcdonalds.app           ✓
[PIB-TIER2] [POPULATED] com.mcdonalds.app sourceDir=...mcdonalds.app.apk  ✓ PIB finds correct paths (was malformed pre-Tier 3)
[B43-BIND] appInfo enriched: className=...McDMarketApplication            ✓
[B47-SLA] BEFORE scheduleTransaction className=...SplashActivity          ✓ NEW
LaunchActivity transaction scheduled: ...SplashActivity                   ✓ NEW
<SIGSEGV at boot-framework.oat+0xa3e636 in SplashActivity exec>           ✗ same addr, NEW context
```

**Same crash address as before Tier 3, but context shifted from bindApplication entry → SplashActivity exec.** Per Diag agent (afb6ac29 in flight at memory write time): likely a SECOND classloader instantiation during Activity exec (Hilt DI / fragment load / R8 classes2.dex / runtime SDK) that bypasses Tier 3's coverage. May need to extend the hint-coverage or fix the AOSP @NonNull violation directly.

## Per-app outcomes after Tier 3 (Pass 3)

| App | Result |
|---|---|
| HelloWorld | RENDERS — no regression, lifecycle CREATED+RESUMED, pid alive 4+ min |
| McD | PROGRESS — past scheduleTransaction, dies in SplashActivity exec at same +0xa3e636 |
| Netflix | PROGRESS — PIB-TIER3 hint fires (`BUNDLE-HINT-USED` marker), dex chain loads |
| Spotify, Amazon, Maps, Zoom | Not exercised (need explicit Android ability syntax — pre-existing test methodology limitation) |

## Fix A.ii revert: DEFERRED

Both spec conditions met (McD progressed, no `[B43-BIND][FIX-AII]` markers fired) BUT new SplashActivity-exec wall fires before full validation across all apps. Bundle revert with the next McD pass to avoid intermediate cycles.

## Key architectural insights from today

- **Drift escape pattern**: relocating classes from westlake-dev-owned to [user]-owned jars is repeatable. Scope B (AppSpawnXInit + AppSchedulerBridge) + Scope C (PIB) both used this. Future drift exposures use same pattern.
- **BCP first-jar-wins ordering** is an engine invariant ([[feedback-bcp-first-jar-wins-2026-05-25]]).
- **Local build pipeline is VALIDATED at jar level** but boot image regen is fragile. ([[reference-local-build-infra-2026-05-25]])
- **No-flash recovery is reliable** when pre-snapshot is complete. Multi-step rollback procedure: restore staged Windows artifacts when on-device snapshots are incomplete.
- **PMS audit + probe methodology** prevented a wrong-bit Tier 1 deploy. Engine-principle "verify before deploy" discipline paid off.

## See also

- [[v3-scope-b-success-2026-05-24]] — Scope B precedent that Scope C extended
- [[feedback-engine-principle-validation-2026-05-24]] — the audit-first principle that drove today's discipline
- [[feedback-no-builds-on-hbc-or-alex-2026-05-25]] — directional rule from today's session
- [[feedback-bcp-first-jar-wins-2026-05-25]] — BCP ordering engine invariant
- [[reference-local-build-infra-2026-05-25]] — local build pipeline inventory + Tier 3 retry recipe
