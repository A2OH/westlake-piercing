---
name: arkui-headless-testing
description: Headless ArkUI unit test setup - standalone CMake build of ace_engine tests running natively on x86 host
type: project
---

ArkUI headless unit testing is set up via a standalone CMake project at `arkui_test_standalone/`.

**Why:** The OHOS build system has ~48 subsystem dependencies for ace_engine, making it impractical to build tests through the normal product config. This standalone approach bypasses the GN build entirely.

**How to apply:**
- Three Claude Code skills are available: `/arkui-test-setup`, `/arkui-test-run`, `/arkui-test-add`
- Build output goes to `/tmp/arkui-test-build/` (volatile across reboots)
- Skia headers are stubbed, not real - only type declarations, no implementations
- `linker_stubs.cpp` provides empty implementations for ~138 symbols from unused patterns
- `ENABLE_ROSEN_BACKEND` must NOT be defined (pulls in v1 Skia code paths)
- Generated theme code must be regenerated after `/tmp/` is cleaned
- The button_test_ng has 26 tests that run in ~3ms
- Detailed guide at `arkui_test_standalone/GUIDE.md`
