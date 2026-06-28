---
name: dlopen-in-child-allocates-2026-05-27
description: "ENGINE INVARIANT extension — System.loadLibrary / dlopen in AppSpawnXInit.initChild (post-fork child) ALSO breaks the Fix A allocation rule. dlopen itself allocates linker state (DSO descriptor, __cxa_atexit handler chain, TLS slots) outside the child's CMS-tracked heap spaces → HeapTaskDaemon mark_sweep.cc:487 SIGABRT. Even a 4KB probe .so with only write() and zero C++ runtime triggers it. dlopen counts as ALLOCATE, not RESOLVE. Move native loading to substrate-time (link into libart.so or appspawn-x) and register methods via parent-zygote RegisterNatives (RESOLVE-archetype, COW-safe)."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**Adding `System.loadLibrary("name")` to `AppSpawnXInit.initChild` is UNSAFE even though initChild runs post-fork.**

The Fix A allocation rule originally distinguished:
- Parent preload + RESOLVE (Class.forName) → SAFE (COW-inheritable)
- Parent preload + ALLOCATE (Resources.getSystem) → UNSAFE (mark_sweep SIGABRT in child)
- Child initChild + RESOLVE → SAFE
- Child initChild + ALLOCATE (Provider object via Security.insertProviderAt, J.1) → SAFE (per-child heap)

J.2 (2026-05-27) discovered a NEW failure: **child initChild + dlopen → ALSO SIGABRT.**

Why: `dlopen` allocates linker state (DSO descriptor, __cxa_atexit handler chain, TLS slots, .bss spaces) via raw mmap/sbrk, NOT via the GC-managed allocator. These spaces exist in the process but are not registered with libart's heap. When HeapTaskDaemon's CMS sweep walks references and hits a pointer into one of these spaces, `mark_sweep.cc:487` triggers: `Tried to mark 0x... not contained by any spaces`.

## Validation evidence

J.2 agent built **4 progressive iterations** narrowing the failure surface:
1. Full sqlite_jni.so with std::mutex → libc++ symbol gap (caught early, fix iteration)
2. + pthread_mutex + libc++_shared → libc++ symbol gaps continue
3. + static libc++ → ARM EHABI gap (dl_unwind_find_exidx)
4. + 4 bionic shims → ALL symbols resolve, then HeapTaskDaemon abort fires

Then the elimination test: built a **4KB probe .so** with only `write()` syscall and NO C++ runtime. System.loadLibrary("probe") in initChild → **SAME HeapTaskDaemon SIGABRT**. This isolates the cause to the `dlopen` mechanism itself, not the .so contents.

## How to apply

**For loading new native code into Westlake-spawned child processes:**

1. ✗ DO NOT add `System.loadLibrary` to `AppSpawnXInit.initChild`
2. ✗ DO NOT add `System.loadLibrary` to `AppSpawnXInit.preload` either (Fix A original rule — parent allocation breaks COW too)
3. ✓ Statically link the native .o into `libart.so` or `appspawn-x` at substrate build time — native symbols available before any Java starts, no dlopen ever happens
4. ✓ Register JNI methods via explicit `RegisterNatives` call from C++ in `appspawn-x` parent init (RESOLVE-archetype, COW-safe — modifies class table only, no allocation)
5. ✓ Or: pre-bake native loading into boot image via dex2oat (if dex2oat supports `--with-native`)

## What's NOT covered by this rule

- APK-shipped native libs loaded via the APK's own `System.loadLibrary` calls. Those happen via `BaseDexClassLoader.findLibrary()` after the app's Application.<init>, not in initChild. May still cause issues with GC tracking, but is a DIFFERENT failure surface (deferred to later investigation).

## Anti-example

J.2 attempt:
```java
public static void initChild(...) {
    OHSecureRandomProvider.registerIfNeeded();  // J.1: pure Java allocation, safe
    System.loadLibrary("sqlite_jni");           // J.2: dlopen, UNSAFE → HeapTaskDaemon SIGABRT
    // ...
}
```

Rolled back. Same Class.forName / Provider allocation patterns survive but dlopen does not.

## Why this is structural, not a libart bug

OHOS's libart is [build-host]-built with their CMS heap implementation. The native heap tracking expects all live references to land in registered spaces. dlopen-introduced spaces are intentionally outside that — they belong to the dynamic linker, which is per-process state. There's no clean way to register them with libart's GC without modifying libart, which is the wrong layer.

The right layer to fix this is the BUILD: statically link native bridges into the substrate so dlopen never happens in child processes.

## See also

- [[v3-fix-j1-landed-2026-05-27]] — J.1 SecureRandom in initChild SUCCEEDED (Java Provider allocation, not dlopen)
- [[v3-fix-j2-landed-2026-05-27]] — J.2 SQLite dlopen FAILED with this rule
- [[feedback-fix-a-resolve-not-allocate-2026-05-26]] — original Fix A rule (parent allocation broken)
- [[feedback-appsched-bytecode-breaks-preload-2026-05-26]] — d.1 lesson on minimizing AppSpawnXInit/AppSchedulerBridge bytecode changes
