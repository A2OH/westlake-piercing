---
name: r8-inlined-stacks-hide-root-cause-2026-05-28
description: "FEEDBACK — R8 inlining collapses multiple distinct exception sites into the same method (and reports the inlined source line as 'Unknown Source:NNN' with different :NNN per site). Fixing the first observed exception can unmask a SECOND exception in the SAME method at a DIFFERENT line that has a completely different root cause. Always compare the :NNN line numbers before/after fixes — if the line shifted, the fix was for a symptom, not the cause. Discovered W2 v3 2026-05-28 — fixed CloneNotSupportedException at ClassesInfoCache.a:132 only to unmask ClassCastException at ClassesInfoCache.a:197 in same R8-inlined method."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

When debugging exceptions in R8-obfuscated code (typical for production Android apps like McD), the source line shown as `Unknown Source:NNN` is the inlined location within the synthetic method. Two distinct exceptions in the same `methodName` may have DIFFERENT `:NNN` values reflecting DIFFERENT code paths.

**Always record the `:NNN` line number when capturing a wall.** When the fix is applied and you see "the same exception is gone but another exception in the same method appears," CHECK THE `:NNN` — if it shifted, you fixed a symptom and unmasked the cause.

## Concrete example (W2 series 2026-05-28)

**Baseline** (W1 wall, before any W2 patch):
```
CloneNotSupportedException
    at java.lang.Object.clone(Object.java:263)
    at androidx.lifecycle.ClassesInfoCache.a(Unknown Source:132)  ← :132
    at androidx.lifecycle.ClassesInfoCache.d(Unknown Source:36)
    at androidx.lifecycle.LifecycleRegistry$ObserverWithState.<init>
```

**W2 v1** (added clone()=return this to Proxy): same CNSException gone, McD throws different exception:
```
ClassCastException: $Proxy3 cannot be cast to androidx.lifecycle.Lifecycle$Event
    at androidx.lifecycle.ClassesInfoCache$CallbackInfo.<init>(Unknown Source:36)
    at androidx.lifecycle.ClassesInfoCache.a(Unknown Source:197)  ← :197 (DIFFERENT LINE!)
    at McDMarketApplication.onCreate
```

**W2 v3** (reflective shallow clone): identical CCE at :197.

The `:132` and `:197` are different inlined sites in the same R8 synthetic `ClassesInfoCache.a()`. Reading the smali for the method reveals two distinct code paths:
- `:132` — the line that called `Object.clone()` on the annotation (we fixed this)
- `:197` — the line that did `(Lifecycle.Event) entry.getValue()` (this is the actual problem)

The root cause: McD's annotation `Proxy.value()` returns the Proxy itself instead of the resolved Enum constant. This causes the map (`Map<Method, Lifecycle.Event>`) to contain Proxy refs instead of Event enums. The `(Lifecycle.Event)` cast fails. The clone() was a separate, unrelated step that we accidentally fixed first.

## How to apply

When fixing a wall captured via `[B43-BIND] Caused by` chain:

1. **Record the full stack with line numbers.** Save `:NNN` for every frame.
2. **After applying fix, re-test and compare stack lines.** If `:NNN` changed, you unmasked a deeper bug.
3. **Before committing to "fix worked," verify the SAME `:NNN` no longer fires.** Then check the rest of the bind chain for new walls at OTHER `:NNN` lines.
4. **For R8-obfuscated apps**: baksmali the relevant method and read both inlined paths. Map `:NNN` to actual source lines in the synthetic.

## Anti-example today

W2 v1 → W2 v2 → W2 v3 all targeted `Proxy.clone()` based on the original CNS stack. After three iterations:
- W2 v1 fixed clone() at :132 — exposed CCE at :197 (we missed this).
- W2 v2 took the wrong direction (Cloneable in interfaces) — hit Fix A rule, rolled back.
- W2 v3 fixed clone() better (reflective shallow copy) — still CCE at :197 because that line was NEVER the clone() line.

Cost: ~6 hours across W2 series, all addressing a symptom. Time would have been saved by reading the smali for `ClassesInfoCache.a()` after W2 v1 to confirm we were attacking the right line.

## See also

- [[feedback-sysfreeze-vs-stderr-diagnostic-2026-05-27]] — read hilog [B43-BIND] not sysfreeze
- [[v3-fix-w2-v3-2026-05-28]] — full diagnostic that uncovered this pattern
- [[feedback-engine-principle-validation-2026-05-24]] — "architectural fix vs band-aid" rule applies here too
