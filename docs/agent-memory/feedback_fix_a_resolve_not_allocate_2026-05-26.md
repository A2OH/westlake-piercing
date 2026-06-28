---
name: fix-a-resolve-not-allocate-2026-05-26
description: "ENGINE INVARIANT — Fix A 2026-05-22 eager-preload pattern (Class.forName in parent zygote → COW-inherited to children) ONLY generalizes to RESOLVE operations (no allocation). Does NOT generalize to ALLOCATE-AND-CACHE patterns (Resources.getSystem() etc.) because allocated objects in parent heap with native pointers survive COW structurally but child mark-sweep can't recognize the spaces → GC SIGABRT (mark_sweep.cc:487 Tried to mark X not contained by any spaces)."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**Fix A 2026-05-22 eager-preload archetype (`Class.forName(name, true, loader)` in `AppSpawnXInit.preload`) ONLY generalizes to RESOLVE operations.**

DO NOT use the parent-zygote preload pattern for:
- `Resources.getSystem()`
- `Application.<init>` allocations
- `LayoutInflater` initialization
- Any operation that allocates objects with native pointers and caches them in static fields

ALWAYS-safe operations for preload:
- `Class.forName(name, true, loader)` — pure resolve, updates class table only
- Static method invocations that don't allocate
- Constant string interning

## Why

Per 2026-05-26 5app-v2 attempt (commit `75118a4c7ce`):
- Added `Resources.getSystem()` to `AppSpawnXInit.preload()` (parent zygote)
- Built clean, deployed clean, marker fired in parent
- HW immediately SIGABRT'd in HeapTaskDaemon: `mark_sweep.cc:487 Tried to mark 0x5b6fa800 not contained by any spaces`
- Address `0x5b6fa800` is in parent's heap zones below the regenerated boot image base `0x70000000`
- Rolled back; HW + substrate restored byte-for-byte

The COW model preserves PAGE STRUCTURE but child's GC manages spaces based on its OWN allocation timeline. Parent-allocated objects:
- ✓ Are accessible via inherited static references
- ✗ Live in heap zones the child's mark-sweep doesn't track
- → First child GC pass tries to mark them, fails, ABORT

## How to apply

For "I need X to be initialized before per-app Java runs":
1. If X is a class load → ✓ safe to add to `AppSpawnXInit.preload()` via `Class.forName(name, true, loader)`
2. If X requires allocation → ✗ DO NOT put in preload
3. Alternatives for X requiring allocation:
   - Move to `AppSpawnXInit.initChild()` (child-side, post-fork) — IF child Java runs before X is needed
   - Move to C++ side of appspawn-x (before initChild Java) — IF wall fires before Java starts
   - libart-side initialization — IF wall is in libart class init
   - Pre-bake X into boot image (boot.art has Resources class state baked in) — IF dex2oat supports it

## Anti-example today

5app-v2 attempted Resources.getSystem() warmup in parent zygote preload following Fix A pattern. Same code structure as Fix A (single line, inside existing try, marker log after). But Resources.getSystem() allocates AssetManager + ResourcesImpl + Theme + native handles — exactly the heap-zone violation. SIGABRT within first child fork.

## Reverse-validation

Fix A 2026-05-22 originally:
```java
Class.forName("adapter.activity.AppSchedulerBridge", true, AppSpawnXInit.class.getClassLoader());
```
This is pure resolve — no allocation. Updates class table only. Children inherit resolved class. ✓ Worked, McD progressed.

vs 5app-v2 2026-05-26:
```java
android.content.res.Resources.getSystem();  // allocates inside
```
Allocates. Children inherit static reference to a parent-heap allocation. ✗ Crashed.

## See also

- [[v3-helloworld-renders-fix-a-2026-05-22]] — original Fix A success (resolve pattern)
- [[v3-mcd-2026-05-26]] — today's 5app-v2 attempt that surfaced this rule
- [[feedback-appsched-bytecode-breaks-preload-2026-05-26]] — related: another preload-fragility rule
