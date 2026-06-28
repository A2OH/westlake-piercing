---
name: ArkUI ARM32 TLS/dynlink fix
description: OHOS musl libc.a dynlink.o overrides __init_tls and __libc_start_init for static binaries — fix by renaming symbols
type: project
---

## Problem
Static ARM32 binaries that link ArkUI code crash in musl startup (SIGSEGV at NULL) because `dynlink.o` in `libc.a` provides strong definitions that override weak implementations needed for static binaries.

## Root Cause
OHOS musl's `libc.a` contains `dynlink.o` (dynamic linker) which defines:
- `__init_tls` (T, strong) — no-op stub (TLS handled by dynamic linker)
- `__libc_start_init` (T, strong) — calls `do_init_fini` which needs dynamic linker data

These override the correct weak (W) versions from `__init_tls.o` and `__libc_start_main.o`.

**Why:** Something in ArkUI references a symbol from `dynlink.o` (e.g., `__dlsym`, `__dls2`), pulling it in. dalvikvm doesn't reference these symbols, so dynlink.o isn't pulled in for dalvikvm.

## Fix
1. **Patch libc.a:** `llvm-objcopy --redefine-sym __init_tls=__dynlink_init_tls_disabled --redefine-sym __libc_start_init=__dynlink_libc_start_init_disabled dynlink.o`
2. **Custom __init_tls:** Assembly file (`custom_init_tls.S`) that calls `__init_tp` from `__init_tls.o` to properly set up pthread/TLS
3. Patched libc saved as `ohos-sysroot-arm32/usr/lib/libc_static_fixed.a`

**How to apply:** When linking static ARM32 ArkUI binaries:
- Use `libc_static_fixed.a` instead of `libc.a` in sysroot
- Link `custom_init_tls.o` before `-lc`

## Files
- `arkui_test_standalone/custom_init_tls.S` — assembly TLS init
- `ohos-sysroot-arm32/usr/lib/libc_static_fixed.a` — patched libc
- `arkui_test_standalone/linker_stubs.cpp` — ARM32 virtual thunk stubs (n96, n128, n360 offsets)
- `foundation/arkui/ace_engine/.../overlay_manager.cpp` — lazy init fix for RefPtr<Curve> globals
