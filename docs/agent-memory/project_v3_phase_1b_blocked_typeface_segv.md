---
name: v3-phase-1b-blocked-typeface-segv-2026-05-19
description: "2026-05-19 finding: V3 Phase 1b (Java main in chroot) BLOCKED. AOSP Typeface.<clinit> setSystemFontMap SEGVs in libhwui native font loader during preload chain. Reproduced both with chroot's own /system/fonts AND with host /system/fonts bind-mounted. processdump unavailable in chroot — no backtrace. This is the substrate-fidelity gap that drives the chroot ceiling and motivates the /system pivot to path B."
metadata:
  node_type: memory
  type: project
  date: 2026-05-19
  triggers:
    - V3
    - Phase-1b
    - chroot
    - Typeface
    - libhwui
    - DAYU200
    - SEGV
  originSessionId: agent-77-2026-05-19
---

# Empirical finding

**2026-05-19:** V3 W2 Phase 1b.2 (Java main inside chroot after spawn
round-trip) is **BLOCKED** on a Typeface preload SEGV that the chroot
cannot diagnose. Agent 74 reached the failure at commit `ad143662`;
prior work at commits `ee449def` (Phase 1a-plus PASS), `36717af8`
(Phase 1b.1 spawn round-trip PASS), and `1f789703` (3 side-fixes
enabling full liboh_android_runtime.so load) all landed clean.

## Failure point

```
AppSpawnXInit.initChild
  → liboh_android_runtime.so loaded (after 3 side-fixes in 1f789703)
  → JNI registration succeeds
  → AOSP Typeface.<clinit>
    → setSystemFontMap
      → preload /system/fonts/Roboto-Regular.ttf
        → libhwui native font loader
          → SIGSEGV  (no backtrace; processdump unavailable in chroot)
```

## What was tried

1. Use the chroot's own `/system/fonts/` ([build-host]-bundle Roboto-Regular.ttf):
   SEGV.
2. Bind-mount host `/system/fonts/` into chroot
   (RO bind, same idiom as [[feedback_chroot_dynamic_elf_ro_bind]]):
   **still SEGV**. So the bug is not a missing-font / bad-bytes issue
   at the file-content layer; the crash is **inside** libft2 /
   libminikin / libhwui's native parser-loader chain.

## Why we couldn't go further inside the chroot

- `processdump` is part of the OH faultlog stack and not available
  inside the chroot Phase 1a artifact set.
- The SEGV terminates the child process before any application-level
  diagnostic can run.
- Without a backtrace, root-causing the libhwui crash is guess-work.

# Why it matters

- Phase 1a substrate is validated; Phase 1b engine question still open.
- This is a **substrate-fidelity gap**: something about the chroot's
  font-loader environment differs enough from a real OH process that
  preload SEGVs. Candidates worth probing (none proven):
  - Missing libharfbuzz transitive dependency (dlopen'd at runtime
    inside libhwui, not picked up by ldd → see also
    [[feedback_chroot_dynamic_elf_ro_bind]] point on dlopen blind spot).
  - Stale AOT-baked `sFontMap` in the shipped framework that points
    at paths the chroot doesn't expose at the same inode.
  - Missing `/system/etc/fonts/` config entry that the loader expects.
  - Wrong SELinux label on chroot-bind-mounted font path triggering
    a deny that the loader doesn't handle gracefully.
- **The chroot's brick-safety made debugging this particular bug
  class hard.** That's not a chroot-design flaw — it's the inherent
  tradeoff: brick-impossible deploys can't run brick-only diagnostic
  tools.

# Pivot decision (user, 2026-05-19)

Per the user pivot decision recorded in [[handoff_2026-05-19]]
afternoon update: accept chroot Phase 1a-plus as the **maximum useful
chroot validation** and pivot to a hardened **/system deploy** for the
engine question. Two grounds:

1. **Diagnostics.** /system deploy has `processdump` available — same
   crash with a real backtrace turns this from a guess into a fix.
2. **Product goal.** Composable peer-window product target requires
   /system eventually anyway. Phase 1b chroot was a brick-safe
   stepping-stone, not the destination.

Brick risk is mitigated by porting all 4 chroot safety fixes back into
the hardened /system script — see
[[feedback_hardened_script_gate_8_to_13]] for the 6 additional /system-
specific atomicity gates added on top.

# How to apply

- **Don't try to fix Typeface SEGV in chroot first.** The diagnostic
  cost is too high. Expect to encounter the same SEGV on /system, but
  with `processdump` available the backtrace is one re-run away.
- **First action under /system:** trigger the SEGV deliberately, grab
  the processdump backtrace, then triage by symbol depth:
  - Crash inside libft2 → font-file or parser bug; check format
    fidelity of Roboto-Regular.ttf bytes byte-for-byte vs known-good.
  - Crash inside libminikin → font config / sFontMap mismatch.
  - Crash inside libhwui glue → likely a missing dlopen'd transitive
    dep (libharfbuzz / libicuuc / similar).
- **Don't broaden the bind-mount surface** to try to "fix" this in
  chroot. Bind-mounting host `/system` would defeat the brick-safety
  guarantee that justifies chroot in the first place.

# What chroot extracted (still valuable)

Chroot Phase 1a-plus is NOT wasted work. It produced:

- 4 deploy-safety fix classes (hdc_shell_check, RO bind-mount,
  dual-path manifests, host-side awk script generation) — all now
  ported into the hardened /system script.
- Confidence that the substrate layer (mount table, ELF resolution,
  appspawn-x socket, ART load, framework BCP load) is correct: the
  SEGV is happening LATER in the boot chain, not earlier.
- 200-iteration hdc 3.2.0b probe artifact (H2 from W2 postmortem
  DIES).
- Empirical reuse-direction validation for [[project_v3_hbc_reuse_direction]].

# Cross-references

- [[handoff_2026-05-19]] — afternoon update section captures the pivot
- [[project_v3_chroot_phase1a_validated]] — substrate milestone this
  blocked Phase extends from
- [[feedback_hardened_script_gate_8_to_13]] — the /system safety
  gates that make path-B brick-recoverable
- [[feedback_ohos_only_no_android_phone]] — the directional constraint
  that frames why we're on OHOS at all
- [[feedback_soft_brick_w2_2026-05-16]] — origin of brick-recoverable
  discipline; chroot was its safety response, /system needs the
  atomicity gates instead
- `docs/engine/V3-W2-PHASE-1B2-RETRY.md` — agent 74 retry artifact
- commits: `ee449def` (1a-plus PASS), `36717af8` (1b.1 PASS), `1f789703`
  (3 side-fixes), `ad143662` (1b.2 blocked at Typeface SEGV)
