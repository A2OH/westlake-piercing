---
name: sysfreeze-vs-stderr-diagnostic-2026-05-27
description: "FEEDBACK — When McD (or any AOSP app) hits LIFECYCLE_TIMEOUT, look in hilog [B43-BIND] trace FIRST for any InvocationTargetException with full stack, NOT in sysfreeze stack snapshots. Sysfreeze main-thread stack shows DOWNSTREAM symptom (nativePollOnce park) because the [B47-SLA] gate SKIPs silently after bind failure — AMS doesn't know bind failed and schedules foreground anyway, leaving a zombie main thread parking on a MessageQueue that no one will ever post to. Real wall is whatever threw in handleBindApplication; the park is just the dying breath."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

When diagnosing AOSP-app LIFECYCLE_TIMEOUT on Westlake, **start with hilog `[B43-BIND]` trace, not with the sysfreeze main-thread stack**.

DON'T: read sysfreeze.log main-thread stack, see `nativePollOnce(epoll_wait)`, frame the problem as "Handler/Looper dispatch wall" or "MessageQueue eventfd broken."

DO: pull hilog with `hilog -x`, grep for `B43-BIND` and `B47-SLA` and `InvocationTargetException`. The chain reveals the real wall:

```
[B43-BIND] ensureBindApplication start
[B43-BIND] resolved ApplicationInfo ...
[B43-BIND] providers populated: N
[B43-BIND] AppBindData constructed
... (Application.onCreate runs here) ...
[B43-BIND] ensureBindApplication FAILED: java.lang.reflect.InvocationTargetException
  Caused by: RuntimeException: Unable to create application X: <some Java exception>
  Caused by: <root cause exception>
    at <stack trace of the actual wall>
[B47-SLA] ENTRY recordId=N
[B47-SLA] SKIP: ensureBindApplication did not complete (sBindAppDone=false); refusing to scheduleTransaction
AMS ForegroundLifecycle ... (AMS doesn't know we failed)
AppMS com.X foregrounded
LIFECYCLE_HALF_TIMEOUT (25s later)
LIFECYCLE_TIMEOUT (50s after foregrounding)
kill reason=Reason:LIFECYCLE_TIMEOUT
```

The real wall is the `Caused by:` chain. The nativePollOnce stack in sysfreeze is a zombie symptom.

## Why

Westlake's Fix A.2 (B47-SLA gate, 2026-05-24) prevents `scheduleTransaction` when `sBindAppDone == false` — correct behavior, but it SKIPS SILENTLY. AMS treats the lack of explicit failure response as success, schedules the foreground lifecycle event, and the zombie main thread parks in MessageQueue waiting for a message that will never come.

Without the [B43-BIND] trace, you only see:
- Sysfreeze: main thread in nativePollOnce
- Child stderr: huge volume of vtable activity (real progress!) then thread spawning
- Conclusion: "stuck in MessageQueue, must be looper/handler dispatch issue"

This is wrong. The looper IS fine. There just isn't going to BE any next message because bind failed before LaunchActivityItem could be scheduled.

## How to apply

When McD (or any AOSP app) hits LIFECYCLE_TIMEOUT:

1. `$HDC shell "hilog -x" > hilog.txt` IMMEDIATELY after the timeout fires
2. `grep -E "B43-BIND|B47-SLA|InvocationTargetException|Caused by|at " hilog.txt | head -40`
3. The InvocationTargetException's `Caused by` chain is the actual wall
4. Sysfreeze logs are useful for SECONDARY diagnosis (e.g., "was the binder thread blocked", "what other threads exist") but NEVER as primary

The Westlake-side [B43-BIND] tracepoints are in adapter-runtime-bcp.jar's AppSchedulerBridge.ensureBindApplication. They were added per `feedback_engine_principle_validation_2026-05-24.md` — these tracepoints are LOAD-BEARING for diagnosis.

## Concrete example

2026-05-27 sysfreeze diagnostic said main thread parks in MQ_nativePollOnce → led to misframing as "Handler/Looper dispatch wall." Spent 2 cycles chasing the dispatch wall before bisect-3 / B-fix-1 / B-fix-2 reframed.

Later same day, the CD-redeploy-retry agent read hilog [B43-BIND] correctly and found:
```
ensureBindApplication FAILED: InvocationTargetException
  Caused by: IllegalStateException: Method addObserver must be called on the main thread
    at androidx.lifecycle.LifecycleRegistry.j
    at McDMarketApplication.onCreate:75
```

This is the ACTUAL wall. The MessageQueue park was a downstream zombie state.

## Cleaner failure path (B47-SLA gate improvement)

Per the retry agent's recommendation (worth tracking): improve the [B47-SLA] gate to REPORT bind failure back to AMS instead of silently SKIPping. AMS would kill the app immediately (no 50s wait) and the failure mode becomes loud and clearly attributable.

## See also

- [[v3-fix-j2-gp-landed-2026-05-27]] — original (wrong) diagnostic that misframed nativePollOnce as the wall
- [[v3-cd-redeploy-2026-05-27]] — retry agent's corrected hilog-based diagnostic
- [[feedback-engine-principle-validation-2026-05-24]] — where [B43-BIND]/[B47-SLA] tracepoints came from
