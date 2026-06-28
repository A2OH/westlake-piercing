---
name: Real McDonald's app on Westlake — ART v118 switch interpreter
description: MCD SplashActivity.onCreate() runs through Westlake interpreter on phone. 33 DEX files loaded, 100+ native calls interpreted, analytics SDK init reached. NPEs from clinit failures in mock environment.
type: project
---

## Status: SplashActivity.onCreate() executes (2026-04-13)

MCD's SplashActivity.onCreate() runs through Westlake's ART v118 switch interpreter
on the phone (OnePlus 6). All bytecode interpreted, no phone ART in execution loop.

### What Works
- 33 MCD DEX files loaded on bootclasspath ✅
- SplashActivity class loaded (61 methods, 13-level hierarchy) ✅
- Instance allocated via AllocObject ✅
- Activity fields set (mBase, mInstrumentation, mApplication, mComponent, mIntent, mWindow, mToken) ✅
- onCreate() called via reflection ✅
- 100+ native method calls dispatched through InterpreterJni ✅
- Real MCD bytecode runs: reflection (Class.forName, Method.invoke), UUID generation,
  string processing (charAt, toCharArray, newStringFromUtf8Bytes), Math.ceil, nanoTime ✅
- Execution reaches analytics SDK / Hilt DI initialization ✅
- Runs in 170ms total ✅

### Key Fixes Applied
1. **ConcurrentHashMap.U CAS loop** — split re-init chain, set U+offsets between phases
2. **Unsafe.theUnsafe null** — create via AllocObject if clinit failed
3. **Locale re-init FATAL** — skip (causes "Unexpected change back of class status")
4. **LocaleList clinit** — force-mark as kInitialized after pre-setting fields+mList
5. **StandardCharsets.UTF_8 null** — AllocObject(Charset) + set name="UTF-8"
6. **UUID.randomUUID()** — native stub returning sequential UUIDs
7. **SecureRandom.nextBytes()** — native stub with pseudo-random fill
8. **Build fields null** — pre-set 15 Build fields + VERSION.SDK_INT=35
9. **kotlin.text.Charsets.UTF_8** — set from StandardCharsets after clinit tolerance
10. **Shorty patterns** — added DD, DDD, LLII to InterpreterJni dispatcher

### Current Blocker
NPE from `String.contentEquals(CharSequence)` — null String in MCD analytics code.
ConcurrentHashMap.U is still null for NEW CHM instances created during MCD execution
(20+ NPEs tolerated). Root cause unclear — U was set earlier but may be GC'd or
overwritten by failed re-init triggered by charset/class loading.

### Architecture
```
BCP: core-oj-a15.jar:core-libart-a15.jar:art-patch.jar:framework.jar:mcdloader.jar:mcd_classes[1-33].dex
Boot image: boot.art (v118, 49541 methods, interpreter bridges)
Binary: build-bionic-arm64/bin/dalvikvm (25MB, static ARM64)
Phone: OnePlus 6 (Android 15/LineageOS 22.2, rooted)
Deploy: /data/local/tmp/westlake/
```

### Run Command
```bash
cd /data/local/tmp/westlake && ./dalvikvm \
  -Ximage:boot-img/boot.art \
  -Xbootclasspath:core-oj-a15.jar:core-libart-a15.jar:art-patch.jar:framework.jar:mcdloader.jar:mcd_classes.dex:mcd_classes2.dex:...:mcd_classes33.dex \
  -classpath mcdloader.jar \
  McdLoader
```

### Key Files
- `patches/dalvikvm/dalvikvm.cc` — all runtime patches, stub registration, clinit fixes
- `patches/runtime/interpreter/interpreter.cc` — InterpreterJni shorty dispatch
- `patches/runtime/common_throws.cc` — NPE caller logging
- `stubs/framework_native_stubs.c` — native method stubs (Log, SystemProperties, Locale)
- `test-java/McdLoader.java` — main test driver (loads SplashActivity, calls onCreate)
