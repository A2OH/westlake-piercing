---
name: Subtractive validation, not additive shimming
description: When an app misbehaves, start from a fully-working baseline (real Android) and subtract layers until it breaks. Never add shims speculatively. Codified 2026-05-12 after a week of additive patches failed to converge.
type: feedback
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---
When debugging Westlake's compatibility with a real app, the *epistemically correct* method is subtraction:

1. Start from a baseline where the app runs perfectly (real Android, native execution).
2. Substitute one Westlake layer in (or remove one real-Android layer) at a time.
3. Observe which substitution FIRST breaks rendering or behavior.
4. That layer is the one that's load-bearing AND requires Westlake coverage. Everything above it works without intervention.

**Why:** The opposite reflex (observe NPE → add shim → repeat) produces linear coverage growth with app count, never converges, and accumulates per-app workarounds masquerading as generic shims. Over the week of 2026-05-05 to 2026-05-12, tasks #86–#97 followed the additive pattern. Each unblocked ~50 dex bytecodes. The pattern was unsustainable and resulted in 3087 LOC of off-architecture bypass code (WestlakeFragmentLifecycle) plus a custom dex parser (DexLambdaScanner) before the strategic error became visible.

**How to apply:**
- Every new debugging effort starts with: "what's the smallest substitution from real-Android-stack that reproduces the failure?"
- Build the subtractive harness before adding any new shim. The harness is reusable across apps.
- If a fix can be expressed as "the existing framework path would have worked if X service implemented method Y" → implement the service method, not a bypass.
- If a fix requires reflection on framework internals, that's a smell — investigate why the framework's own construction path didn't run.

**Anti-patterns** (codified in `docs/engine/AGENT_SWARM_PLAYBOOK.md` §3):
- Additive shimming (observe-NPE → add-class → repeat)
- Renderer-time substitution
- Per-app hardcoded shortcuts
- Reflection on framework objects to bypass natural construction paths
- Speculative completeness

**Reference:** see `docs/engine/BINDER_PIVOT_DESIGN.md` §2.4 for the full rationale.
