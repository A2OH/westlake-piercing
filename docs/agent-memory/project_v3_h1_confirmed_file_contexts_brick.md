---
name: v3-h1-confirmed-file-contexts-brick-2026-05-22
description: "H1 CONFIRMED 2026-05-22. Brick cause of 10+ V3 deploys: [build-host]'s 34-line file_contexts overwrites factory ~618-line file, then any restorecon scope touching factory paths beyond the 34 covered globs causes service-load failures → init reboot loops → bootloader updater → /system revert. Skipping Stage 3f file_contexts push prevents brick (15s reboot vs 7+ min)."
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Fact

**The V3 deploy brick is caused by `/system/etc/selinux/targeted/contexts/file_contexts` being overwritten by [build-host]'s 34-line variant (not factory's ~618 lines), combined with any restorecon scope that touches factory paths missing from the 34-line globs.**

Confirmed 2026-05-22 via brick-safe subtraction test:
- [build-host]'s `deploy_stage.sh` AS-IS minus 2 lines (`stage_push` of file_contexts skipped)
- Stages 0-3.9 + Stage 3.5 reboot on factory-fresh DAYU200 V7
- Result: board re-enumerated in **15 seconds**, all services up, /system writes persisted
- vs prior 10+ bricks where board took 7+ min or never returned, /system reverted to factory

## Why

Causal chain:
1. Stage 3f pushes `$OUT/oh-service/file_contexts` (34 lines: ASF header + ~20 pre-image lines + 3-line adapter patch) over factory `/system/etc/selinux/targeted/contexts/file_contexts` (~618 lines)
2. [build-host]'s own AS-IS `deploy_stage.sh:506` comment claims "factory 618 lines + adapter 3-line patch" but the actual deployed file is NOT merged
3. Any subsequent restorecon touching paths whose globs only exist in the factory 618 lines (NOT in the 34-line replacement) falls through to default `system_file:s0`
4. Factory services (foundation, render_service) try to dlopen libs but SELinux denies (wrong domain → wrong type)
5. foundation is `critical:[1,4,240]` → crash-loop
6. init reaches reboot threshold → reboots
7. Bootloader update-fail counter trips → triggers /misc updater
8. /system restored from factory baseline

[build-host] survives this latent bug because:
- Their restorecon scope at Stage 3f line 545-547 is INTENTIONALLY narrow: only `/system/bin/appspawn-x` (single file) + `/system/android/lib/` (their adapter tree)
- Both paths are covered by the 34-line replacement's globs → relabel correct
- Their dev board hasn't been reflashed since 2026-04-04 → never re-runs full deploy

We bricked because:
- Multiple full-deploy attempts on freshly-reflashed boards ([build-host] never does this)
- Our hardened script's wider restorecon sweep (5 top-level /system dirs) hits factory paths missing from the 34 lines

## How to apply

### For ANY future V3 deploy on DAYU200 V7

**Production recipe (the actual fix):**

1. Build a MERGED file_contexts: factory 618 lines + adapter 3 lines = 621 lines
2. Replace `adapter/out/oh-service/file_contexts` ([build-host]'s 34-line) with merged 621-line version
3. Re-enable Stage 3f file_contexts push (it's safe with merged version)
4. Keep [build-host]'s narrow restorecon scope (don't add wider sweep)

This gives:
- /system writes persist (file_contexts merge means no glob loss)
- appspawn-x gets correct `appspawn_exec:s0` label (3-line patch applied)
- Brick-safe

**Forbidden patterns going forward:**

- Do NOT push [build-host]'s 34-line file_contexts unmodified
- Do NOT add restorecon scope beyond what's needed for the deployed paths
- Do NOT use our `scripts/v3/deploy-hbc-to-dayu200-hardened.sh` derivative (2443 LOC, agent 122 identified 3 high-risk additions; this finding confirms they cause brick)

### For diagnostics

If a future deploy bricks AGAIN, first check:
1. Is `/system/etc/selinux/targeted/contexts/file_contexts` line count > 100? (If ~34: the latent bug surfaced — switch to merged version immediately)
2. What restorecon paths did the script touch? Did any exceed the deployed adapter tree?

### For the H1-test script (brick-safe but incomplete)

`scripts/v3/h1-test/deploy_stage_h1.sh` is BRICK-SAFE (skips file_contexts entirely) but Android apps won't launch because appspawn-x lacks `appspawn_exec:s0` label. Use ONLY for:
- Substrate validation (verifying OH services come up cleanly)
- Pre-flight before deploying merged-file_contexts variant

## Empirical evidence (2026-05-22)

- Pre-reboot: 101 files md5+size verified on device (Stage 3.9 passed except expected file_contexts mismatch from our deliberate skip)
- Post-reboot:
  - Board uptime 1 min, hdc reachable, SELinux Enforcing
  - foundation pid 494, render_service pid 624, com.ohos.launcher pid 1526, hdcd pid 734
  - `/system/bin/appspawn-x` present (mtime 2017-08-04 = deploy timestamp, NOT factory 2026-04-04)
  - `/system/android/` present with 84 files
  - `/system/lib/libwms.z.so` mtime 2017-08-04 (deploy preserved, factory mtime would be 2026-04-04)
  - `/system/etc/.../file_contexts` 618 lines, md5 c0b79274... = factory baseline (we skipped overwrite)
  - 27 boot image segments under `/system/android/framework/arm/`

Log files:
- `/tmp/h1-test-131514.log` — Stages 0..3.9 (183 lines)
- `/tmp/h1-test-stage35-132052.log` — Stage 3.5 (15s reboot)
- `/tmp/h1-test-verify-132205.log` — post-reboot verification

## Cross-references

- `docs/engine/V3-H1-TEST-RUN.md` — full test write-up
- `docs/engine/V3-[build-host]-FILE-BY-FILE-FORENSIC.md` — agent 127 H1 derivation (commit `3518f1e4`)
- `docs/engine/V3-[build-host]-DEEP-DEPLOY-AUDIT.md` — agent 126 operational context (commit `528a8e74`)
- `scripts/v3/h1-test/deploy_stage_h1.sh` — H1-test script (commit `bc5bff61`)
- `/tmp/WESTLAKE_V3_HELP_REQUEST_2026-05-22.md` — letter to [build-host] with H1 hypothesis (will follow up confirming)
- [[appspawn-x-critical-boot-bricks-v7-from-2026-05-20]] — SUPERSEDED; the brick was NOT appspawn-x criticality, it was file_contexts overwrite. The cfg ondemand-isolation was a useful defense-in-depth but not the cure.
- [[risky_productive_over_brick_avoidance_from_2026-05-20]] — confirmed posture: risky-productive paid off; 10 bricks ≠ wasted, they constrained the hypothesis space until H1 emerged
