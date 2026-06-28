---
name: appspawn-x-critical-boot-bricks-v7-from-2026-05-20
description: "5 consecutive Stage-4 reboot bricks of DAYU200 V7 ROM caused by appspawn_x.cfg critical:[0] + start-mode:boot triggering init reboot-loop when appspawn-x fails. M-series mitigations are NOT the cause. Don't --reboot until appspawn-x ondemand isolation passes."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**Do NOT run end-to-end deploy with `--reboot` (Stage 4) on DAYU200 V7 ROM until appspawn-x ondemand isolation test passes.**

The default `appspawn_x.cfg` in [build-host]'s bundle declares appspawn-x as:
- `"critical":[0]` — init will reboot if this service fails
- `"start-mode":"boot"` — init starts immediately on boot, before recovery is possible

When appspawn-x fails to start (whatever the cause), init's critical-service handler triggers a recovery reboot, which fails, which loops, which leaves the board un-enumerable. USB endpoint disappears. Operator hard power-cycle or RKDevTool reflash required.

## 2026-05-21 SECOND CORRECTION: [build-host]'s OWN PROCEDURE ALSO BRICKS

Agent 101 ran [build-host]'s `deploy_stage.sh` AS-IS (max 2-line host-path adaptation). Stages 0-3.9 ALL PASS, 101 files md5+size verified, P-1 boot-image chcon to `system_lib_file:s0` confirmed applied to all 27 segments PRE-REBOOT (verified by `ls -lZ`). [build-host]'s Stage 3.5 internal `sync; reboot` → board soft-bricked. Same symptom class as our previous 6 bricks.

**Major implication:** the brick is NOT caused by deviating from [build-host]'s procedure. Our prior 6 bricks were NOT "we did the deploy wrong" — they were the SAME underlying problem [build-host]'s own procedure also hits on this specific board.

This means the bug is in our ENVIRONMENT vs [build-host]'s, not in our PROCEDURE:
- Different V7-images version baked-in than [build-host]'s dev board has
- Different bootloader / kernel / partition layout
- Different DAYU200 hardware revision
- [build-host]'s procedure assumes their own dev infrastructure (custom kernel? specific firmware? hand-tuning we don't have?)

**Real next investigation paths (none cheap):**
1. **Serial UART logging** through the reboot — hdc.exe goes silent at exact failure moment, only serial captures kernel + init through reboot
2. **Rescue-mode appspawn-x** from shell (start manually BEFORE reboot, observe live SIGABRT). Agent 99 tried this with ondemand cfg but board bricked first
3. **Static diff** of [build-host]'s V7 cfg files we deployed (appspawn_x.cfg, file_contexts, ld-musl-namespace-arm.ini, appspawn_x_sandbox.json) vs whatever [build-host]'s V7-images expects them to look like
4. **Cross-check V7-images version** with [build-host] — we may have a stripped/older variant

Until one of these surfaces a concrete failure mechanism, ALL deploy attempts on this V7 will likely brick. Don't run another full-deploy-with-reboot without one of (1)/(2)/(3)/(4) progressing first.

## 2026-05-21 CORRECTION: hypothesis REFUTED

Agent 99 isolation test: deployed V3 with `appspawn_x.cfg` MODIFIED to `critical:[]` + `start-mode:"ondemand"` (verified on-device). NO --reboot. Then manual `hdc target boot`. Board NEVER came back (>22 min). Same brick class.

**appspawn-x critical:[0]+boot is NOT the sole cause.** Something else in Stages 3b-3f's artifacts also triggers the boot failure. The original feedback rule below was directionally correct (don't --reboot until cause isolated) but wrong about WHICH cause.

Real next-cause-candidates (per agent 99 recommendation):
1. **Other 3 cfgs in Stage 3f** may declare their own critical:[N] services — audit all 4 cfgs
2. **`jobs[]` array** in appspawn_x.cfg runs at boot trigger even with service ondemand — strip it and re-test
3. **`/system/android` dir** triggers a first-boot SELinux relabel scan that fails
4. **A shim .so** violates an init invariant only surfacing at first boot

Required next step: **single-stage bisection** (deploy 3b only + reboot, 3c only + reboot, etc.) to find the actual bricking artifact set. Each bisection = potential brick = operator-recovery cost. NO more confident "cannot brick" framings until we have bisection data.

## Empirical evidence (2026-05-20)

Five consecutive end-to-end attempts hit Stage 4 reboot brick:
- agent 94: died at Stage 3f Channel A (M6 mitigated this in M7)
- agent 96: Stage 3f green (M7 architectural fix), aborted on restorecon -R unsupported (trivial M7-v2 fix)
- agent 97: M7-v2 green, Stage 4 reboot, board came back, Stage 6 failed (misdiagnosed as "/system reverted")
- agent 98: M7-v2 green, all pre-reboot SELinux labels CORRECT, Stage 4 reboot → board NEVER returned (worse symptom, same cause)

The empirical proof M-series is correct:
- Agent 98 Stage 3.7 verified 15 hot-path SELinux labels stuck
- Agent 98 Stage 3.9 verified 101 files md5+size+no-drwx
- Pre-reboot: `/system/bin/appspawn-x` had `u:object_r:appspawn_exec:s0` (CORRECT label)
- Pre-reboot: dual-path libs all correct contexts
- The reboot itself is what bricks, not deploy robustness

This pattern matches `feedback_additive_shim_vs_architectural_pivot.md`: 3+ retries in same layer without end-to-end progress = pivot the layer. I (orchestrator) failed to recognize the signal in time; dispatched 5 retries when I should have stopped at 3 and pivoted to isolation testing.

## How to apply

**Before any end-to-end retry with --reboot on V7:**

1. **Modify appspawn_x.cfg** to be non-critical + ondemand BEFORE deploying:
   - `"critical":[]` (not `[0]`)
   - `"start-mode":"ondemand"` (not `"boot"`)
   - Then init won't reboot-loop if appspawn-x fails; it just won't start the service
2. **Deploy WITHOUT --reboot** flag — exit script after Stage 3.9, do NOT issue reboot
3. **Soft reboot via `hdc target boot` manually** — proven to preserve /system writes per agent 80 (factory ROM) and just-now-marker-test (V7 ROM)
4. **After reboot**: manually `init.start appspawn_x` and capture crash signature via dmesg/hilog. THIS is the diagnostic we need to know WHY appspawn-x fails on V7
5. Only AFTER appspawn-x boots cleanly under isolation, switch cfg back to critical:[0]+start-mode:boot

**When deploying onto V7 in general:**
- Recognize [build-host]'s appspawn_x.cfg semantics assume [build-host]'s own dev environment (where appspawn-x has been validated to start cleanly). On clean V7 baseline without [build-host]'s iterative dev cycle, it may fail
- Capture appspawn-x crash signature (likely missing transitive .so, ABI mismatch, or SELinux denial) BEFORE re-enabling critical/boot

## Cross-references

- [[feedback_additive_shim_vs_architectural_pivot]] — the discipline rule I failed to apply; 3+ retries in same layer = pivot signal
- [[feedback_risky_productive_over_safety_theater]] — risky-productive doesn't mean ignore-pivot-signals; should have pivoted at attempt 3
- [[feedback_soft_brick_w2_2026-05-16]] — original W2 brick had same family (reboot triggers cascading failure)
- [[project_v3_phase_1b_blocked_typeface_segv]] — separate issue but similar pattern: substrate misalignment we need to isolate empirically
- `docs/engine/V3-W2-E2E-98-REPORT.md` — agent 98's empirical capture
- `docs/engine/V3-W2-E2E-97-M7v2-REPORT.md` — agent 97's misdiagnosis we corrected
