---
name: v3-chroot-phase1a-substrate-validated-2026-05-19
description: "2026-05-19 milestone: V3 chroot substrate validation PASSED on factory-clean DAYU200. 5 mounts (3 RW: proc/sys/dev; 2 RO: lib/system/lib). SELinux Enforcing throughout. [v3-chroot-launch] marker emitted. Brick-impossible architecture validated end-to-end. Phase 1a validates substrate ONLY — engine question (does an Activity launch?) still open; needs Phase 1b ($V3_CHROOT_HELLO_CMD recipe) or Phase 1c ([build-host] AMS replacement)."
metadata:
  node_type: memory
  type: project
  date: 2026-05-19
  triggers:
    - V3
    - DAYU200
    - chroot
    - W2
    - Phase-1a
  originSessionId: agent-70-2026-05-19
---

# Milestone

**2026-05-19:** V3 chroot substrate validation PASSED end-to-end on a
factory-clean DAYU200 (commit `0c9e7532`). This is the first empirical
confirmation that the brick-impossible deploy model from the chroot
containment proposal works on a real board — and that the 2026-05-16
W2 soft-brick recovery path is sound.

This is a **substrate** milestone, not an **engine** milestone. The
runtime files are in place, mounts are correct, SELinux is happy,
the chroot can exec a binary. Whether that binary can actually launch
a Java Activity is the W4-empty question (see [[handoff_2026-05-19]]
finding #6) — not yet answered.

# What PASSED

- 5 bind-mounts active under `/data/local/tmp/v3-hbc-chroot/`:
  - 3 RW: `/proc`, `/sys`, `/dev`
  - 2 RO: host `/lib`, host `/system/lib` (per
    [[feedback_chroot_dynamic_elf_ro_bind]])
- SELinux Enforcing throughout (the proposal's `--setenforce-0` fidelity
  caveat was unnecessary for Phase 1).
- `[v3-chroot-launch]` marker emitted in hilog from a binary executed
  inside the chroot.
- Smoke script idempotent: re-runnable without state corruption.
- Factory image NEVER written: rollback = `umount` + reboot.

# Empirical evidence

- Commit `0c9e7532` — Phase 1a PASS + smoke script
- Commit `ad52b63d` — Stage 3 RO bind-mount fix
- Commit `f10ee81b` — hdc_shell_check helper (3 control-flow sites fixed
  during bring-up)
- Commit `53a78196` — chroot containment proposal landed
  (`docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md`, 805 LOC)
- Commit `f158cf58` — hdc 3.2.0b probe report (H2 from soft-brick DIES)
  (`docs/engine/V3-HDC-3.2.0B-PROBE-REPORT.md`)

# Architecture (Phase 1)

```
DAYU200 factory image (untouched)
  ├── /system, /vendor (untouched, brick-safe)
  ├── /data/local/tmp/v3-hbc-chroot/
  │     ├── usr/bin/, usr/lib/, etc/  ([build-host] bundle artifacts)
  │     ├── proc/                     (bind-mount RW)
  │     ├── sys/                      (bind-mount RW)
  │     ├── dev/                      (bind-mount RW)
  │     ├── lib/                      (bind-mount RO, host /lib)
  │     └── system/lib/               (bind-mount RO, host /system/lib)
  └── SELinux Enforcing (unmodified policy)
```

~91% of [build-host] bundle is TRIVIAL_RELOCATE per the chroot containment
proposal — only ~9% needs Phase 2 (/system) consideration.

# What's STILL OPEN

**Engine question** (W4-empty, per [[handoff_2026-05-19]] finding #6):
Does the runtime actually launch an Android Activity from inside the
chroot? Not yet answered because [build-host] bundle ships ZERO chroot-bypass
binaries (no `aa`, `bm`, `dalvikvm`, `apk_runner`). Reaching
`MainActivity.onCreate` from the chroot needs ONE of:

- **Phase 1b** (½-day): cross-compile `test_tlv_client` against
  chroot-resolvable libs; invoke via
  `chroot /data/local/tmp/v3-hbc-chroot /system/bin/sh -c "$V3_CHROOT_HELLO_CMD"`.
  The `$V3_CHROOT_HELLO_CMD` recipe is the deliverable. Cheap; high-value
  engine evidence.

- **Phase 1c** (½-week+): replace [build-host] AMS with a chroot-compat launcher.
  Heavier; only if Phase 1b doesn't satisfy.

- **Phase 2** (deferred): /system writes for proven artifacts. Defer
  until Phase 1b/1c lands engine evidence — brick-safe path of least
  regret.

# Why this matters

- **First brick-safe substrate proof.** The W2 soft-brick taught the
  team that direct /system writes are too dangerous for unproven
  artifacts. Phase 1a proves the chroot alternative actually works.
- **Phase 1 / Phase 2 gating is empirically grounded.** Before today,
  Phase 2 was the only option; now Phase 2 is a layering choice
  conditional on Phase 1b/1c success.
- **hdc 3.2.0b cleared as a side effect** ([[handoff_2026-05-19]]
  finding #2). The 200-iteration probe ran during bring-up and produced
  the artifact that kills H2 from the W2 postmortem.
- **3 anti-patterns surfaced and bounded** (hdc_shell silent-NOOP via
  host exit code, `|| true` chcon, per-binary ldd walking). All three
  are now codified rules: [[feedback_hdc_shell_check_pattern]] and
  [[feedback_chroot_dynamic_elf_ro_bind]] today; the chcon rule was
  already in [[feedback_soft_brick_w2_2026-05-16]] (rule 2).

# Not pivot evidence

3 retries in different layers (hdc_shell_check fix, RO bind-mount add,
smoke script idempotence) all fixed in brick-safe domain. That's
iteration, not symptom-rotation per [[feedback_two_pivots_in_two_days]].
The V3 [build-host]-runtime-reuse direction stands; chroot containment is the
deploy-layer choice, not an architectural pivot.

# Read these for context

1. `docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md` — strategic Phase 1/2
   split + 91% TRIVIAL_RELOCATE analysis
2. `docs/engine/V3-W4-EMPTY-HELLOWORLD-INVOCATION.md` — why Phase 1a
   substrate doesn't auto-answer the engine question
3. `docs/engine/V3-HDC-3.2.0B-PROBE-REPORT.md` — H2-from-W2-postmortem
   death certificate
4. `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` — the brick-safe deploy
   driver (read `hdc_shell_check` at lines 198-213 and the Stage 3 RO
   bind-mount block)
5. [[handoff_2026-05-19]] — current 5-minute orientation
6. [[feedback_hdc_shell_check_pattern]] — control-flow safety rule
7. [[feedback_chroot_dynamic_elf_ro_bind]] — Stage-3 lib bind rule
8. [[project_v3_hbc_reuse_direction]] — V3 strategic direction
   (chroot containment now noted as the deploy model)

# Cross-references

- [[handoff_2026-05-19]] — orientation
- [[project_v3_hbc_reuse_direction]] — strategic direction (updated)
- [[feedback_hdc_shell_check_pattern]] — bring-up forged this rule
- [[feedback_chroot_dynamic_elf_ro_bind]] — bring-up forged this rule
- [[feedback_soft_brick_w2_2026-05-16]] — what this milestone closes the
  loop on
- [[feedback_two_pivots_in_two_days]] — pivot discipline held
- `docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md`
- `docs/engine/V3-W4-EMPTY-HELLOWORLD-INVOCATION.md`
- `docs/engine/V3-HDC-3.2.0B-PROBE-REPORT.md`
- `docs/engine/WESTLAKE-ISLAND-BORROW-MAP.md`
- `docs/engine/03-REQUIREMENT-INDEX.md`
- commits: `0c9e7532`, `ad52b63d`, `f10ee81b`, `53a78196`, `f158cf58`,
  `9705487c`, `caa3fd56`, `ca7c03bd`, `77c9540e`
