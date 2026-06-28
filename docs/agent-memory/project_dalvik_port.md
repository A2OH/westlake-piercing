---
name: dalvik-port-status
description: Dalvik VM 64-bit port status, fixes applied, and integration with OHOS QEMU
type: project
---

## Dalvik VM 64-bit Port — Current State

### B0 (#481): COMPLETE — Hello World runs on x86_64
```
Hello from Dalvik on Linux!
os.arch = x86_64  |  java.vm.name = Dalvik  |  1 + 1 = 2  |  Done!
```

### Root Cause Fix (the big discovery)
**ObjectInlines.h `dvmGetFieldObject`/`dvmSetFieldObject`** read/wrote 8 bytes (via `JValue.l`) for instance object fields that are only 4 bytes in Dalvik heap objects. This corrupted adjacent fields, causing cascading pointer corruption. Fixed to use `*(u4*)` reads and `(u4)(uintptr_t)` writes.

### All Fixes Applied (17 total)
1. Fix 6: return-object `retval.l` + GET_REGISTER_AS_OBJECT
2. Fix 36: sizeof(u4)==sizeof(ClassObject*) assert removed
3. Fix 38: Class hash callback `return 1` vs `(int)clazz`
4. Fix 42: Interface idx `(uintptr_t)` cast
5. Fix 43: nativeFunc ANDROID_MEMBAR_STORE + direct write
6. 64bit-class-fix.patch: Interface linking per-element extraction
7. CheckJni.cpp: `const dreg_t* args` signature match
8. link_fixups.cpp: removed duplicate stubs
9. AGET_OBJECT: pointer-width element reads
10. getLongFromArray/putLongToArray: proper dreg_t slot handling
11-12. Stack.cpp: long/double arg memcpy → dreg_t slot split
13. outsSize stack overflow: sizeof(dreg_t)
14. **ObjectInlines.h: field accessor width** (ROOT CAUSE)
15-17. ARM32 build: void* casts, const jvalue*, return type

### Patches
- `$HOME/android-to-openharmony-migration/dalvik-port/patches/dalvik-kitkat-64bit-all.patch` (6054 lines, 87 files)
- NEVER run `git checkout -- .` on `$HOME/dalvik-kitkat/`

### Build
- x86_64: `cd dalvik-port && make -j$(nproc)` → `build/dalvikvm`
- ARM32: `make TARGET=ohos-arm32 -j$(nproc)` → `build-ohos-arm32/dalvikvm` (7.1MB static)

### A1 (#470): IN PROGRESS — QEMU system instability
- QEMU boots, init runs, data mounts
- Kernel panics from SMP IPI handler (service crashes)
- Console service stopped before shell available
- Dalvikvm files injected into userdata.img but can't run them
- **Why:** Headless standard system has many services that crash, destabilizing kernel
- **How to apply:** Need to either use initramfs approach or add dalvikvm as init service

### GitHub Issues (A2OH/harmony-android-guide)
- #481 B0: DONE (dalvik fixes)
- #471 A2: DONE (ARM32 build)
- #470 A1: IN PROGRESS (QEMU execution)
- #472 A3: TODO (deploy package)
- #473 A4: TODO (e2e smoke test)
