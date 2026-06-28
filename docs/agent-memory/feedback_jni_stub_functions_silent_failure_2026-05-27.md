---
name: jni-stub-functions-silent-failure-2026-05-27
description: "FEEDBACK — When porting JNI registration tables, every method in the JNINativeMethod[] array MUST have a real implementation. Stub functions (4-byte `bx lr` no-ops returning void or zero) pass RegisterNatives validation, run without exception, but break callee invariants. Failure manifests far downstream as cryptic errors from native code that depends on the stubbed setup. Discovered 2026-05-27: J.2-G shipped 3 stub registrations for SQLite extension functions; downstream FTRHXContentProvider failed at REINDEX LOCALIZED with no signal pointing back to the stubs. Diagnosed only via nm --print-size showing the 4-byte function bodies."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

When porting a JNI registration block from AOSP, **every method in the table needs a real implementation**, not a stub. Stub bodies (`return;` or `return 0;` or just `bx lr`) silently break invariants of the wider native subsystem the methods belong to.

DON'T:
```cpp
static void nativeRegisterLocalizedCollators(...) {} // placeholder, fill in later
```

DO either:
- Port the AOSP implementation fully (often involves additional subsystem like ICU/audio/binder)
- Skip the registration entirely (omit from JNINativeMethod[]) — caller gets UnsatisfiedLinkError, which is LOUD and points at the missing method
- Implement a minimal real version that maintains the contract (e.g., for `RegisterLocalizedCollators` minimal would be calling `sqlite3_create_collation` with a no-op comparator — at least the collation EXISTS in the SQLite engine)

NEVER:
- Ship empty/no-op functions and assume "we'll come back to it"
- Assume downstream code won't depend on the side effects

## Why this trap is severe

A stub-registered native method silently passes RegisterNatives. App code calls it. Function returns. App proceeds. Later, OTHER native code (often deep in some subsystem like SQLite, audio, ICU) hits an invariant that depended on the stub having done real work, and throws a cryptic error from that subsystem.

The error message DOES NOT NAME the stub. The user-visible failure (in our case: `SQLite error 1: unable to identify the object to be reindexed`) is 4 layers downstream from the actual gap (`nativeRegisterLocalizedCollators` no-op).

J.2-G fired:
- 50 native methods registered, 3 of them no-op stubs
- McD's `FTRHXContentProvider.onCreate` calls `SQLiteConnection.open()` — works (real implementation)
- `setLocaleFromConfiguration` calls `nativeRegisterLocalizedCollators` — works (returns void, calls go through stub)
- `REINDEX LOCALIZED` SQL runs — SQLite engine looks up "LOCALIZED" collation — NOT REGISTERED (stub didn't call sqlite3_create_collation) — throws error
- `nativePrepareStatement` throws this error
- `handleBindApplication` catches as InvocationTargetException, marks bind failed
- B47-SLA gate refuses to schedule LaunchActivity
- AMS times out 25s later, kills app
- Looks like a Handler/Looper dispatch wall (which doesn't exist)

We spent ~2 days framing this as a Handler/Looper wall before the diagnostic revealed the stubs.

## How to apply

**Pre-deploy check after porting any JNI registration:**

```bash
# Verify every registered native has > 8 bytes of code body
arm-linux-androideabi-nm --print-size --defined-only /path/to/lib.so | \
    awk '/ [tT] / { size = strtonum("0x" $2); if (size < 16) print $0 }'
```

Anything < 16 bytes is suspicious. Real JNI implementations are typically 50-500 bytes.

**Code review checklist for JNI registration ports:**

1. Did I implement all N methods in JNINativeMethod[] with real bodies?
2. Do any of them just `return` or `return nullptr`?
3. Did I read the AOSP-equivalent implementation for each one to know what work it does?
4. If a method has a side effect (registers a collation, hooks a callback, allocates a resource), is that side effect actually performed?
5. Did I bench-test each method (or test the downstream code that uses it) before declaring the port complete?

## Anti-example today

J.2-G's `android_database_sqlite.cpp` had implementations for the 47 "core" methods (nativeOpen, nativeClose, nativePrepareStatement, nativeExecute, etc.) ported from AOSP, but 3 "extension" methods got stubbed:

```cpp
static void nativeRegisterLocalizedCollators(JNIEnv*, jclass, jlong, jstring) {
    // TODO: implement
}
static void nativeRegisterCustomScalarFunction(JNIEnv*, jclass, jlong, jstring, jobject) {
    // TODO: implement  
}
static void nativeRegisterCustomAggregateFunction(JNIEnv*, jclass, jlong, jstring, jobject) {
    // TODO: implement
}
```

These needed: `sqlite3_create_collation_v2()` with an ICU UCollator (for LOCALIZED), and `sqlite3_create_function_v2()` with Java callback bridging (for the custom functions). All non-trivial; agent skipped them. They were missed in the "twin-build determinism check" because both builds produced bit-identical stubs.

## How to retrofit

For J.2-G+: implement `nativeRegisterLocalizedCollators` using `sqlite3_create_collation_v2(...,SQLITE_UTF16,UCollator*,...)` with libicui18n's `ucol_open`. Implement the 2 custom function stubs analogously (callable from JNI back into the Java SQLiteCustomFunction object). Rebuild liboh_android_runtime.so. Deploy. McD passes Forter init.

## See also

- [[v3-fix-j2-g-landed-2026-05-27]] — original J.2-G with the 3 stubs
- [[v3-handler-diag-2026-05-27]] — diagnostic that revealed the stubs
- [[aosp-version-jni-signature-drift-2026-05-27]] — sister trap (signature mismatch vs stub mismatch)
- [[feedback-engine-principle-validation-2026-05-24]] — "architectural fix vs band-aid" rule applies: don't ship partial implementations
