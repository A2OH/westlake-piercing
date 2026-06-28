---
name: Additive shim vs architectural pivot — when to switch
description: When ≥3 consecutive CRs in the same architectural layer keep revealing new symptoms while end-to-end visible result doesn't advance, the addition itself is the smell — pivot the layer instead of patching it
type: feedback
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---
## The rule

When you find yourself dispatching a chain of additive shim/patch CRs in the same architectural layer (canvas, lifecycle, IPC, etc.), and each fix lands cleanly but the END-TO-END VISIBLE RESULT doesn't advance, **stop adding shims and audit the layer itself**. The pattern of "fix one gap → new gap appears → fix that → another gap" is the structural problem signaling itself.

## Why (concrete Westlake examples — 2026-05-15)

**The example that prompted this rule:** Westlake OHOS render path. We hand-rolled `SoftwareCanvas` extending `android.graphics.Canvas` that only recorded `drawColor` + last `drawRect`, dropping `drawText` / `drawBitmap` / `drawPath`. To make noice's `setContentView` reach this canvas, we shipped:
- CR-W: setContentView NPE chain fix (3 NPEs)
- CR-X: arsc parser + theme `windowBackground` resolution
- CR-X+1: lifecycle drive (Activity.onStart/onResume)
- CR-Y+1: BCP-level `Locale.forLanguageTag` patch
- CR-Z: BCP-level `Date`/`ByteOrder` `<clinit>` traps
- CR-Z bootstrap: Hilt UPAE bootstrap

Each landed cleanly. After all 6 CRs (~24 commits, ~½ day's strategy time) the panel pixel was STILL launcher fallback white (CR67 / agent 32 diagnostic confirmed: `op count=4`, `decor tree=FrameLayout > FrameLayout childCount=0`, all 4 canvas samples = `0xfff5fbf4` theme bg only). The substrate was rendering an empty FrameLayout because SoftwareCanvas was architecturally incapable of recording widget ops AND the lifecycle work made `setContentView` "complete" in a recovery shell rather than the real LinearLayout. Each shim fixed one symptom; the structural problem (wrong canvas, wrong recovery semantic) kept generating new symptoms.

**Counter-example that worked (CR60 32-bit pivot, 2026-05-14):** Outside reviewer flagged that we'd been adding M6 daemon + AF_UNIX bridge + SCM_RIGHTS code (~1000 LOC) to route around the bitness mismatch between 64-bit dalvikvm and 32-bit OHOS userspace. Rather than continue, we pivoted dalvikvm to 32-bit. 3-day spike validated. Made M11/M12 cross-arch work moot. Saved weeks.

## How to apply

1. **Count CR depth in the same layer.** When dispatching CR-N in a layer where CR-{N-1, N-2} also lived, ask: does the failure mode form an INFINITE ladder (each fix uncovers another gap) or a BOUNDED chain (3-5 fixes total expected)?

2. **Audit end-to-end visible result.** If the visible "marker" hasn't advanced (panel pixel hasn't changed, log line hasn't moved past the same blocker class), the chain is probably infinite.

3. **Estimate replacement cost.** What would replacing the LAYER cost vs the next 3-5 CRs in the chain? When the comparison is even or replacement is cheaper, pivot.

4. **Bias toward replacement** when:
   - The chain is symptom-driven (each new CR is reactive to a new error)
   - The architectural premise is hand-rolled (SoftwareCanvas, M6 daemon, etc.) when a "real" alternative exists (Skia via libnative_drawing, OHOS XComponent surface)
   - The end goal of the chain is "make wrong thing keep working" rather than "make right thing work"

5. **Bias toward staying additive** when:
   - The chain is naturally bounded (e.g., a fixed list of missing AOSP API methods to stub — each one is generic + needed eventually)
   - The architectural premise is sound; we're only filling expected gaps
   - The end-to-end visible result IS advancing per CR (concrete progress markers move)

## Specific anti-patterns to recognize

- **"Just one more shim and we're there"** — if you've said this for 3+ CRs in the same layer, the layer is wrong
- **"Technical PASS but semantic FAIL"** — when stages PASS but the actual user-visible result hasn't changed (CR67 / CR-AA-diag pattern), the test criteria are checking the wrong things
- **Hand-rolled X when OHOS/Android ships a real X** — SoftwareCanvas vs Skia, M6 daemon vs render_service, etc. The hand-roll is permissible as a placeholder but should never be the production path

## Lesson cost

In Westlake terms: ~10-15% of code today (~500 LOC of SoftwareCanvas + drm_inproc_bridge.c + buffer materialization) is throwaway. ~30-40% of strategy/dispatch time was "polish the wrong path." Most code (~85%) transferred to the correct architecture (CR-BB) because it was generic substrate work (lifecycle, BCP patches, theme resolution). The pure-shim parts didn't transfer.

## Related memories

- `feedback_subtraction_not_addition.md` — debug by removing layers from working baseline, not by speculatively adding
- `feedback_macro_shim_contract.md` — the contract that limits HOW we add (no Unsafe / no setAccessible / no per-app branches)
- `feedback_no_per_app_hacks.md` — only architectural API shims, never per-app
- `feedback_bitness_as_parameter.md` — CR60 the canonical example of pivoting an architectural assumption rather than routing around it
