---
name: ART15 ARM64 bionic build status
description: ART boots on OnePlus 6 ARM64. 6 tests PASS. McdLoader.main() called. System.out null blocks app output. Build system fixed with $(RUNTIME_OBJS) deps.
type: project
---

## Status (2026-04-11 evening)

### What Works (OnePlus 6, ARM64, imageless)
- 6 basic tests PASS (MinimalTest EXIT=42, MathTest, ConstStr, VirtualTest, StrCall, CustomVirtual)
- framework.jar + all 33 MCD DEX files load on boot classpath
- McdLoader.main() found and called
- ConcurrentHashMap works (native objectFieldOffset bypass)
- All InterpreterJni shorty patterns handled (30+ patterns)
- Aggressive clinit failure tolerance (imageless mode)

### Build System
- **CRITICAL**: `link-runtime` target now includes `$(RUNTIME_OBJS)` in deps
- Two-step: `make all` compiles AOSP .o, then `$(RUNTIME_OBJS)` overwrites with patches
- `find patches -name "*.cc" -exec touch {} \;` before `make link-runtime` to force patch recompile
- Assembly .o files from git (with fix_hidden_symbols applied) — do NOT recompile (header issues)
- Boot image approach FAILED: phone's CC GC images incompatible with our CMS runtime

### Remaining Blocker
System.out/err are null → McdLoader's println fails → can't see MCD output
- Root cause: System.<clinit> NPE at addLegacyLocaleSystemProperties (null string)
- Fix: option C (patch locale code to not crash) — not yet attempted
