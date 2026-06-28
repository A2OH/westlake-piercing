---
name: hardened-script-gates-8-to-13-atomicity-for-system-deploys
description: "When porting a deploy from brick-safe chroot to brick-recoverable /system, add 6 atomicity / recovery gates on top of the 4 chroot deploy-safety fixes. Gates 8-13: chcon snapshot/restore, atomic file swap via mv rename for live .so, mount-restore, force-stop OH Photos before display-touching, loop-budget on launch, processdump probe in Stage 0. The hardened script at commit 73ae3ac1 is the canonical reference implementation."
metadata:
  node_type: memory
  type: feedback
  date: 2026-05-19
  triggers:
    - V3
    - DAYU200
    - deploy-script
    - /system
    - brick-recovery
    - hardened-script
    - atomicity
  blast_radius: any-system-touching-deploy-script-on-OHOS-DAYU200
  originSessionId: agent-77-2026-05-19
---

# Rule

**Any future deploy script that writes to the OHOS `/system` partition
MUST adopt all 6 atomicity / recovery gates below (gates 8-13), in
addition to the 4 chroot deploy-safety fixes (hdc_shell_check, RO
bind-mount idiom, dual-path manifests, host-side awk script generation).
The hardened script at commit `73ae3ac1` (1707 LOC, ported from the
1190 LOC chroot script at commit `0c9e7532`) is the canonical reference
implementation.**

# Why

**Concrete: 2026-05-19 user pivot from chroot Phase 1b to /system path B**
after [[project_v3_phase_1b_blocked_typeface_segv]].

Chroot is **brick-impossible** by construction: it never touches
`/system`, so mid-deploy interrupt = leave the chroot half-mounted,
worst case is `umount` + reboot. /system is **brick-recoverable** but
not brick-impossible: mid-deploy interrupt = inconsistent /system
state = potentially brick. ROM flash is the worst-case escape but
costs ~1 hour.

The 4 chroot fixes (gates 1-7 conceptually, but only 4 were
needed under chroot because chroot didn't have the brick-recovery
problem):

- `hdc_shell_check` ([[feedback_hdc_shell_check_pattern]])
- RO bind-mount of host lib paths ([[feedback_chroot_dynamic_elf_ro_bind]])
- Dual-path manifests (chroot path AND host /system path) — port-only
  hack; under /system it just means writing the canonical path
- Host-side awk script generation (Fix C) — was already correct under
  chroot, no change for /system

**…are necessary but not sufficient when /system is in scope.** /system
adds 6 new brick mechanisms each gate addresses:

| Gate | Brick mechanism | Recovery primitive |
|------|----------------|---------------------|
| 8 | mid-deploy chcon drift leaves SELinux contexts wrong | snapshot before, restore on abort |
| 9 | overwriting an already-mapped `.so` mid-process-life crashes that process | atomic `mv` rename onto a 17-entry LIVE_SERVICE_SO allowlist (new path + rename = COW for the live process) |
| 10 | mid-deploy bind-mounts left dangling | mount-restore subcommand reverts to factory mount table |
| 11 | OH Photos holds the display surface; new artifact can't touch fb | force-stop OH Photos before any display-touching step (Island pattern from `demo/run-live.sh:L60-62`) |
| 12 | engine launch hangs forever consuming board | loop-budget caps launch at 60s default; abort + restore on timeout (Island pattern from `demo/run-live.sh:L71`) |
| 13 | engine crashes silently (the Phase 1b typeface SEGV symptom) | Stage 0 probe asserts `processdump` is present and runnable, so any later SEGV produces a backtrace |

# How to apply

## Adopt all 6 gates

For any new /system-touching deploy script on OHOS DAYU200 (or any
OHOS board where similar atomicity matters), the gates are not
optional — they're a structural requirement. The hardened script's
Stage 0 (preflight) runs gates 8, 11, 12, 13 prereqs; the per-Stage
write loop uses gates 9, 10 as the write primitive; gate 8 wraps the
whole script as a snapshot-restore boundary.

## New subcommands the gates introduce

- `restore-chcon` — restore SELinux contexts from the snapshot taken
  by gate 8 at Stage 0.
- `restore-mounts` — revert mount table to factory (gate 10).
- `probe-processdump` — gate 13's standalone check; useful for
  pre-flight on a board you don't know the state of.
- `--snapshot-only` flag — runs gate 8 + all read-only preflight,
  exits without any /system writes. ZERO-risk dry-run.

## Stage A invocation pattern (2026-05-19 in-flight)

Agent 76 is running:

```bash
./scripts/v3/deploy-hbc-to-dayu200-system.sh \
    preflight \
    --snapshot-only
```

This exercises gates 8, 13, and 11's probe path WITHOUT any /system
writes. It's the recommended first-touch of any new board, and the
recommended re-touch after any out-of-band board state change.

## Allowlist discipline (gate 9)

The 17-entry LIVE_SERVICE_SO allowlist is empirical and board-version-
specific. Future agents extending the script: any new live `.so` must
be added to that allowlist explicitly, NOT inferred from a scan. The
allowlist is the audit point for "what we're allowed to live-swap";
implicit live-swap is brick-class.

## Don't conflate atomic-swap with `cp` (gate 9 anti-pattern)

`cp src dst` truncates `dst` mid-write. Any process with `dst` mmap'd
catches a SIGBUS on read. The correct primitive is:

```bash
cp src dst.new      # write to new path
mv dst.new dst      # atomic rename (POSIX guarantee on same fs)
```

The kernel's COW semantics mean already-mapped processes keep seeing
old inode; new exec'ers see new inode. This is the only safe live-swap
primitive on /system.

# Why this matters beyond V3 W2

Any future /system-touching deploy on OHOS will face the same brick
mechanisms. The gates aren't W2-specific — they're /system-specific.

V3 W4-engine production deploy, W6 composable peer-window deploy, W8
SceneBoard rework, and any V3 W11 OHOS adapter framework rollout that
touches `/system` should all adopt the same 6 gates. Treat the
hardened script as the canonical reference; copy the gate
implementations rather than re-deriving them.

# Co-occurring discipline

- [[feedback_soft_brick_w2_2026-05-16]] — origin of brick-recovery
  discipline. The W2 postmortem produced rules 1-3 (channel-health
  probe, no `|| true` on chcon, no silent-SKIP). Today's gates 8-13
  extend that family with /system-specific atomicity primitives.
- [[feedback_hdc_shell_check_pattern]] — gate 9's `mv` step needs
  `hdc_shell_check` to detect a silent-NOOP rename failure.
- [[project_v3_chroot_phase1a_validated]] — the chroot work that
  produced the 4 base fixes the gates layer on top of.
- [[project_v3_phase_1b_blocked_typeface_segv]] — the immediate
  motivator: chroot diagnostic ceiling drove path B; gate 13's
  processdump probe is the diagnostic value-add that justifies the
  brick risk.

# Where this lives

- This memory file: lead rule + gate inventory.
- `scripts/v3/deploy-hbc-to-dayu200-system.sh` at commit `73ae3ac1`:
  canonical implementation (1707 LOC; gates 8-13 + Fix A-D).
- Predecessor: `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` at commit
  `0c9e7532` (1190 LOC; gates 1-7 + 4 chroot fixes).
- `docs/engine/V3-DEPLOY-HARDENED-SOP.md` — operational SOP that
  invokes the script with the right stages in the right order.

# Cross-references

- commits: `0c9e7532` (chroot base), `3bfcfa5e` (initial /system port),
  `73ae3ac1` (gates 8-13 added)
- `scripts/v3/deploy-hbc-to-dayu200-system.sh` — canonical impl
- `docs/engine/V3-DEPLOY-HARDENED-SOP.md` — operational SOP
- `demo/run-live.sh` (Westlake-Island) — source of gate 10/11/12 patterns
- [[feedback_soft_brick_w2_2026-05-16]] — origin postmortem
- [[feedback_hdc_shell_check_pattern]] — gate 9 control-flow safety
- [[feedback_chroot_dynamic_elf_ro_bind]] — chroot fix that does NOT
  carry over (no bind-mounts on /system)
- [[project_v3_chroot_phase1a_validated]] — chroot milestone the gates
  build on
- [[project_v3_phase_1b_blocked_typeface_segv]] — pivot motivator
- [[handoff_2026-05-19]] — current orientation; afternoon update
  records the path-B pivot
