---
name: v3-mcd-2026-05-27
description: "2026-05-27 — 5 architectural wall families CLOSED for McD in one day. Vtable corruption (Vtable-A v1+v2), PMS provider lookup (getProviderInfo + queryContentProviders), AbstractMethodError on ContentProvider, UserManager null service stub. McD now reaches AMS ForegroundLifecycle + foregrounded. Next wall: Handler/Looper dispatch gap — LaunchActivityItem scheduled but main thread parks in nativePollOnce for 50s → APP_FREEZE. Plus: 4 new analysis docs landed (bionic count 2033, NDK ABI 4500, API count 250-350, per-APK analyzer tool). HW regression-clean throughout."
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Day in 2 sentences

**5 architectural wall families CLOSED for McD in one day** (vtable corruption, PMS getProviderInfo, AbstractMethodError ContentProvider, UserManager null stub, PMS queryContentProviders). McD now reaches AMS ForegroundLifecycle + foregrounded; only Handler/Looper dispatch gap left between us and LIFECYCLE CREATED.

## Substrate state (end-of-day 2026-05-27)

| File | md5 | Source |
|---|---|---|
| libart.so | **`fbd2b928`** | Vtable-A v1 (broad bypass retired) + Vtable-A v2 (abstract resolution) + companion + Fix I + J.2.b |
| adapter-runtime-bcp.jar | `1ed8e9ce` | Fix I |
| oh-adapter-framework.jar | **`13ed0c8e`** | UserManager stub + PMS getProviderInfo + PMS queryContentProviders |
| appspawn-x | `3abe3bde` | Scope C BCP reorder |
| boot-framework.oat | `d9ebf221` | Local regen |
| boot-oh-adapter-framework.oat | **`325a944a`** | Local regen (PMS-QUERY) |
| boot.oat | (depends) | Regen per cycle |

HW renders cleanly throughout all 5 deploys.

## McD progression chain — DEEPEST EVER

```
✓ Process spawn
✓ ApplicationInfo populated (Tier 3)
✓ bindApplication
✓ Class linking (vtable v1 + v2)
✓ McDMarketApplication.<init> body
✓ kotlinx Dispatchers / CoroutineScheduler init
✓ androidx.startup.AppInitializer.discoverAndInitialize
✓ [FIX-PMS-PROVIDER] backup-filled InitializationProvider  ← first ever fire
✓ ContentProvider.attachInfo (no AbstractMethodError)
✓ McDMarketApplication enrichment
✓ [B47-SLA] SplashActivity recordId=53
✓ AMS marks ForegroundLifecycle               ← NEW MILESTONE
✓ AMS foregrounds McD                          ← NEW MILESTONE
✗ Main thread parks in MessageQueue.nativePollOnce for 50s
✗ AMS LIFECYCLE_TIMEOUT → APP_FREEZE
```

## 5 wall families closed today (in chronological order)

| # | Fix | LOC | Substrate change |
|---|---|---|---|
| 1 | **Vtable-A v1 + companion** (libart) | ~30 + 20 | libart → ab3bfbd1 → 3da028a2 |
| 2 | **Fix I CoreComponentFactory** (jar) | 4 | adapter-runtime-bcp → 6d00bf5b |
| 3 | **Vtable-A v2 abstract resolution** (libart) | ~30 | libart → fbd2b928 |
| 4 | **PMS getProviderInfo + resolveContentProvider** (jar, deployed first time today) | ~75 | oh-adapter-framework → 6f837d84 → 13ed0c8e |
| 5 | **UserManager IUserManagerStub** (jar) | ~95 | oh-adapter-framework → 59464f15 |
| 6 | **PMS queryContentProviders** (jar) | ~55 | oh-adapter-framework → 13ed0c8e (final) |

Total LOC today across all adapter + libart fixes: ~309 LOC

## The next wall — Handler/Looper dispatch gap

**Not a class linking issue. Not a service stub issue. NEW family entirely.**

- AMS scheduleTransaction binder fires successfully
- AMS marks McD ForegroundLifecycle
- LaunchActivityItem present in transaction
- McD's main thread sits idle in `MessageQueue.nativePollOnce` for 50s
- The transaction isn't reaching ActivityThread.H handler

**Investigation in flight (agent `ad79322ed5f8399d3`):** trace `nativeOnScheduleLaunchAbility v2` chain at hilog 16:14:46.244. Verify whether H.sendMessage fires onto McD's main Looper.

**Failure mode candidates:**
- A: Binder transaction never reaches McD process
- B: Received but not routed to ActivityThread.scheduleTransaction
- C: scheduleTransaction routes but doesn't call H.sendMessage
- D: H bound to wrong Looper
- E: nativePollOnce doesn't wake on new message (epoll/eventfd gap)
- F: Native pipe between binder thread and main thread broken

## 4 new analysis docs landed today

1. **V3-API-COUNT-2026-05-27** (commit `fdaf2a31f76`) — ~250-350 Android API surfaces curated; ~80-130 net unbridged
2. **V3-BIONIC-COUNT-2026-05-27** (commit `d25bf71a1af`) — 2,033 bionic exports / 554 bionic-only / ~50 practical bridging surface / McD = 4
3. **V3-NDK-BIONIC-ABI-2026-05-27** (commit `5591e71860f`) — 3-layer breakdown (L1 bionic + L2 NDK higher-layer + L3 linker) — total ~4,500 symbols; user-clarified ~200 system-traversing
4. **V3-APK-ANALYZER-TOOL-2026-05-27** (commit `f600a348688`) — reusable `analyze_apk_native.sh` validated against 5 known APKs

## Critical engineering insights

### Insight 1 — "Prior fix not actually deployed" (PMS-PROVIDER case)
The earlier PMS-PROVIDER fix (commit `5afddd6207f`) was CODE-ONLY — never deployed to substrate. The PMS-QUERY agent today was the first end-to-end activation of BOTH PMS-PROVIDER and PMS-QUERY backup-fills. Commit landing ≠ deployment landing. Always verify substrate md5 against expected.

### Insight 2 — system-traversing vs symbol count framing
Operator pushed back on "1,000 NDK symbols need bridging" framing. Real answer: only **system-traversing calls** need bridging. In-process (libicu, libz) and kernel-direct (GLES/Vulkan via /dev/dri) don't. True practical surface: ~200 symbols, not 1,000+.

McD's bionic+NDK bridging need: 0 net work (all 4 bionic-only are in-process; only NDK lib is liblog/already-bridged).

### Insight 3 — UserManager stub never fires but is load-bearing
`[FIX-USERMGR]` marker never fires across 3 McD launches. Yet adding the stub class to BCP changed dex2oat layout, which shifted McD's ContentProvider init ordering, which made McD skip Firebase entirely and go straight to its own SDK. The stub's PRESENCE in the substrate is load-bearing, independent of whether its methods are called.

### Insight 4 — Architecturally, McD's blocker has always been Java framework
Today's 5 fixes prove: McD's actual walls were not native ABI (bionic/NDK gap was 0 net). All 5 fix families were Java framework layer (vtable / PMS / ContentProvider / UserManager / dispatch). The "engineer-years" estimate for Java framework gap is what's actually being burned down each session.

## Cumulative architectural state

**Working substrate (HW renders):**
- 3-CL collapsed to 1-CL (Scope B, 2026-05-24)
- BCP first-jar-wins ordering with adapter-runtime-bcp FIRST (Scope C, 2026-05-25)
- PIB drift-immune ([user]-owned jar)
- Tier 3 bundle-name hint
- Fix I CoreComponentFactory factory-skip
- WS-H libart broad signature-drift bypass RETIRED in favor of Candidate A + companion
- Vtable-A v1 (name-mismatch relocation)
- Vtable-A v2 (abstract-resolution relocation)
- J.2.b CanMethodUseNterp gate
- PMS getProviderInfo + resolveContentProvider backup-fill
- PMS queryContentProviders backup-fill
- IUserManagerStub registered to OHServiceManager
- ApplicationInfo backup-fill (Fix A.ii) still active for defense in depth
- 30-segment boot image (Tier 3+)

**Local build infrastructure (proven):**
- javac 21 + r8.jar D8 + jar (NOT d8 — anon-inner-class NPE)
- [build-host] dex2oat64 + libc++/libsigchain cached locally
- 32-bit ARM libart cross-compile (`build_libart_pathA.sh`)
- oatdump cross-compile (v183, doesn't decode v230 — minor gap)
- per-APK bionic/NDK analyzer (`analyze_apk_native.sh`)
- Twin-build determinism check before every deploy
- ALL boot segments snapshotted per partial-snapshot landmine rule

## Memory rules adopted/reinforced today

- **Code commit ≠ substrate deployment** — always re-verify md5 of /system/* before assuming a "landed" fix is actually live
- **System-traversing vs symbol-count framing** for ABI bridging — counting symbols is misleading; count what crosses OHOS system boundary
- **Bytecode-shape effects from non-AppSchedulerBridge classes can still help** even if their methods aren't called (UserManager stub case)

## Next walls (post-Handler/Looper fix)

After dispatch wall closed, McD will reach `[LIFECYCLE] CREATED` for SplashActivity. Then:
- SplashActivity.onCreate executes — likely Hilt DI fires
- setTheme() / setContentView() — theme resolver may fight
- First frame paint — WindowManager binds Activity window to display

Per yesterday's wall-depth analysis: all of these are characterized walls with known fix patterns. None are impossible.

## See also

- [[v3-mcd-2026-05-26]] — yesterday's vtable corruption identification (root cause for today's Vtable-A work)
- [[v3-scope-c-pib-relocation-2026-05-25]] — Scope C + Tier 3 baseline
- [[feedback-no-builds-on-hbc-or-alex-2026-05-25]] — directional rule, still holding
- [[feedback-bcp-first-jar-wins-2026-05-25]] — engine invariant
- [[feedback-appsched-bytecode-breaks-preload-2026-05-26]] — d.1 lesson, still constrains
- [[feedback-fix-a-resolve-not-allocate-2026-05-26]] — preload allocation rule
- [[reference-local-build-infra-2026-05-25]] — extended with new artifacts today
