---
name: V2 substrate port to OHOS — directional decision
description: 2026-05-15 strategic commitment to port full V2 binder substrate to OHOS (M10-M13) instead of CR-BB XComponent HAP path; deep-dive analysis precedes any code
type: project
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
status: SUPERSEDED-BY project_v3_hbc_reuse_direction.md (2026-05-16)
---

> **SUPERSEDED 2026-05-16** by `project_v3_hbc_reuse_direction.md` /
> `docs/engine/V3-ARCHITECTURE.md`. The V2-OHOS direction documented
> below was the strategic commitment for ~24 hours (2026-05-15 morning
> to 2026-05-16 morning). It is superseded by V3 ([build-host]-runtime reuse)
> after CR-EE / CR-FF audits demonstrated [build-host] has independently solved
> the OHOS Android-app hosting problem with real AOSP-14 ART + real
> `framework.jar` cross-built to OHOS musl. Reasoning preserved here
> verbatim for traceability per the memory preservation discipline.
> The V2 Phase-1 Android-phone path documented in `project_binder_
> pivot.md` is UNCHANGED by V3. See `feedback_two_pivots_in_two_days.md`
> for the discipline note on this rapid pivot.

## Decision

**2026-05-15:** Westlake Phase 2 OHOS will pursue **full V2 substrate port** (M10 own libbinder + M11 audio daemon for OHOS + M12 surface daemon for OHOS) rather than CR-BB Candidate C (XComponent HAP + same-process dalvikvm).

## Why

Two CR-BB candidates were considered:
- **Candidate C (XComponent HAP)** — 4-6 weeks MVP. Single window per HAP. dalvikvm runs as worker thread inside ArkTS HAP. Westlake apps appear as sub-views, NOT peer windows.
- **V2-OHOS port** — 8-12 weeks. Multi-window via own binder + ServiceManager. Westlake apps as peer OHOS apps (Westlake-managed first-class windows).

The user chose **V2-OHOS** because Westlake's product goal is "Android apps as first-class OHOS citizens managed by WindowManager alongside ArkTS apps." Candidate C's "single window per HAP, sub-views" structure doesn't deliver that. V2-OHOS does.

This is also consistent with `feedback_additive_shim_vs_architectural_pivot.md` (2026-05-15) — the previous additive shim chain (CR-W → CR-X → CR-Y → CR-Z) was the canonical bad-example. V2-OHOS is the architectural-commitment path.

## Mandatory pre-implementation: top-down render-pipeline analysis

Before writing any V2-OHOS code:

1. **Deep-dive analysis of OHOS rendering pipeline.** Identify GLUE POINTS where a non-OHOS-app process can submit frames to render_service. Each candidate glue point needs: API contract, IPC layer, SELinux constraints, performance characteristics, CR61 implications.
2. **Codex review** of the analysis (independent 2nd opinion on glue point choice).
3. **Mock APK validation** — small test app that proves the chosen glue end-to-end before committing to surface daemon implementation.

Only THEN: westlake-surface-daemon for OHOS implementation (M12), audio daemon (M11), and integration (M13).

## What transfers from prior work

Per `feedback_additive_shim_vs_architectural_pivot.md` audit, ~85% of today's code transfers to V2-OHOS:
- CR60 32-bit dalvikvm (E1-E11) — bitness prerequisite
- CR59 Hilt App.mBase — pure Java substrate
- CR-W setContentView NPE fixes — substrate plumbing
- CR-X arsc parser + theme resolution — Resources plumbing
- CR-X+1 lifecycle drive — substrate
- CR-Y+1 Locale.forLanguageTag BCP patch — libcore gap
- CR-Z Date/ByteOrder clinit fixes — libcore gaps
- CR62 thread-local Context, CR63b Configuration.setTo, CR64 STRIP_CLASSES — substrate

What's discarded (relegated to demo fallback):
- SoftwareCanvas (~200 LOC) — replaced by real Skia path TBD
- drm_inproc_bridge.c (~300 LOC) — replaced by V2 surface daemon
- InProcessAppLauncher's buffer materialization (~100 LOC) — replaced by daemon protocol

## In-flight work that's still useful

Pre-W0 spike (libdvm_arm32.so shared library + signal chaining, agent 36 in flight) — useful regardless of path. Even V2-OHOS may benefit if surface daemon hosts dalvikvm in-process (one design option). Don't cancel.

## Cross-references

- `docs/engine/CR41_PHASE2_OHOS_ROADMAP.md` — original M9-M13 plan (this is the path)
- `docs/engine/CR-BB-OHOS-RENDER-STRATEGY.md` — CR-BB pre-V2-decision research (not to be implemented; kept for context)
- `docs/engine/CR60_BITNESS_PIVOT_DECISION.md` — bitness prerequisite for V2-OHOS
- `docs/engine/CR61_BINDER_STRATEGY_POST_CR60.md` — binder strategy (own libbinder on /dev/vndbinder, no OHOS libipc); V2-OHOS direction confirms CR61 holds
- `feedback_additive_shim_vs_architectural_pivot.md` — the lesson that prompted this pivot
