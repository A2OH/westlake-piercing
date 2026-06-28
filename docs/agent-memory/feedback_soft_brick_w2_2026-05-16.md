---
name: soft-brick-w2-2026-05-16-hdc-channel-health-probe-between-every-stage
description: "When a deploy script's transport channel can fail silently (exit 0 + empty stdout), probe channel health before and after every Stage. Do not run chcon/restorecon/critical commands if the prior stdout-probe didn't echo expected bytes. Especially when transport is a Windows binary from WSL with wslpath translation — two layers of error-swallowing."
metadata: 
  node_type: memory
  type: feedback
  date: 2026-05-16
  triggers: 
    - V3-W2
    - DAYU200
    - [build-host]-deploy
    - hdc-shell
    - soft-brick
  blast_radius: any-deploy-to-/system
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

# Rule

**Before any `/system` write on an OHOS board (or any deploy whose
transport channel can degrade silently), probe channel health BETWEEN
every Stage. If the shell stdout channel goes silent post-Stage, STOP
immediately and reboot — do not continue to chcon/restorecon/Stage-4.**

Corollary rules:

- **A borrowed SOP encodes the author's mid-bringup board state, not a
  clean board.** Add a fresh-bringup gate before adopting verbatim.
- **Run a new deploy in increments on the first attempt** (one component
  at a time + verify) until the bare-minimum recovery path is exercised
  end-to-end. Full-SOP-in-one-shot is unsafe until you've proven the
  rollback works.

# Why

**Concrete: 2026-05-16 V3-W2 attempt soft-bricked DAYU200.**

Agent 49 ran `scripts/v3/deploy-hbc-to-dayu200.sh` per `westlake-deploy-ohos/v3-hbc/scripts/DEPLOY_SOP.md`.
Stage 0/1/3 passed. Then `hdc shell` started returning exit 0 with empty
stdout, while `hdc file send/recv` continued to work. The deploy script's
chcon batches at lines 188-193 (3c-end) and 246-248 (3e-end) ended with
`|| true`, so silent-channel chcon failures didn't propagate to the
suite verdict. Then `hdc target boot` was issued; device never
re-enumerated over USB.

Post-mortem (`docs/engine/V3-W2-POSTMORTEM.md`) ranks top-2 causes:

- **H1:** chcon silently no-op'd → labels at default `system_file:s0` →
  appspawn:s0 domain dlopen flock-denied → ART JNI_CreateJavaVM SIGABRT
  respawn storm on next service tick (predicted by [build-host] SOP §3c line 104
  + §3e line 146).
- **H2:** `/mnt/c/Users/<user>/Dev/ohos-tools/hdc.exe` Windows binary
  invoked from WSL with `wslpath` conversion regressed on the shell
  stdout channel while preserving file send/recv.

Both compound: H2 fires first → chcon stops echoing back → labels
silently wrong → H1 fires on next service tick → USB drops.

The single highest-value intervention that would have caught this in
time is a stdout-channel-health probe between every Stage. The
`scripts/v3/run-hbc-regression.sh::w2_slot` already implements a
push-probe-recv fallback because the W2 agent had seen the quirk
intermittently before — but the deploy itself didn't use that pattern.

**Tangential evidence the channel goes silent in this exact way:**
`scripts/v3/run-hbc-regression.sh` lines 449-489 — the slot author
(same agent 49) wrote the workaround inline. The discipline gap was
that the discipline lived in the regression-time probe but not in the
deploy-time procedure.

**Why borrowed-SOP discipline matters too:** [build-host]'s `DEPLOY_SOP.md v4`
is dated 2026-04-21. The `boot-framework.art` size guard (SOP §0
line 19) is 23,760,896 bytes; W1 pulled 23,781,376 bytes (+20 KB drift,
W2 report line 13). Two SOP-mandated `.so` files (`libinstalls.z.so` +
`libappexecfwk_common.z.so`) were absent from W1's `v3-hbc/lib/` pull
entirely (W2 report lines 33-36) and the deploy script silently SKIPped
them. The SOP was correctly authored for [build-host]'s exact stack state — but
that stack state has already drifted between SOP-author date and W1
pull date. Westlake adopted the SOP without a fresh-bringup gate.

# How to apply

## Rule 1 — Channel-health probe (primary)

Add a `channel_alive` function that returns 0 only if `hdc shell` echoes
back an expected sentinel string. Call it between EVERY Stage:

```bash
channel_alive() {
    local sentinel="HDC_SHELL_PROBE_$(date +%s)"
    local got; got=$("$HDC" shell "echo $sentinel" 2>/dev/null | tr -d '\r\n')
    [ "$got" = "$sentinel" ]
}

# Use at every Stage boundary:
channel_alive || { echo "ABORT: hdc shell stdout dead — do not run chcon"; exit 2; }
```

If the probe fails, **the device is currently alive but the host can't
hear it**. Reboot the device via the OOB path (or stop the deploy and
escalate to operator) before issuing any more chcon/restorecon/rm-rf
commands, because those commands will silently no-op and leave the
device in a half-deployed state that init won't survive on next service
tick.

## Rule 2 — Never `|| true` a chcon/restorecon

Replace `chcon ... || true` patterns with assert-and-abort. If the chcon
fails silently, you lose the chain. Verify label stuck via
`ls -lZ <path> | grep <expected_label>` round-trip.

## Rule 3 — Never silent-SKIP a required artifact

In `push_file` (or equivalent): `[ -f "$src" ] || die "required artifact
$src missing"`. The W2 silent-SKIP semantics (lines 78-81 of the deploy
script) returned 0 on missing source. Two SOP-mandated `.so` files were
absent and the deploy continued. An allowlist of files that may
legitimately be absent should be explicit; everything else is fatal at
pre-flight, not silent during push.

## Rule 4 — Run a new deploy in increments on the first attempt

When adopting a borrowed SOP, the **first** end-to-end attempt should be
incremental: Stage 0 → verify → Stage 1 → verify → ... where "verify"
includes Stage 1 rollback rehearsal (push then restore from `.orig_${TS}`
backups while the channel is still healthy). Only after the bare-minimum
recovery path is exercised should the full SOP run in one shot. The W2
attempt was full-shot on day one; recovery is now blocked on operator
power-cycle.

## Rule 5 — A borrowed SOP needs a fresh-bringup gate

When adopting a sibling team's SOP, ask:

- What's the SOP's reference board state? ([build-host]'s was a multi-deploy
  board mid-bringup, not factory.)
- What artifacts has it implicitly drifted past since the author date?
  ([build-host]'s +20 KB `boot-framework.art` drift.)
- Are there SOP §"待沉淀" / TODO sections noting known gaps?
  ([build-host] SOP lines 237-246 listed 4 missing items the SOP knew about.)
- What's the host-side tooling version pin? (No `hdc version` check ran.)

Add the gate to V3-DEPLOY-SOP.md Stage 0 before any agent runs the
adopted SOP.

## Rule 6 — When a probe fails post-deploy, capture diagnostics via the
**alternative** channel before any rollback

If `hdc shell` is dead but `hdc file recv` works (the exact W2 scenario):

```bash
# Push a diagnostic script and pull its output via file recv.
# Pattern is in run-hbc-regression.sh::w2_slot lines 449-489.
"$HDC" file send diag.sh /data/local/tmp/diag.sh
"$HDC" shell "sh /data/local/tmp/diag.sh > /data/local/tmp/diag.out 2>&1" \
    >/dev/null 2>&1 || true     # shell may be silent; that's fine
"$HDC" file recv /data/local/tmp/diag.out /tmp/diag.out
# Now /tmp/diag.out has hilog tail + pidof + ls -lZ + restorecon dry-run.
```

This buys you a full diagnostic snapshot even when the primary channel
is gone — critical for distinguishing post-mortem hypotheses.

# Where this lives

- This memory file: lead rule + how-to.
- `docs/engine/V3-W2-POSTMORTEM.md` §4 G1-G7: concrete SOP/script
  amendments encoding rules 1-3 + 5.
- `docs/engine/V3-W2-POSTMORTEM.md` §5: formalized recovery procedure
  that encodes rule 6.
- `scripts/v3/run-hbc-regression.sh` (next agent's W2-followup):
  G7 adds `check_hdc_shell_stdout_alive` + `check_deploy_chcon_assertions`
  + `check_required_lib_inventory` slots so the discipline is mechanized
  at regression time.

# 2026-05-19 follow-up — silent-NOOP class STRUCTURALLY BOUNDED

The silent-NOOP failure class that caused the W2 soft-brick is now
bounded by **four** rules — the original three here plus the structural
follow-up landed during V3 W2 Phase 1a chroot bring-up
([[project_v3_chroot_phase1a_validated]]):

- **Rule 1** (THIS doc) — channel-health probe BETWEEN every Stage
- **Rule 2** (THIS doc) — never `|| true` a chcon/restorecon
- **Rule 3** (THIS doc) — never silent-SKIP a required artifact
- **Rule 7** ([[feedback_hdc_shell_check_pattern]]) — never trust
  `hdc.exe` host exit code in control-flow; use `hdc_shell_check` helper
  that propagates the device-side exit code via sentinel wrapper

Rule 7 closes the structural hole: even if rules 1-3 are followed by an
agent, the **host-side exit-code-0 laundering** that `hdc.exe` performs
would still allow `if hdc_shell "<check>"; then <destructive>; fi`
patterns to silently fire the destructive branch. Rule 7 makes that
class of control-flow safe by construction.

Additionally:

- **H2 hypothesis DIES.** The 2026-05-19 hdc 3.2.0b empirical probe
  (commit `f158cf58`, doc `docs/engine/V3-HDC-3.2.0B-PROBE-REPORT.md`)
  ran 200 iterations with 0 silent returns. The Windows `hdc.exe`
  stdout-channel regression hypothesized in §3 of this postmortem is
  not reproducible on the current bundle. Don't pin
  `KNOWN_GOOD_HDC_VERSIONS=1.3.0c-e`; don't chase 1.3.0c binaries from
  [build-host].
- **H1 hypothesis still PLAUSIBLE but not refalsified.** The 2026-05-19
  chroot Phase 1a deploy avoided `/system` writes entirely (chroot
  containment per `docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md`), so
  the SELinux-respawn-storm hypothesis could not fire by construction.
  If Phase 2 (/system writes) is ever reactivated, H1 mitigations
  remain mandatory.

# Cross-references

- `docs/engine/V3-W2-POSTMORTEM.md` — full postmortem this rule derives
  from
- `docs/engine/V3-W2-BOOT-[build-host]-RUNTIME-REPORT.md` — W2 agent's
  checkpoint (the source of truth)
- `docs/engine/V3-DEPLOY-SOP.md` — Westlake-adapted SOP (gets G1-G6
  amendments)
- `scripts/v3/deploy-hbc-to-dayu200.sh` — the original /system deploy
  driver that needs rules 1-3 baked in (now superseded by chroot
  variant for Phase 1)
- `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` — 2026-05-19 brick-safe
  chroot deploy; rules 1-3 + 7 all baked in
- `westlake-deploy-ohos/v3-hbc/scripts/DEPLOY_SOP.md` — [build-host]'s SOP v4
  (the borrowed SOP, do not modify in place)
- `docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md` — 2026-05-19 Phase 1
  deploy model (brick-impossible alternative to /system writes)
- `docs/engine/V3-HDC-3.2.0B-PROBE-REPORT.md` — 2026-05-19 empirical
  H2 death certificate
- [[feedback_hdc_shell_check_pattern]] — 2026-05-19 structural follow-up
  (rule 7); host-exit-code laundering bounded
- [[feedback_chroot_dynamic_elf_ro_bind]] — 2026-05-19 companion rule;
  Stage-3 RO bind-mount of host lib paths
- [[project_v3_chroot_phase1a_validated]] — 2026-05-19 milestone marker
- [[feedback_subtraction_not_addition]] — co-occurring rule (rule 4
  here is a corollary: subtraction discipline extends to deploy
  adoption, not just to debugging)
- [[feedback_additive_shim_vs_architectural_pivot]] — anti-pattern
  guard: W2 failure does NOT warrant a V3 pivot; it's normal W-level
  engineering rework
- [[feedback_two_pivots_in_two_days]] — same theme; the pivot bar is
  high
- [[project_v3_hbc_reuse_direction]] — V3 direction memo
- [[handoff_2026-05-19]] — current V3 orientation (supersedes
  handoff_2026-05-16)
- [[handoff_2026-05-16]] — V3 orientation when this postmortem landed
  (superseded)
