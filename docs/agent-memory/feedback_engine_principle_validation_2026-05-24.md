---
name: feedback-engine-principle-validation-2026-05-24
description: "2026-05-24 reset proved the engine-not-per-APK principle works at scale. After QW chain hit additive-shim wall + QW6 regressed QW4, Diag-F audit + Scope A + Scope B + Fix A.ii landed 6-apps-benefit-from-one-upstream-fix. Concrete validation rules for future work."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**When a wall family fires 3+ times in the same architectural layer, STOP adding shims. Run an audit instead.** The architectural pivot is cheaper than the Nth symptom patch.

**Why:** Today's session proved this empirically:
- Before: QW1 → QW2 → QW3.b → QW4 → QW5.a → QW6 (regressed QW4). 6 substrate fixes in same family. McD wall just moved layers deeper after each.
- Operator pushed back ("are you still following the principle?")
- Single audit (Diag-F, ~25 min) identified single upstream root cause (3-CL geometry from [build-host]'s adapter classloader being separate from BCP)
- Scope A (~1d) validated H4 hypothesis
- Scope B Phase 2 (~½d) deployed durable fix (BCP consolidation)
- Fix A.ii (~1d) traced one more upstream defect (PackageManagerAdapter empty ApplicationInfo) and fixed it at root
- **Result**: 6 apps unblocked from one upstream fix; McD progressed past `<init>` for first time today; HW regression-clean

## How to apply

For each new wall:
1. **First**, check if this wall is in the SAME architectural layer as the last N walls you've patched. If yes → STOP, run audit.
2. **Audit pattern** (~½d, read-only): trace upstream from the symptom site to find the actual contract violation. Don't just patch the @NonNull check that fired — find what made the field null.
3. **One upstream fix > N defensive patches.** Look for the single Java/C++ method that returns null/wrong-shape, not the dozen sites that consume it.
4. **Architectural pivot is bookkeeping cost, not engineering cost.** Reverting band-aids after upstream fix lands is cheap. Just do it.

## Concrete diagnostic methodology that worked (from Fix A.ii)

When you see a SEGV/NPE in AOT-compiled framework code:
1. Pull boot.oat from device
2. `oatdump --oat-file=<oat> --addr2instr=<file_offset - 0x1000>` (note offset correction for OAT v230)
3. Identify the Java method (class + signature + dex_pc)
4. Look at AOSP source for that method — find what virtual method it calls
5. **Bytecode usually doesn't lie** — the @NonNull violation site is the symptom; the bug is one frame up, in whatever code populated the null
6. Add `[FIX-XXX] J_<step>_TOP` diagnostic ping at the suspected populator
7. Check hilog for the value at that point. If empty/null → traced.

This methodology took Fix A.ii agent ~2-3h to apply. Same toolkit will work for Fix A.iii (the second `boot-framework.oat+0xa3e636` NPE that still blocks Amazon/Maps/Zoom).

## What NOT to do (anti-patterns proven today)

1. **Don't add per-APK smali patches** — even if they "work" temporarily. They violate the engine-not-patcher principle (G5) and accumulate as tech debt that breaks any new app.
2. **Don't add libart band-aids for symptom that recur** — QW2/QW4 worked individually but QW6 regressed QW4 because they were all treating the same cross-CL ghost. The architectural fix (Scope B BCP consolidation) eliminated the ghost.
3. **Don't trust the symptom site naively** — Fix A's first triage assumed the `ConfigurationController` NPE was about missing classes in the slim runtime jar (option a in the brief). Fix A.ii proved it was actually about `PackageManagerAdapter` returning empty `ApplicationInfo`. One frame up = real bug.
4. **Don't dispatch defensive null-checks when upstream contract is the bug** — agent 153's first instinct was a `if (res == null) return;` patch. That's a band-aid. The real fix is to ensure `getResources()` returns valid Resources.

## Validation criteria for "architectural fix" vs "band-aid"

A fix is architectural if all of:
- Lives in substrate (BCP / libart / adapter)
- Generalizes (multiple apps benefit, not just one)
- Doesn't require per-APK code or per-APK config
- Restores an AOSP contract (returns valid object instead of null) OR adapts an OHOS contract to AOSP (parcel rewrite, sample of) OR moves classes to where they belong in CL hierarchy
- Doesn't accumulate defensively (every fix should make the next debug easier, not harder)

A fix is a band-aid if any of:
- Touches APK contents (smali patch, manifest rewrite, dex repack)
- Defensive null-check at a site that "shouldn't be null per contract"
- Adds a workaround for behavior that real Android exhibits the same way
- Requires "and also for app X..." follow-ups

Use this checklist before dispatching.

## Reference incidents

- **2026-05-23 QW chain (band-aids)**: 6 substrate fixes (QW1-QW6) over 1 day. QW6 regressed QW4. McD wall shifted but didn't break. End-of-day state: 12/12 kotlinx preload OK with QW6, but back to 10/12 when QW6 reverted. Net forward progress: marginal.
- **2026-05-24 Scope B + Fix A.ii (architectural)**: 2 fixes in 1 day. Scope B closed 3-CL geometry permanently. Fix A.ii unblocked 6 apps. McD reached its own `<init>` user code body for first time. Net forward progress: substantial + durable.

## See also

- [[v3-scope-b-success-2026-05-24]] — the validation case
- [[feedback_additive_shim_vs_architectural_pivot]] — the original anti-pattern that 2026-05-24 confirmed
- [[feedback_no_per_app_hacks]] — closely related rule
- [[v3-mcd-chain-2026-05-23]] — the chain that demonstrated the wrong direction first
