---
name: appsched-bytecode-breaks-preload-2026-05-26
description: "AppSchedulerBridge bytecode shape changes can break AppSpawnXInit.preload's eager-resolution edge for SOME apps but not others. Proven 2026-05-26: a ~25 LOC try-catch+rethrow addition to ensureBindApplication compiled fine, HW rendered normally, but McD died upstream after only 16 dex registrations (baseline ~25+). Cause: javac/d8 synthetic methods from exception-handling shifted layout; Fix A 2026-05-22 eager-resolution edge that AppSpawnXInit.preload pre-resolves became broken-link stub for McD's preload chain only."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**When modifying `AppSchedulerBridge.java` (especially adding try/catch or rethrow logic), expect McD-style preload regression even if HW passes.** Fix A's eager Class.forName resolution in AppSpawnXInit.preload is fragile to bytecode-layout shifts.

**Why:** Fix A 2026-05-22 (`AppSpawnXInit.preload` calling `Class.forName("adapter.activity.AppSchedulerBridge", true, ...)`) pre-resolves AppSchedulerBridge at PARENT process init. The COW fork pattern then gives CHILD an inherited resolved class table. If the recompiled AppSchedulerBridge has synthetic methods or different layout, the resolved class table in CHILD becomes stale — manifests as method dispatch failures during the CHILD's framework preload chain.

McD's preload chain is more complex than HW's (touches more classes, follows more eager-resolution edges). It regresses earlier when the parent-resolved class table doesn't match the loaded jar.

**How to apply:**
1. Always test BOTH HW AND McD after any AppSchedulerBridge change
2. Don't trust HW PASS alone — it doesn't exercise the same preload chain
3. If McD regresses but HW passes: suspect Fix A eager-resolution stale; try restarting appspawn-x with extra cache clear OR rebuild Fix A list
4. Minimize bytecode delta: prefer EDIT existing methods over ADD new try/catch blocks. Each new synthetic class file shifts the layout.

## Concrete incident (2026-05-26 CoreComponentFactory fix attempt)

Attempted to add to `AppSchedulerBridge.ensureBindApplication`:
- (d.1) ~10 LOC try/catch rethrow on InvocationTargetException
- Caller-side guard against scheduling LaunchActivityItem on bind failure
- (d.2) ~15 LOC factory-skip filter in `applyManifestFieldsToAppInfoLocal`

Result:
- HW: PASSED (full UI render, lifecycle CREATED+RESUMED, pid alive)
- McD: REGRESSED — only 16 dex registrations vs baseline ~25+, died silently BEFORE AppSpawnXInit.preload finished, `[FACTORY-SKIP]` markers never fired (didn't reach that code path)

Hypothesis: the new try/catch synthetic class files shifted AppSchedulerBridge's method layout. Fix A 2026-05-22's eager Class.forName had pre-resolved AppSchedulerBridge in the PARENT zygote with the OLD layout's method references. CHILD process inherited the stale resolved references, then loaded the NEW jar at boot, hitting broken-link stubs on resolution lookup for some methods.

Rolled back to Tier 3 baseline. d.2-only retry approach should test this hypothesis: if removing d.1 (the try/catch additions) lets McD progress while keeping d.2 (factory-skip, which only modifies an existing method body), then bytecode shape is confirmed as the perturbation.

## What to do next time

If McD regresses but HW doesn't on an AppSchedulerBridge change:
1. ROLLBACK immediately (substrate stays clean)
2. Check whether the change adds new methods/classes vs editing existing method body
3. If new methods: try refactoring to edit existing methods instead (no new synthetic classes)
4. If editing existing methods: bytecode delta should be minimal; suspect different cause
5. As a last resort: rebuild Fix A 2026-05-22 list to include any new eager-resolution targets

## See also

- [[v3-helloworld-renders-fix-a-2026-05-22]] — original Fix A eager-resolution work
- [[v3-scope-c-pib-relocation-2026-05-25]] — Scope C + Tier 3 (where AppSchedulerBridge lives now)
- [[reference-local-build-infra-2026-05-25]] — local build pipeline (where bytecode shape changes originate)
- [[feedback-bcp-first-jar-wins-2026-05-25]] — BCP ordering invariant (parallel layout-mismatch family)
