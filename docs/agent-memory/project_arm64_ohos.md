---
name: ARM64 OHOS QEMU Status
description: ARM64 OHOS work — ART runtime, boot image works, needs stub libs for native methods
type: project
---

## ARM64 OHOS QEMU (started 2026-03-22)

### Current State
- OHOS ARM64 boots on qemu-system-aarch64 (cortex-a57, 2 cores, 1GB)
- ART runtime deployed at `/data/a2oh/` (dalvikvm 11MB + boot.art/oat + DEX)
- liboh_bridge.so (shared lib, 295KB, 74 JNI methods)
- **Boot image loads successfully** — 4 components in 22ms
- **x86_64 ART works** — Hello World + MockDonalds (partial) run in <1s
- **ARM64 ART stuck at InitNativeMethods** — stub libs deployed, testing

### Key Discovery: InitNativeMethods Hang
- ARM64 dalvikvm hangs at `Runtime::InitNativeMethods()` for 10+ minutes
- Root cause: missing `libicu_jni.so` and `libopenjdk.so` — ART's non-fatal error handling causes infinite loop/spin
- Fix: deployed ARM64 stub .so files (icu_jni_stub.c, openjdk_stub.c)
- Also fixed: `registerNativesOrSkip` undefined symbol in javacore_stub.c and openjdk_stub.c

### x86_64 ART Build (fast testing)
- Location: `$HOME/art-universal-build/build/`
- Has dex2oat, libjavacore.so, libicu_jni.so, libopenjdk.so, libart-compiler.so
- x86_64 boot image at `/tmp/a2ohd/x86_64/` (built from same JARs)
- Binary-patched boot image paths: `/data/a2oh` → `/tmp/a2ohd` (same length)
- JIT compiler missing symbol (non-fatal, runs without JIT)
- MockDonalds starts: OHBridge loaded, MiniServer init OK, MenuActivity fails

### Images
- `/tmp/ohos-arm64-images/` — userdata-art.img (now includes libicu_jni.so, libopenjdk.so)
- Kernel: `$HOME/openharmony-arm64/images/Image` (Linux 5.10 ARM64)
- QEMU: `/tmp/qemu-8.2.2/build/qemu-system-aarch64` (system) + `/tmp/qemu-8.2.2/build-user/qemu-aarch64` (user-mode)

### Repos
- `A2OH/openharmony-arm64` — ARM64 QEMU scripts + images
- `A2OH/westlake` — AOSP shim + OHBridge + dalvik-port

### Key Findings
1. Boot image MUST match bootclasspath count (4-JAR BCP for 4-component image)
2. ART looks for boot.art in `<image_dir>/<arch>/boot.art` (e.g., /data/a2oh/arm64/boot.art)
3. Boot image has dex-locations baked in — must match runtime bootclasspath paths exactly
4. Binary-patching `/data/a2oh` → `/tmp/a2ohd` (same length) works for path remapping
5. QEMU ARM64 emulation is ~100x slower than native — even with boot image + AOT code
6. Static dalvikvm binary's dlopen needs explicit java.library.path or LD_LIBRARY_PATH

**Why:** Need ART running on ARM64 OHOS for P0 (pixels on screen)
**How to apply:** Use x86_64 for fast iteration, deploy to ARM64 QEMU for final integration
