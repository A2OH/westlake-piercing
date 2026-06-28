# `<clinit>` allocation in BCP class is NOT root cause of B-fix bootstrap stall (2026-05-27)

## Negative result — closes a hypothesis

The hypothesis that **`OHGenericServiceBinder.<clinit>` allocating
`ConcurrentHashMap` in the `LOGGED_ONCE` static field** triggered the
child mark_sweep abort during Batch B deployment is **REJECTED**.

B-fix-2 removed the `<clinit>` method entirely and removed the
`LOGGED_ONCE` field, replacing `logOnce` with unconditional
`System.err.println` (no allocation, no field read). Result: HW fails
identically to B-fix-1 with child stderr stuck at 4,198 bytes (vs
4,027 bytes for B-fix-1 with `<clinit>` present, vs 1,043,372 bytes
for healthy baseline).

Last child marker either way: `[CHILD_CK] CK_BEFORE_initChild_call`.
No `[RDFL_CP] A_entry`. No SEGV, cppcrash, or kickdog signal.

## Why this matters for the Fix A rule scope

The Fix A allocation rule (2026-05-26) — "parent eager-resolve only,
never eager-allocate, because child mark_sweep can't reconcile native
pointers allocated in parent VM state" — is correct for the
**AppSpawnXInit.preload()** parent-side `Class.forName()` pattern.

But the rule does NOT generalize to "no `<clinit>` heap allocations in
BCP classes". B's deployment failure is not a `<clinit>` heap issue.
Many BCP classes already have non-trivial `<clinit>` heap allocations
(strings, exception singletons, default registries) and ship fine in
the LIVE A+CD substrate.

The actual cause of B's bootstrap stall is unknown after this run, but
it is **NOT** in:
- `OHGenericServiceBinder.<clinit>` heap state (removed in B-fix-2 → no change)
- `LOGGED_ONCE` field presence (removed in B-fix-2 → no change)
- d8 vs smali bytecode layout (proven in B-fix-1 with d8 alt-build → no change)
- classes.dex contamination (proven in B-fix-1 with diff → ONLY 8 new classes)

Remaining live hypotheses (untested as of B-fix-2):
- Boot image layout shift (8 new classes change OAT-relative addressing
  for subsequent boot segments).
- Verifier loop on first-ever direct `extends Binder` subclasses in BCP.
- Hidden type-hierarchy bug in `adapter-mainline-stubs` stubs that B's
  resolution path exposes (BFIX1 doc noted `Landroid/net/ParseException`
  is a stub extending Object instead of Throwable).
- OHServiceManager.smali modification (load-bearing in B baseline; not
  isolated from the 8 stub additions).

## Next-agent guidance

1. **Do NOT re-attempt B as "remove some heap allocation from `<clinit>`".**
   That hypothesis space is closed. Both presence and absence of the
   allocation produce the identical stall.

2. **Do NOT extend the Fix A rule to "no BCP `<clinit>` heap allocation".**
   That extension is unsupported by evidence and would be misleading
   guidance for future agents.

3. To make progress on B, the next agent must:
   - Sub-bisect B's classes.dex content (per BFIX1 recommendation in
     `V3-BFIX1-DEPLOYED-2026-05-27.md` section "Recommended next steps")
     into smaller increments — modified OHServiceManager only, or
     OHGenericServiceBinder only, etc.
   - OR add libart instrumentation between `CK_BEFORE_initChild_call`
     and `[RDFL_CP] A_entry` to identify which specific BCP class
     hangs the child's class init / verifier walk.

## Substrate state at end of B-fix-2

Rolled back. Live state is the same as post-bfix1 / post-bisect-2:
- libart.so `fbd2b928`
- appspawn-x `3abe3bde`
- adapter-runtime-bcp.jar `0d51de28` (A live)
- oh-adapter-framework.jar `13ed0c8e` (B baseline, B rejected)
- boot.oat `1c667e4b`, boot-framework.oat `09a61e56`,
  boot-oh-adapter-framework.oat `331ac6ee`

## Evidence

- `$HOME/openharmony/docs/engine/V3-BFIX2-DEPLOYED-2026-05-27.md`
  — full B-fix-2 deploy record + per-phase metrics.
- `$HOME/openharmony/docs/engine/V3-BFIX2-EVIDENCE/` — fix source,
  built dex/jar, HW failure + rollback artifacts.
