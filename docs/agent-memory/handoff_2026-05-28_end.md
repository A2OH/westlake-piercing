---
name: handoff-2026-05-28-end
description: "End-of-day handoff for marathon session 2026-05-28. 15+ walls closed. McD bind execution 0s → 22s of real Java on main thread. Application.onCreate completes (Kochava/Split/Apptentive/Hilt all run). LaunchActivityItem dispatches. Dies at Activity.attach → ContextThemeWrapper.getSystemService LinkageError (UNVERIFIED — could be DiagHL artifact). Substrate stable, recoverable. Tomorrow: STEP 1 verify LinkageError on clean substrate (15 min). Then Path A (libart vtable extension for ContextThemeWrapper.getSystemService) OR Path B (reduce 22s bind time OR synthesize WindowMgrClient.createSession). Full doc at docs/engine/HANDOFF-2026-05-28-END.md."
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## START HERE tomorrow

Read `$HOME/openharmony/docs/engine/HANDOFF-2026-05-28-END.md` for full state.

## TL;DR

- **15+ walls closed today**. McD bind execution went 0s → 22 seconds of real Java on main thread.
- **Substrate stable**. All fixes deployed. Pre-snapshots retained at `/data/local/tmp/pre-*/`.
- **Current dying point**: Activity.attach → ContextThemeWrapper.getSystemService LinkageError. **UNVERIFIED** — last observation came from DiagHL-instrumented framework.jar that was rolled back without re-test.
- **STEP 1 tomorrow** (15 min): launch McD on current clean substrate, check hilog for `LinkageError | ContextThemeWrapper`. If repeats → real wall, fix space = libart vtable extension. If not → DiagHL artifact, back to W12-G's diagnosis (22s bind exceeds AMS timeout).

## Final substrate (verified)

| File | md5 |
|---|---|
| libart.so | `82429901` (W12-G) |
| adapter-runtime-bcp.jar | `e0f01b23` (J.1+W11+W12-A+W12-B) |
| oh-adapter-framework.jar | `f80cf012` (W7+W12-B+W12-E) |
| adapter-mainline-stubs.jar | `38ff18ed` (W5) |
| framework.jar | `a7b6f91c` (W8+W12-D) |
| core-oj.jar | `e923545e` (W2 v3) |
| liboh_android_runtime.so | `4f5ea5c8` both paths (CD) |
| boot.oat | `f7e83ad9` (W12-E regen) |
| boot-framework.oat | `295c10d9` |
| boot-oh-adapter-framework.oat | `256339c5` |
| appspawn-x | `3abe3bde` (J.2-G+ baseline) |

## Today's mis-framings (don't repeat)

1. **W2 series (~6h)**: chased Proxy.clone() at line 132; real wall was VTA-1 libart proxy-name at different line (197) in same R8-inlined method. Rule: record `:NNN` before AND after fix.
2. **W12-F (~1h)**: chased "bad_alloc loop" from processdump snapshot; instrumentation showed ZERO bad_alloc throws. Rule: stack snapshot ≠ root cause; verify via instrumentation.
3. **Handler/Looper park (recurring)**: chased "Looper broken" multiple times; deep diag showed Looper dispatches normally. Main thread is BUSY in Application.onCreate for 22s straight, not blocked. Rule: "main thread in nativePollOnce" → is it WAITING for a message or just hasn't returned to Looper yet?

## Walls closed (15+)

1-12-G chain — see HANDOFF doc table. All fixes deployed and verified at substrate level.

## Engineering rules added today (each in own memory file)

- `feedback_dlopen_in_child_allocates_2026-05-27`
- `reference_liboh_android_runtime_dual_path_2026-05-27`
- `feedback_aosp_version_jni_signature_drift_2026-05-27`
- `feedback_jni_stub_functions_silent_failure_2026-05-27`
- `feedback_sysfreeze_vs_stderr_diagnostic_2026-05-27`
- `feedback_proxy_interfaces_modification_breaks_marksweep_2026-05-28`
- `feedback_r8_inlined_stacks_hide_root_cause_2026-05-28`
- `feedback_appspawnx_recovery_traps_2026-05-28`

## Validated methodologies

- **Predictive audit** (V3-MCD-PREDICTIVE-AUDIT-2026-05-27.md): identified 23 walls in one cycle; today closed 15+ matching predictions.
- **Top-down enumeration** (V3-MCD-ONCREATE-TOP-DOWN-2026-05-28.md): decomposed McD onCreate; correctly predicted W17 Typeface wall.
- **Modify-existing-class is safe; new class indices are dangerous** (Fix A allocation rule family).
- **hilog [B43-BIND] discipline**: read this FIRST, not sysfreeze stack.

## Tomorrow's first 30 minutes

```bash
# Verify substrate (5 min)
HDC=/mnt/c/Users/<user>/Dev/ohos-tools/hdc.exe
$HDC shell "md5sum /system/android/lib/libart.so /system/android/framework/adapter-runtime-bcp.jar /system/android/framework/oh-adapter-framework.jar /system/android/framework/adapter-mainline-stubs.jar /system/android/framework/framework.jar /system/lib/liboh_android_runtime.so /system/android/framework/arm/boot.oat"
$HDC shell "pidof appspawn-x" || $HDC shell "nohup /data/local/tmp/start_asx.sh > /dev/null 2>&1 &"

# Run McD with hilog (15 min)
$HDC shell "hilog -r"
$HDC shell "aa force-stop com.mcdonalds.app; sleep 1; aa start -b com.mcdonalds.app -a com.mcdonalds.mcdcoreapp.common.activity.SplashActivity"
sleep 90
$HDC shell "hilog -x" > /tmp/hilog_baseline.txt
grep -E "LinkageError|ContextThemeWrapper|getSystemService.*mismatch|Caused by" /tmp/hilog_baseline.txt | head -20

# Decide path A (LinkageError real) or path B (DiagHL artifact)
```

## See also

- [[v3-mcd-2026-05-27-day-end]] — yesterday's checkpoint (which today extended significantly)
- [[v3-mcd-2026-05-27]] — morning's 5-wall closure
- All [[fix-*-2026-05-28]] memory rules above
