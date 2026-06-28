---
name: v3-mcd-2026-05-27-day-end
description: "End-of-day summary 2026-05-27 (afternoon): predictive audit landed (23 walls), 3 Batch agents built (A+B+CD), cycle 1 combined deploy failed silently (HW broke), bisects identified B as culprit, B-fix attempts on static-allocation hypothesis failed (rejected), test of A+CD on McD showed even A alone causes a libart class-load pathology on complex apps. Final state: substrate ROLLED BACK to J.2-G+ baseline (10/10 md5 match, soft-rebooted device), HW renders cleanly, McD reaches substrate-level landmarks (vtable+forter+kotlinx+androidx). Tomorrow's session starts from clean J.2-G+ with both A and CD artifacts archived for cycle 2."
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Day-in-2-sentences

Took day to validate predictive-audit methodology (HUGE WIN — 23 walls identified in 1 cycle) and attempted Cycle 1 combined-deploy of 3 batches (A: thread-bind, B: 8 service stubs, CD: 57 native impls). Combined deploy broke HW; bisects + B-fix attempts failed to localize/fix structural issue — strong evidence that boot-regen + new-BCP-classes triggers a libart class-load pathology in complex apps (HW survives, McD doesn't). Substrate rolled back cleanly to J.2-G+ baseline; A and CD artifacts archived.

## Final substrate state (verified post soft-reboot)

| File | md5 | Notes |
|---|---|---|
| libart.so | `fbd2b928` | unchanged |
| appspawn-x | `3abe3bde` | unchanged |
| adapter-runtime-bcp.jar | `9b76b56a` | J.2-G+ baseline |
| oh-adapter-framework.jar | `13ed0c8e` | J.2-G+ baseline |
| liboh_android_runtime.so | `102f832e` (both paths) | J.2-G+ baseline |
| boot.oat | `0a9f1554` | J.2-G+ regen |
| boot-framework.oat | `b9194941` | J.2-G+ regen |
| boot-oh-adapter-framework.oat | `2dc4c7ec` | J.2-G+ regen |
| boot-adapter-runtime-bcp.oat | `3582b556` | J.2-G+ regen |

## Today's progression chain (afternoon)

```
Morning: J.2-G+ baseline (McD reaches AMS ForegroundLifecycle per project_v3_mcd_2026-05-27.md)
  ↓
[PREDICTIVE AUDIT] 23 walls enumerated, 4 batches recommended
  ↓
[3 parallel agents] A built ✓, B built ✓, CD built ✓
  ↓
[Cycle 1 combined deploy] HW silent stall → ROLLBACK (substrate restored)
  ↓
[Bisect 1] CD only deployed → HW PASS
  ↓
[Bisect 2] CD + A deployed → HW PASS  
  ↓
[Bisect 3] CD + A + B deployed → HW FAIL (B identified as culprit)
  ↓
[B-fix-1] hypothesis (contamination) → rebuild + deploy → SAME FAIL (hypothesis rejected)
  ↓
[B-fix-2] hypothesis (ConcurrentHashMap clinit alloc) → remove + rebuild + deploy → SAME FAIL (hypothesis rejected)
  ↓
[McD test on A+CD] McD also stalls under A+CD (smaller stress than B but McD complex enough to trigger pathology)
  ↓
[ROLLBACK to J.2-G+] all 10 substrate files restored byte-exact + soft reboot
  ↓
[Post-reboot verify] Substrate OK, HW OK, McD reaches J.2-G+ substrate-level landmarks
```

## Confirmed engineering rules added today

1. **[dlopen-in-child-allocates-2026-05-27]** — System.loadLibrary in initChild → mark_sweep SIGABRT (extends Fix A rule)
2. **[liboh-android-runtime-dual-path-2026-05-27]** — REFERENCE: dual-path deploy required
3. **[aosp-version-jni-signature-drift-2026-05-27]** — verify signatures against on-device smali, not AOSP HEAD
4. **[jni-stub-functions-silent-failure-2026-05-27]** — every JNI method needs real impl, `nm --print-size <16B` check
5. **[clinit-allocation-NOT-root-cause-2026-05-27]** — explicit anti-rule: removing static allocations DOES NOT fix the libart class-load pathology

## Open mystery: libart class-load pathology

When boot.oat is regenerated with new BCP classes (even tiny additions to existing jars), McD-complexity apps stall in libart class-linker creating threads in a tight loop. Mechanism not yet understood. Symptoms:
- Child stderr 4 KB / 600 KB (varies — depends on how far class loading gets before stall)
- Final lines = `[DBG] before new Thread()` / `Thread::Init` repeated
- No SEGV, no cppcrash, no Java exception
- Watchdog kill after ~50s
- HW (trivial class graph) NEVER triggers it
- McD (~5000 R8-bundled classes) ALWAYS triggers it with any new BCP class
- A+CD triggers it for McD too (not just B)
- Removing the static heap allocation in B's OHGenericServiceBinder didn't help

Next-session investigation paths:
1. libart class linker source — look for thread-spawning paths during class verification
2. dex2oat compile flags — is there a layout-stable option?
3. Capture actual class names where stall starts — may reveal a specific verification trigger
4. Compare boot.oat dex file table between J.2-G+ and post-A regens
5. Try DIFFERENT B implementation: rather than 8 new classes, MODIFY existing classes (no new class indices)

## Archived artifacts for cycle 2

- `docs/engine/V3-CD-ARCHIVE/liboh_android_runtime.so.cd-only` md5 `4f5ea5c8` (57 native impls — confirmed safe for HW)
- `docs/engine/V3-A-ARCHIVE/adapter-runtime-bcp.jar.batchA` md5 `0d51de28` (MainThreadMarshal — never reached during testing because McD didn't get to bind)

## Key documents written today

- `docs/engine/V3-MCD-PREDICTIVE-AUDIT-2026-05-27.md` (commit `b5c497c1a13`) — 23 walls + 6 substrate gaps + 4 batches
- `docs/engine/V3-PREDICTIVE-CYCLE-PLAN-2026-05-27.md` — cycle plan structure
- `docs/engine/V3-CYCLE1-DEPLOY-2026-05-27.md` (commit `747f5f38ade`) — combined deploy fail + rollback
- `docs/engine/V3-BISECT{1,2,3}-*-2026-05-27.md` — bisect series
- `docs/engine/V3-BFIX{1,2}-DEPLOYED-2026-05-27.md` — fix attempts that failed
- `docs/engine/V3-HANDLER-DIAG-2026-05-27.md` (commit `07aaa572b6a`) — earlier wall diagnosis (mis-framed Handler/Looper)
- `docs/engine/V3-FIX-J2-G-LANDED-2026-05-27.md` + `V3-FIX-J2-GP-LANDED-2026-05-27.md` — morning's SQLite fixes (these LANDED and stuck)
- `docs/engine/V3-ROLLBACK-TO-J2GP-2026-05-27.md` — final rollback

## Successes that LANDED today (morning, still live in substrate)

1. **J.1 SecureRandom** — McD past FacebookInitProvider (Conscrypt missing → OHSecureRandomProvider shim)
2. **J.2-G SQLite core** — 47 SQLite natives via AndroidRuntime::startReg (Forter FTRHXContentProvider.nativeOpen works)
3. **J.2-G+ SQLite extension stubs replaced** — LOCALIZED collation via ICU 72; both Forter DBs open cleanly

## Engineering process wins

1. **Predictive audit methodology proven** — must keep doing this
2. **Memory rule capture discipline maintained** — 5 new feedback rules captured before being forgotten
3. **Atomic pre-snapshot rule held** — every rollback was clean
4. **Twin-build determinism enforced** — caught no determinism bugs (no false positives)
5. **HARD GATE protocol works** — HW regression → ROLLBACK + STOP prevented bad deploys from cascading

## What to start with tomorrow

1. **Investigate the libart class-load pathology** — read class_linker.cc thread spawning paths, identify what condition causes it. Likely the right fix to unlock B-style adapter expansion.
2. **Re-validate full McD progression at J.2-G+** via hilog query (not just child stderr) to confirm baseline is fully restored
3. **Plan B-2 architecture**: instead of adding 8 new stub classes (which trigger pathology), MODIFY existing OHServiceManager + register stubs DYNAMICALLY at runtime (no new class index in boot.oat)
4. **Consider CD-only redeploy** — CD is proven safe (bisect-1 + bisect-2). Could be deployed as-is for the SC/BBQ render-path benefit, independent of B/A.

## See also

- [[v3-mcd-2026-05-27]] — morning's milestone (5 wall families closed, J.2-G+ baseline)
- [[feedback-fix-a-resolve-not-allocate-2026-05-26]] — original Fix A allocation rule
- [[feedback-dlopen-in-child-allocates-2026-05-27]] — today's J.2 lesson
- [[feedback-jni-stub-functions-silent-failure-2026-05-27]] — today's J.2-G+ lesson  
- [[feedback-aosp-version-jni-signature-drift-2026-05-27]] — today's J.2-G lesson
- [[liboh-android-runtime-dual-path-2026-05-27]] — today's deploy reference
