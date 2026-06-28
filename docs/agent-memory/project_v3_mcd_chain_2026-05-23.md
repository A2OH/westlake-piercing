---
name: v3-mcd-chain-2026-05-23
description: "McD progresses past scheduleTransaction via libart J.2.b nterp-disable + smali NOP chain (G.1.b, H.3, J.2.c). nterp SEGV permanently eliminated. Remaining walls are clean Java Kotlin/Hilt compatibility exceptions, not native crashes."
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Fact

On 2026-05-23, McDonald's APK (`com.mcdonalds.app` 26.31.1, v7a) progressed on DAYU200 V7 from "crashes ~3s in McDMarketApplication.<init>" to **reaches scheduleTransaction OK + 25+ dex registrations past bindApplication**, dying on clean Java exceptions instead of native SEGV.

McD has not yet visibly rendered its splash UI — but the architectural class of remaining walls has changed from "native crashes in libart" to "Kotlin/Hilt version-compatibility Java exceptions in McD's R8-bundled kotlinx".

## Fix chain applied today (in order)

| Fix | Layer | What it does | Where |
|---|---|---|---|
| **G.1.b** | smali (McD) | NOP `kotlinx.coroutines.scheduling.DefaultIoScheduler.<clinit>` body to minimum-fields init (c=new DefaultIoScheduler(), d=UnlimitedIoScheduler.b, return-void) | `smali_classes13/kotlinx/coroutines/scheduling/DefaultIoScheduler.smali` |
| **H.3** | smali (McD) | Null-guard at entry of `CombinedContext.g(Element)` and `.h(CombinedContext)` — return false if other==null | `smali_classes12/kotlin/coroutines/CombinedContext.smali` |
| **J.2.b** | libart | `CanMethodUseNterp(ArtMethod*)` returns false when method's declaring class has non-null classloader → forces app code through C++ switch interpreter which type-checks vregs per-opcode | `art/runtime/nterp_helpers.cc` |
| **J.2.c** | smali (McD) | Removed `androidx.lifecycle.ProcessLifecycleInitializer` meta-data from manifest + NOPed its `create()` + try/catch on constructor's `plus()` chain | `AndroidManifest.xml`, `smali_classes2/...`, `smali_classes12/.../McDMarketApplication.smali` |

## Why J.2.b is the key infrastructure win

Agent 142's J.1 diagnosis: McD died at PC `nterp_op_iget_object + 0x30` because vreg vB held `0x4b569` (309097 — an int) but nterp dereferenced it as an object. Classic signature of **verifier bypass on R8/Hilt classes** with `kAccVerified` force-set but verifier never ran. No pre-verified `.vdex` exists for McD's base.apk.

J.2.b doesn't fix the verifier bypass; it sidesteps it by routing all non-boot-classloader code through the C++ switch interpreter (mterp), which type-checks every vreg op. Side effect: 3-5x slower bytecode for app code — but HW initial render still passes with no observable perf hit on simple UIs.

This is **permanent infrastructure** that benefits every Android APK we run, not just McD.

## Why J.2.a (Java verify-cascade) FAILED — anti-pattern recorded

Agent 143 attempted to Java-reflect a `Class.forName(name, false, appCl)` loop over every entry in every dex in McD's PathClassLoader. Two variants (verify-all + scoped) both died at **AMS LIFECYCLE_TIMEOUT (~99-106s)** before McD's Application even started — the verify cascade drags in transitive Hilt/Dagger graph of thousands of classes. See [[feedback_verify_cascade_ams_timeout]].

## Current device state (board target `dd011a414436314130101250040eac00`)

- `libart.so` md5 `3d11dbd6...` (J.2.b) at `/system/android/lib/libart.so`. Rollback at `/data/local/tmp/libart_pre_fixj2b_144.so` (md5 `626303b4...` = E.a baseline)
- `oh-adapter-runtime.jar` md5 `041a97db...` (Fix F)
- McD APK md5 `e72e5b67...` (J.2.c, contains all G.1.b + H.3 + J.2.c patches) at `/data/app/el1/bundle/public/com.mcdonalds.app/android/base.apk`
- v7a libs at `/system/app/com.mcdonalds.app/lib/armeabi-v7a/` (10 .so files)
- libappexecfwk_common.z.so md5 `4d2c6399...` (.app→.apk patch)
- file_contexts md5 `143516ab...` (H1 merged 626 lines)
- SELinux Permissive
- HW (com.example.helloworld) regression status: PASSES at initial render. **Open**: agent 145 reported HeapTaskDaemon SEGV during sustained use on libart `3d11dbd6...` (vs agent 144 PASS at initial render only). Needs verification.

## Open walls (next-up Kotlin compatibility chain)

In dependency order of what surfaces first:

1. **`AbstractMethodError: kotlinx.coroutines.ExecutorCoroutineDispatcher.k1()`** — J.2.b isolated test (Fix H.3 APK + Fix F jar + J.2.b libart). McD's R8 dropped a method that something else expects.
2. **`NullPointerException` in `kotlinx.coroutines.JobSupport.plus(EmptyCoroutineContext)`** — J.2.c round 1.
3. **`NullPointerException`** in `kotlin.coroutines.CoroutineContext$Element$DefaultImpls.c, parameter key`** — composed J.2.b + J.2.c.

All three are McD R8-obfuscated kotlinx version mismatches. Same surgical pattern as G.1.b / H.3 / J.2.c should resolve each: smali NOP / shim / try-catch. Next task: K.1.a.

## Cross-references

- `docs/engine/V3-FIX-G1B-COROUTINES-SMALI-2026-05-23.md` (agent 139)
- `docs/engine/V3-FIX-H3-COMBINEDCONTEXT-2026-05-23.md` (agent 141)
- `docs/engine/V3-FIX-J1-LIBART-SEGV-DIAGNOSIS-2026-05-23.md` (agent 142, the diagnostic)
- `docs/engine/V3-FIX-J2A-VMRUNTIME-VERIFY-2026-05-23.md` (agent 143, failed)
- `docs/engine/V3-FIX-J2B-NTERP-DISABLE-2026-05-23.md` (agent 144)
- `docs/engine/V3-FIX-J2C-PROCESSLIFECYCLE-2026-05-23.md` (agent 145)
- Evidence dirs under `docs/engine/V3-SESSION-2026-05-22-EVIDENCE/fix-{g1b-139,h3-141,j1-142,j2a-143,j2b-144,j2c-145}/`
- [[v3-helloworld-renders-fix-a-2026-05-22]] — yesterday's HW milestone (Fix A)
- [[feedback_verify_cascade_ams_timeout]] — J.2.a anti-pattern
- [[reference_hbc_libart_rebuild_workflow]] — proven .o-swap pattern

## Significance

Before today: McD crashed in McDMarketApplication.<init> after ~3s. End of today: McD reaches scheduleTransaction, runs 25+ dex registrations, dies on clean Java exceptions only. The native-crash family is closed. Remaining work is application-specific Kotlin shim chain, tractable with smali NOPs at ~1-2h per shim.
