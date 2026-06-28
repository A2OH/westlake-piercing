---
name: prioritize-risky-productive-over-brick-avoidance-from-2026-05-20
description: "Brick recovery via reflash is operator-routine, not crisis. Don't dispatch probe-after-probe or halt-at-every-gate; prioritize productive progress. Risky/productive > safe/stalled."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**Reflashing the board is not hard. Don't waste agent time and operator gates on brick-avoidance strategies that delay productive progress.** Take the risky path when it advances the actual product goal (McD/noice running on OHOS as composable apps).

Specifically forbidden:
- Multi-probe regimes before every board touch (3-4 probes before Stage B was excess)
- Operator-gate-between-every-stage pattern (Stage A → ask → Stage B → ask → Stage C → ask)
- Treating "brick-recoverable in ~1hr ROM flash" as a high cost — it's the normal recovery primitive
- Adding more safety theater (snapshot/restore/recovery subcommands) when the simpler path is "let it brick, reflash, retry with what we learned"

Specifically encouraged:
- Dispatch larger work units (Stage B + Stage C + McD launch in one sweep)
- Accept brick as the recovery boundary, not a failure
- When you learn something from a brick, write it down and move on — don't relitigate
- Productivity over paranoia

## Why

User directive 2026-05-20 after a real brick (Stage B retry → Channel A death → soft-reboot recovery attempt → partial-state /system → won't boot → operator did ROM flash, ~30-60 min).

The brick happened because I'd been chasing brick-avoidance for 4 iterations and STILL hit a brick. Brick avoidance bought zero net safety vs the productive path. The probe stack (READ / WRITE / concurrency / chcon-on-ENOENT) all came back CLEAN and I missed the actual trigger (chmod+symlink+restorecon on /system). Multiple operator gates between stages slowed progress but didn't prevent the failure.

User's stated preference: "reflash board is not hard so don't waste time in non-brick strategy instead moving on with risky but productive path."

The product goal is composable McD/noice on OHOS. Every probe + gate + safety primitive is overhead. Reflash is the recovery mechanism. Don't fight it; accept it.

## How to apply

**When dispatching agents for board work:**
- Default to "all stages in one sweep" rather than per-stage gates
- Brief the agent to push through, capture forensic on any abort, and accept brick as outcome if needed
- Don't add new safety primitives beyond what's structurally required

**When learning happens (probes, bricks, debug):**
- Capture the lesson in memory
- Move on — don't iterate on additional probes to confirm what's already evident from the failure mode

**When planning next move:**
- Ask "what's the most productive single thing I can dispatch right now?" not "what's the safest sequence of small steps?"
- If a path requires 5 sequential gates, collapse into 1 agent that does all 5 with halt-on-failure-and-capture-forensic
- Trust the operator to reflash if needed; don't shield them from that cost

**Specific to Westlake V3:**
- Hardened script's brick-avoidance gates (8-13) are fine to keep but not the point — productive forward motion is
- chroot path was over-invested in (4 iterations of plumbing fixes) when /system path would have hit the actual engine question faster
- McD launch via aa start (the actual goal) should be the next major dispatch, accepting brick risk if it gets us there

## Exceptions (still worth gating)

- Operations with cross-team blast radius (shared infrastructure, pushing to others' branches)
- Irreversible operations not just hardware (e.g., deleting commits we don't have backups of, force-push to main)
- When user explicitly asks for caution on a specific operation

The exception bar is "is it irreversible at the people-level, not just the hardware level?" Hardware can be reflashed; trust + reputation can't.

## Cross-references

- [[feedback_ohos_only_no_android_phone]] — OHOS-only is the target; that filter still applies, just don't gate within OHOS work
- [[feedback_two_pivots_in_two_days]] — pivot discipline still holds; this rule is about VELOCITY within a chosen direction, not about pivots
- [[feedback_soft_brick_w2_2026-05-16]] — H1/H2 brick mechanisms documented; treat as operational signal, not as something requiring more probes to settle
- [[project_v3_chroot_phase1a_validated]] — substrate validation; chroot was the over-invested path
- [[project_v3_phase_1b_blocked_typeface_segv]] — chroot ceiling; the brick-recoverable /system path is the productive alternative
- [[feedback_hardened_script_gate_8_to_13]] — gates exist, but they're not the point; pushing through Stage 4+ is
