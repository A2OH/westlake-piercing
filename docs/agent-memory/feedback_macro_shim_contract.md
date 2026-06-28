---
name: Binder macro-shim anti-drift contract (mandatory in every agent brief)
description: Defines macro vs micro shim boundary; forbidden patterns; self-audit gate that every Builder brief must include going forward
type: feedback
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---
The binder pivot V2 design defines a clear substitution boundary. Every Builder
agent brief MUST include this contract verbatim (or by reference) and the agent
MUST verify compliance before completing.

## The macro-only boundary

**Macro shim (PERMITTED):**
- Implementing public/protected API methods on classes WE own:
  - Westlake-shadowed framework classes: `android.app.Activity`, `android.app.Application`, `android.view.Window`, `com.android.internal.policy.PhoneWindow/DecorView`, `android.view.WindowManagerImpl`, `android.content.res.Resources`
  - Westlake-owned service classes: `WestlakeContextImpl`, `WestlakeResources`, `WestlakeAssetManager`, `WestlakeSharedPreferences`, `WestlakePackageManagerStub`, `WestlakeContentResolver`, `ColdBootstrap`
  - Westlake-owned binder services: `WestlakeActivityManagerService`, `WestlakePowerManagerService`, `WestlakeWindowManagerService`, `WestlakeDisplayManagerService`, `WestlakeNotificationManagerService`, `WestlakeInputMethodManagerService`, `WestlakePackageManagerService`
- Each method body must be one of:
  - (a) AOSP-default verbatim (e.g., `theme.applyStyle(resid, true)`)
  - (b) Safe primitive (return null / false / 0 / empty list / no-op)
  - (c) Delegation to another method on our own class
- `ServiceMethodMissing.fail(...)` for genuinely-unimplementable methods (CR2 pattern)

**Micro shim / drift (FORBIDDEN):**
- `sun.misc.Unsafe.allocateInstance(...)` on framework.jar classes
- `Field.setAccessible(true)` + reflective set on framework.jar internal fields
- "Planting" state on framework's `ResourcesImpl`, `AssetManager`, `Configuration`, `Theme`, `ActivityThread`, `LoadedApk`, `ContextImpl`, etc.
- Per-app branches (`if (pkg.equals("com.mcdonalds.app")) ...`)
- Workarounds that catch `NoSuchMethodError` / `LinkageError` from framework class reflection
- Adding new methods to `WestlakeContextImpl` (CR22 freeze)

## Self-audit gate (run before reporting complete)

```bash
# Zero new Unsafe usage
grep -rn "sun.misc.Unsafe\|jdk.internal.misc.Unsafe\|Unsafe.allocateInstance" <touched files> | grep -v "^.*://"
# Zero new setAccessible
grep -rn "setAccessible(true)" <touched files> | grep -v "^.*://"
# Zero new per-app branches
grep -rniE "noice|mcdonalds|com\.mcd|noice\.fragment" <touched files> | grep -v "^.*://\|^.*// "
```

If any of the above produce results that are NEW (i.e., not pre-existing in the
file before this CR), the agent has DRIFTED and must STOP, revert, and report.

## Architectural validation

Each CR's report must answer:

1. **Surface ownership:** "All methods edited live on classes I own (list them)."
2. **No plant pattern:** "Zero new Unsafe / setAccessible / framework-private-field access."
3. **No per-app branches:** "Zero new per-app string literals or class-name matches."
4. **Boundary respect:** "If a fail-loud was promoted, it was promoted on a class I own — not via reflection on framework.jar."

## When in doubt

If the next blocker would require touching a framework.jar internal class, the
correct response is NOT to plant the field. The correct responses are:
- (a) Shadow that framework class entirely (e.g., CR30-B shadowed Resources)
- (b) Provide a thin Westlake substitute that the framework class wraps
- (c) Document the blocker + STOP, dispatch an architect CR to redesign

This contract supersedes any earlier brief instruction that implies field-plant work.
