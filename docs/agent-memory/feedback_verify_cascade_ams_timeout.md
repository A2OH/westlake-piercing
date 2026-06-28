---
name: feedback-verify-cascade-ams-timeout
description: "Java Class.forName(name, false, appCl) loop over every entry in every dex of an R8-shrunk app's PathClassLoader will dragnet the transitive class graph and exceed AMS LIFECYCLE_TIMEOUT (~100s) before the Application even starts. Don't use Java-side verify-all as a workaround for native verifier bypass."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**Do NOT attempt to force-verify an R8/Hilt-shrunk Android app's classes via a Java-side `Class.forName(name, initialize, appCl)` loop over its full PathClassLoader dex inventory.** It will not complete within AMS's `LIFECYCLE_TIMEOUT` window (~99-106s on OHOS DAYU200) and the app will be SIGKILLed before its Application.onCreate runs.

**Why**: Modern Android apps with Hilt/Dagger have transitive class graphs of thousands of classes. Each `Class.forName(name, false, cl)` triggers verification which transitively resolves every referenced type. The cascade is unbounded for any seed class deep in the DI graph. Even scoped (prefix-match) variants hit the same wall because the first Hilt class loaded brings everything else with it.

**How to apply**: When ART's nterp interpreter SEGVs on a class that bypassed verification (e.g., the `nterp_op_iget_object` family at PC `0x6bcab0` with vreg holding an int instead of an object pointer per Agent 142's J.1 diagnosis 2026-05-23), reach for libart patches instead:

- Preferred: patch `art::interpreter::CanMethodUseNterp(ArtMethod*)` to return `false` for any method whose declaring class's classloader is not the boot classloader. Forces app code through the C++ switch interpreter which type-checks vregs per-opcode. ~½d, drop-in libart.so swap, proven by Fix J.2.b (agent 144 commit `4f2162d1`, libart md5 `3d11dbd6...`).
- Alternative: targeted smali NOP on the specific ContentProvider/initializer that triggers the SEGV (~1-2h, single-app workaround — does not address systemic issue).
- **Avoid**: any Java-reflection loop that visits "every class in the app". This is the J.2.a failure pattern.

Specific OHOS notes:
- `dalvik.system.VMRuntime.setVerifyMode(boolean)` and `setVerifyEnabled(boolean)` BOTH `NoSuchMethodException` in OHOS AOSP-15 libcore (agent 143 verified) — these AOSP APIs are not exposed.
- AMS `LIFECYCLE_TIMEOUT` is `persist.sys.abilityms.timeout_unit_time_ratio` × base. Even bumping the ratio to 10 doesn't buy enough headroom for full-graph verification of an R8 app.

## Reference incident

Agent 143 (2026-05-23) implemented Fix J.2.a verify-cascade as Java patch in `oh-adapter-runtime.jar`. Built two variants:
- v1 (verify-all 13 dex): McD pid 22155 lived 146s, killed.
- v2 (scoped 14 prefix-match packages + 90s self-deadline): McD pid 22419 lived 106s, killed.
Both hit `AAFWK LIFECYCLE_TIMEOUT (SplashActivity load timeout)` at ~99s after `AppScheduler::ScheduleLaunchApplication`. The verify cascade never completed; McD's Application.onCreate never ran. Reverted to Fix F jar md5 `041a97db...`. Full report at `docs/engine/V3-FIX-J2A-VMRUNTIME-VERIFY-2026-05-23.md`.

User then dispatched J.2.b (libart patch) + J.2.c (smali NOP) in parallel; J.2.b eliminated the nterp SEGV permanently with HW regression PASS.
