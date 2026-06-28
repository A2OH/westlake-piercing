---
name: two-pivots-in-two-days-v1-v2-2026-05-15-and-v2-v3-2026-05-16-discipline-note
description: "Reflective lesson — when consecutive strategic pivots happen on consecutive days, the next pivot must clear a higher evidence bar. Both pivots were well-justified; a third needs harder evidence still."
metadata: 
  node_type: memory
  type: feedback
  date: 2026-05-16
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

# Two pivots in two days — what's OK, what isn't

## Timeline

- **2026-05-15 morning** — V1 (additive-shim chain CR-W → CR-Y → CR-Z on
  SoftwareCanvas) pivots to **V2** (full V2-substrate port to OHOS:
  M10 own libbinder + M11 audio daemon + M12 surface daemon). Documented
  in `project_v2_ohos_direction.md`. Driver: `feedback_additive_shim_vs_
  architectural_pivot.md` rule firing — CR-W → CR-X → CR-Y → CR-Z chain
  (~24 commits, ~½ day) on SoftwareCanvas had visible-result not advancing
  while symptoms kept rotating. The layer (SoftwareCanvas / drm_inproc_
  bridge) was wrong.

- **2026-05-16 today** — V2-OHOS pivots to **V3** ([build-host]-runtime reuse).
  Documented in `docs/engine/V3-ARCHITECTURE.md` + `project_v3_hbc_reuse_
  direction.md`. Driver: CR-EE / CR-FF audits of [build-host]'s adjacent work tree
  (`~/adapter/`) revealed [build-host] has independently solved the OHOS Android-
  app hosting problem at multi-engineer-month cost with real AOSP-14 ART
  + real `framework.jar` cross-built to OHOS musl, reaching `MainActivity.
  onCreate` line 83 — significantly past Westlake's V2-OHOS milestone.
  Same company, same product effort. Reusing the work is the rational move.

That's two strategic pivots in two days. Both were correct calls under the
discipline of `feedback_additive_shim_vs_architectural_pivot.md`. But the
pattern itself is a signal worth recording so future decisions don't
trivially flip a third time.

## Why both pivots were OK (the bar each cleared)

### V1 → V2 (2026-05-15)

**Evidence bar cleared:**
1. ≥3 consecutive CRs in the same architectural layer (CR-W, CR-X, CR-Y,
   CR-Z) — the rule's own threshold.
2. End-to-end visible result didn't advance (panel pixel = launcher
   fallback white per CR67 / CR-AA diagnostics) despite each CR landing
   cleanly with its own per-CR PASS.
3. Each CR generated new symptoms in the same layer (NPEs migrating
   through arsc parsing → theme resolution → lifecycle drive → Date /
   ByteOrder `<clinit>` → setContentView NPE chain).
4. The replacement direction (V2 substrate as platform-level pivot) had
   a precedent — Phase-1 Android-phone V2 in-process Option 3 was already
   working on cfb7c9e3, so the substrate concept was empirically validated.

### V2 → V3 (2026-05-16)

**Evidence bar cleared (harder than V1→V2):**
1. Independent empirical demonstration — [build-host]'s standalone work tree
   reaches `MainActivity.onCreate` line 83 on the same DAYU200 board class
   we target. CR-EE catalogued 38 cross-built native .so, 11 framework
   jars, 9-segment dex2oat boot image, 5 forward + 6 reverse bridges
   (530+ AIDL methods), `liboh_skia_rtti_shim.so` 12.6 KB instead of
   23 MB Skia rebuild. The replacement isn't speculation — it's an
   audited, working artifact.
2. The V2-OHOS premise was falsified: V2-OHOS premise was "Westlake owns
   the substrate, the daemon-based render path is the right glue." [build-host]'s
   evidence shows the substrate can run on OHOS with real HWUI / real
   Skia / real framework.jar through proper Binder-service-boundary
   transcoding (4 surgical L5 patches via `OHEnvironment.getXxxAdapter()`).
   The 4-CR sample chain on V2-OHOS (CR62, CR63b, CR64, CR65...) was
   re-running the V1 pattern at one layer up.
3. CR-DD analysis explicitly considered the option and recommended
   HYBRID — V3 effectively chooses option (d) "reuse [build-host]" which CR-DD /
   CR-BB didn't enumerate because [build-host]'s existence wasn't known to those
   CRs when authored.
4. No IP / org concern with reusing [build-host]'s work — same company, same
   product effort.
5. Westlake's *learnings* from V2 carry forward unchanged: 5-pillar
   pattern (still applies on phone), CR59 lifecycle drive lesson (applied
   at [build-host]'s `AbilitySchedulerBridge` Handler.post seam under V3), macro-
   shim contract (narrower scope under V3), CR60 bitness discipline (V3
   stack is 32-bit ARM on DAYU200), no-per-app-branches rule. Only the
   *artifacts* are archived. (See `V3-V2-CARRYFORWARD-AUDIT.md` from W11.)

## What's NOT OK going forward (anti-pattern)

A **third pivot in three days** (or even in the next two weeks) without
clearing an even higher evidence bar is the failure mode this note exists
to prevent.

Specifically, **DO NOT pivot V3 to something else** based on:

1. **A first-CR setback in V3 work** — W1 might surface a missing
   artifact, W2 might surface a boot regression, W4 might surface a
   Westlake-scope diff [build-host] didn't anticipate. None of these warrant a
   layer pivot. They warrant a CR within V3 to address the specific gap.

2. **A second teammate's adjacent work** that looks attractive at a
   glance. CR-EE / CR-FF took ~½-1 day of careful audit each before
   producing the recommendation. A third pivot must come from comparable
   depth of audit, not from a one-paragraph proposal.

3. **A general feeling that "V3 is also hard."** V3 is hard. So is every
   non-trivial OS port. Hardness alone is not pivot-evidence.

4. **A failed acceptance gate on a single W.** Each W has acceptance
   criteria; one W failing means the W needs rework, not the architecture
   layer. The `feedback_additive_shim_vs_architectural_pivot.md` threshold
   is **≥3 consecutive CRs in the same layer with visible result not
   advancing AND symptoms rotating** — applied to V3 that means at least
   3 W-level retries in the same architectural seam (e.g., 3 attempts at
   W4 adapter customization, all failing in different ways).

## The "three consecutive pivots" red line

If a third architectural pivot is ever proposed within a 2-week window
of V3 landing:

1. **STOP.** Do not draft architecture docs.
2. **AUDIT** the pivot pattern itself: are we pattern-matching the
   "additive-shim" rule loosely to justify normal engineering setbacks
   as "layer is wrong"?
3. **REQUIRE** the third-pivot proposal to include:
   - The specific ≥3 CRs / Ws in the same V3 layer with the same
     symptom-rotation pattern (cite commit hashes + per-CR reports)
   - Independent empirical evidence (not a paper proposal) that the
     replacement layer works for our acceptance criteria
   - A pre-mortem of what would need to be true for the third pivot to
     also need a fourth pivot in week 4
4. **PEER REVIEW** by at least 2 agents (codex 2nd opinion + 1 swarm
   peer) before committing.

The reason this matters: each pivot loses on the order of 1-2 weeks of
swarm momentum. Two pivots in two days is fine when both are well-
justified. Three pivots in two weeks would be a project failure mode.

## Concrete commitments

1. **W1-W7 close on V3** before considering any architectural alternative.
   The first hard signal would be W6 / W7 failing acceptance after W4
   adapter customization has had 3+ attempts. That's at least 3-4 weeks
   from today.

2. **W11 carryforward audit** (in flight 2026-05-16) is the canonical
   record of what we keep and what we archive. If V3 ever pivots, the
   keep-list shrinks; the archived V2-OHOS material is forever archived,
   not reactivated.

3. **CR-DD HYBRID proposal** is documented as superseded — V3 effectively
   chooses option (d) "reuse [build-host]." Future readers should not interpret
   CR-DD as a live alternative.

4. **[build-host] artifact pull (W1)** freezes a version. Periodic re-pull cadence
   defined in W9's RCA-discipline doc. If [build-host] pivots away from this stack
   themselves, we inherit an orphan-fork problem; the cadence + re-audit
   discipline is the mitigation.

## What stays the same across V1, V2, V3 (the conceptual through-line)

- **Macro-shim contract** (narrower scope each generation but same rule)
- **No per-app branches** (same grep, same rule)
- **Subtraction not addition** when debugging
- **Fail-loud not silent stub**
- **CR60 bitness discipline** (32-bit ARM on DAYU200)
- **APK transparency** (V2 didn't enforce; V3 does at W6/W7 acceptance)
- **5-pillar pattern conceptual lessons** (5-pillar pattern itself is
  PHONE-ONLY but the lessons apply at integration seams everywhere)

The pivots are about which layer Westlake owns. The discipline rules are
about how Westlake operates within whichever layer it owns at the time.

## Cross-references

- `feedback_additive_shim_vs_architectural_pivot.md` — the rule that
  fired twice in two days
- `feedback_subtraction_not_addition.md` — co-occurring rule
- `feedback_macro_shim_contract.md` — narrower scope each generation
- `feedback_no_per_app_hacks.md` — invariant across pivots
- `project_v2_ohos_direction.md` — V1→V2 pivot record (SUPERSEDED)
- `project_v3_hbc_reuse_direction.md` — V2→V3 pivot record (CURRENT)
- `docs/engine/V3-SUPERVISION-PLAN.md` §7 risk 7 — same theme from
  supervision angle
- `docs/engine/CR-DD-CANDIDATE-C-VS-V2-OHOS-RECONSIDERED.md` — the
  reconsidered analysis that surfaced [build-host] as option (d)
