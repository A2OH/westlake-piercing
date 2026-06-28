---
name: proxy-interfaces-modification-breaks-marksweep-2026-05-28
description: "FEEDBACK — Extending Fix A allocation rule: modifying inputs to native Class generators (e.g., adding Cloneable to java.lang.reflect.Proxy.getProxyClass0's interface array) changes the synthesized class's structure in ways that escape the child's CMS heap tracking → HeapTaskDaemon SIGABRT (mark_sweep.cc:487 'Tried to mark X not contained by any spaces'). Same failure family as Resources.getSystem() in preload (5app-v2) and dlopen in initChild (J.2). Proxy class structural changes count as ALLOCATE, not RESOLVE."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

Extending Fix A allocation rule:

> Modifying inputs to native Class generators counts as ALLOCATE-in-parent, NOT RESOLVE. Breaks child mark_sweep.

Concrete instances now in the rule family:
- `Resources.getSystem()` in `AppSpawnXInit.preload()` (5app-v2 2026-05-26)
- `System.loadLibrary` / `dlopen` in `AppSpawnXInit.initChild` (J.2 2026-05-27)
- **Modifying `Proxy.getProxyClass0` to add interfaces to the synthesized $Proxy<N> class (W2 v2 2026-05-28)** ← NEW

All produce: `mark_sweep.cc:487 Tried to mark 0x... not contained by any spaces` HeapTaskDaemon SIGABRT, often AFTER initial render (so "PASS at initial render, flag HeapTaskDaemon SEGV under sustained use" rule applies).

## Why

When Java code modifies the interfaces array passed to the native Proxy generator (e.g., to add Cloneable), the generated `$Proxy<N>` class has a different shape than the JVM's class table expected. The native generator's internal allocations land in heap spaces that aren't registered with libart's CMS sweep machinery. When child mark_sweep walks heap references, it hits a pointer into an unregistered space → abort.

This is the same general mechanism as preload-time allocations and dlopen-time linker state: any path that allocates native state in heap zones outside libart's CMS tracking will trip mark_sweep eventually.

## How to apply

**For modifying Proxy / Class / Method / Field behavior in core-oj.jar:**

DO NOT:
- Modify `Proxy.getProxyClass0` interface array
- Modify any method that feeds into a native class/method/field generator
- Add Cloneable / Serializable / etc. to generator inputs
- Modify class hierarchy of dynamically-generated classes

DO instead:
- Override the relevant method on the existing Java class (e.g., add `clone()` directly to Proxy.smali)
- Use reflection in a Java-only path to achieve the desired behavior
- For Proxy.clone() specifically: reflective shallow copy via `getDeclaredConstructor(InvocationHandler.class).newInstance(this.h)` — produces a true clone without touching class generators

## Anti-example today (W2 v2 2026-05-28)

```smali
# In Proxy$ProxyClassFactory.apply():
# Old: pass interfaces[] directly to generateProxy
# New: allocate interfaces2[] of length N+1, copy + append Cloneable.class
new-array v_new, v_len_plus_one, [Ljava/lang/Class;
... copy original interfaces ...
sget-object v_clone, Ljava/lang/Cloneable;->class:Ljava/lang/Class;
aput-object v_clone, v_new, v_N
# Then pass v_new to generateProxy
```

Native `generateProxy` accepts the modified interfaces, creates `$Proxy<N>` with the new shape. Allocation lands outside libart CMS spaces. Child HeapTaskDaemon SIGABRTs at `mark_sweep.cc:487`.

## Working fix for the original problem (Wall #2 / #3)

Strategy C (Java-only reflective clone):

```smali
# Add to java/lang/reflect/Proxy.smali (override clone() method):
.method protected clone()Ljava/lang/Object;
    .registers 3
    .annotation system Ldalvik/annotation/Throws;
        value = { Ljava/lang/CloneNotSupportedException; }
    .end annotation
    
    # Get this.getClass()
    invoke-virtual {p0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v0
    
    # Get InvocationHandler constructor
    const/4 v1, 0x1
    new-array v1, v1, [Ljava/lang/Class;
    const/4 v2, 0x0
    const-class v2_class, Ljava/lang/reflect/InvocationHandler;
    aput-object v2_class, v1, v2
    invoke-virtual {v0, v1}, Ljava/lang/Class;->getDeclaredConstructor([Ljava/lang/Class;)Ljava/lang/reflect/Constructor;
    move-result-object v0
    
    # Build args array with this.h
    iget-object v1_h, p0, Ljava/lang/reflect/Proxy;->h:Ljava/lang/reflect/InvocationHandler;
    const/4 v1, 0x1
    new-array v1, v1, [Ljava/lang/Object;
    aput-object v1_h, v1, v2
    
    # Constructor.newInstance(args) returns a new $Proxy<N> with same handler
    invoke-virtual {v0, v1}, Ljava/lang/reflect/Constructor;->newInstance([Ljava/lang/Object;)Ljava/lang/Object;
    move-result-object v0
    
    return-object v0
.end method
```

This produces a true shallow copy without modifying the Proxy class generator. No mark_sweep concerns.

## Lesson on diagnostic methodology

Per the new diagnostic rule (`feedback_sysfreeze_vs_stderr_diagnostic_2026-05-27`): when McD fails after a substrate change, IMMEDIATELY check:
1. Is the failure `HeapTaskDaemon mark_sweep.cc:487` SIGABRT? → Fix A allocation rule family
2. Is the failure a Java exception in [B43-BIND]? → That's the real wall

This W2 v2 failure was the former (Fix A family). Same rule, third instance this session.

## See also

- [[feedback-fix-a-resolve-not-allocate-2026-05-26]] — original Fix A rule (preload allocate-and-cache)
- [[feedback-dlopen-in-child-allocates-2026-05-27]] — extension: dlopen in initChild
- [[feedback-sysfreeze-vs-stderr-diagnostic-2026-05-27]] — how to know it's a Fix A family failure vs Java wall
- [[v3-fix-w2-v2-2026-05-28]] — today's specific encounter (W2 v2 Strategy A)
