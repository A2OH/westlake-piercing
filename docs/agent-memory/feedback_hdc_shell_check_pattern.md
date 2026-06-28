---
name: hdc-shell-check-pattern-host-exit-code-laundering
description: "Any hdc.exe shell call used in boolean control-flow (if/while/&&/||) must propagate device-side exit code via sentinel wrapper. hdc.exe returns host exit 0 regardless of remote command outcome; tr launders any inline signal. Same anti-pattern family as 2026-05-16 soft-brick. Use hdc_shell_check helper."
metadata:
  node_type: memory
  type: feedback
  date: 2026-05-19
  triggers:
    - hdc-shell
    - DAYU200
    - deploy-script
    - control-flow
    - silent-NOOP
  blast_radius: any-deploy-or-validation-script-using-hdc-shell
  originSessionId: agent-70-2026-05-19
---

# Rule

**Any `hdc.exe shell` invocation used in boolean control-flow context
(if / while / `&&` / `||`) MUST propagate the device-side exit code via
sentinel wrapper. Never rely on `hdc.exe`'s host-side exit code in
control-flow — it is permanently 0 regardless of what happened on the
device.**

# Why

**Concrete: 2026-05-19 chroot deploy attempt nearly re-enacted the
2026-05-16 soft-brick.**

`hdc.exe` (and `hdc` Linux native) returns host process exit 0 whenever
the host-side transport succeeded — regardless of whether the remote
command on the device exited 0, 1, or signalled. The standard idiom
`hdc shell "<cmd>" | tr -d '\r'` then launders any inline signal the
remote command might have emitted on stdout.

Net effect: `if hdc_shell "mountpoint -q /data/local/tmp/v3-hbc-chroot/proc";
then echo "already mounted"; fi` is **permanently TRUE** because the host
hdc exited 0. The script then SKIPS the mount step, leaving the chroot
in a half-set-up state that may or may not be brick-safe.

This is the same anti-pattern family as the 2026-05-16 soft-brick
([[feedback_soft_brick_w2_2026-05-16]]) — silent-NOOP via error-swallowing
in the host transport layer. The chroot deploy script
(`scripts/v3/deploy-hbc-to-dayu200-chroot.sh`, commit `f10ee81b`) caught it
during Phase 1a bring-up because the chroot bind-mount was idempotent and
the smoke test would have failed loudly on a real engine attempt.

But on a non-idempotent step (chcon, rm, mv, ln -s, mount of a non-empty
target) the same pattern silently corrupts state. The 2026-05-16 chcon
silent-NOOP via `|| true` was the worst-case manifestation; this pattern
is the **structural** generalization.

# How to apply

## Helper: `hdc_shell_check` (canonical implementation)

See `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` lines 198-213:

```bash
# Wrap remote command with sentinel; parse on host.
# Returns the DEVICE-side exit code, not the host hdc.exe exit code.
hdc_shell_check() {
    local cmd="$*"
    local out
    out=$("$HDC" shell "$cmd; echo __EXIT__=\$?" 2>/dev/null \
          | tr -d '\r')
    local device_exit
    device_exit=$(echo "$out" | sed -n 's/.*__EXIT__=\([0-9]*\)$/\1/p' \
                  | tail -1)
    # Strip sentinel from any captured stdout if caller wants it.
    [ -n "$device_exit" ] && return "$device_exit"
    return 127  # sentinel never echoed — channel-degraded; fail loud
}
```

If the sentinel never appears in stdout, that itself is a channel-health
failure (same class as the 2026-05-16 silent-stdout regression). Return
127 (= treat as "command not found" / hard failure) — never silently
succeed.

## Where to use it

- Every `if hdc_shell "<cmd>"`, `while hdc_shell "<cmd>"`,
  `hdc_shell "<cmd>" && ...`, `hdc_shell "<cmd>" || ...` site.
- Any mount / chmod / chcon / rm / mv / ln -s / cp / mkdir verification
  that runs over hdc shell.
- Any "is process running" / "does file exist" / "is mountpoint mounted"
  probe whose answer determines a destructive next step.

## Where it's NOT needed

- Output-parsing sites: `out=$(hdc_shell "<cmd>"); [ "$out" = "expected" ]`.
  The control-flow predicate is on the parsed output, not on the hdc
  exit code. These already use sentinel-echo patterns implicitly (their
  whole purpose is to capture stdout). About 30+ sites in the chroot
  deploy script are this pattern and stayed unchanged.

## Audit procedure for a new deploy / validation script

```bash
# Find risky control-flow patterns.
grep -nE '(^|[^a-zA-Z_])(if|while)\s+hdc_shell|hdc_shell\s+[^|]*&&|hdc_shell\s+[^|]*\|\|' \
    scripts/v3/*.sh
```

Every hit needs `hdc_shell` → `hdc_shell_check` conversion, OR a
documented justification why host-exit-0 is the correct semantic at
that site (rare — typically only true for fire-and-forget like
`hdc_shell "reboot" || true` where you don't care about exit code).

# Co-occurring discipline

- [[feedback_soft_brick_w2_2026-05-16]] — origin postmortem. The class
  of silent-NOOP-via-error-swallowing is now bounded by:
  - Rule 1 (channel-health probe BETWEEN every Stage)
  - Rule 2 (never `|| true` a chcon/restorecon)
  - Rule 3 (never silent-SKIP a required artifact)
  - **THIS RULE (never trust host exit code from hdc.exe in control-flow)**
  Together these 4 rules cover the soft-brick failure mode.

- [[feedback_chroot_dynamic_elf_ro_bind]] — co-discovered same day; the
  chroot Stage 3 RO bind-mount is the brick-safe path that exercised
  these rules end-to-end.

- [[feedback_subtraction_not_addition]] — the diagnostic discipline that
  surfaced this anti-pattern (one removal at a time from a working
  baseline, not speculative addition).

# Where this lives

- This memory file: lead rule + canonical helper.
- `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` lines 198-213: helper
  implementation; commit `f10ee81b` converted 3 control-flow sites.
- Future agents adding new deploy / validation scripts must audit via
  the grep above before merging.

# Cross-references

- `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` — canonical helper site
- commit `f10ee81b` — the conversion + 3 sites fixed
- `docs/engine/V3-W2-POSTMORTEM.md` — original silent-NOOP postmortem
- [[feedback_soft_brick_w2_2026-05-16]] — origin
- [[feedback_chroot_dynamic_elf_ro_bind]] — companion 2026-05-19 rule
- [[handoff_2026-05-19]] — current orientation
- [[project_v3_chroot_phase1a_validated]] — milestone where this rule
  was forged
