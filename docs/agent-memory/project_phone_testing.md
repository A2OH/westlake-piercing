---
name: Phone Testing Status
description: MockDonalds on Android phones — what works, what's broken, exact setup needed
type: project
---

## Phone Testing (2026-03-23)

### What worked on Mate 20 Pro (LYA-L29, Android 10, serial LHS7N18B15001711)
- Earlier in session: MockDonalds menu displayed with fonts, stable, no blinking
- This was BEFORE path changes — dalvikvm wrote to `/data/local/tmp/a2oh/framebuffer.raw`
- Old viewer (SurfaceView-based) read from same path
- The viewer that worked was the FIRST SurfaceView viewer BEFORE all the blinking fixes

### What broke it
1. Changed FB_PATH from `/data/local/tmp/a2oh/framebuffer.raw` to `/sdcard/Android/data/...`
2. Changed viewer from SurfaceView to ImageView
3. The ImageView viewer can read the file but shows white because file is being read mid-write
4. App can't read `/data/local/tmp/` due to SELinux on Android 10

### Root cause of rendering issue
- The `aosp-shim.dex` from the deploy package (3709568 bytes) is the OLD version without classloader fix
- Agent B's fixed version (3709528 bytes) has corrupt DEX header (file_size field wrong)
- Without classloader fix, `MenuActivity` is found by boot classloader but views don't render content
- WITH classloader fix (Mate 9 earlier), full 8-item menu renders

### To fix for next session
1. Agent B must rebuild `aosp-shim.dex` with valid DEX header (file_size must match actual size)
2. OR: fix the viewer to use `cp` from `/data/local/tmp/` to `/sdcard/` before reading
3. The dalvikvm should write to `/data/local/tmp/a2oh/framebuffer.raw` (dalvikvm runs as shell user)
4. The viewer should `cp` the file to its own dir then read it

### Two phones tested
- Huawei Mate 9 (TEV0216C, Android 7) — dalvikvm works, rendering works
- Huawei Mate 20 Pro (LYA-L29, Android 10) — dalvikvm works, rendering worked earlier

### Key files on phone
- `/data/local/tmp/a2oh/dalvikvm` — ARM64 static binary with ohbridge_render.c
- `/data/local/tmp/a2oh/run.sh` — launch script
- `/data/local/tmp/a2oh/framebuffer.raw` — rendered frame (1536000 bytes = 480×800×4)
- `/data/local/tmp/a2oh/DejaVuSans.ttf` — font (copied from /system/fonts/DroidSans.ttf)

### Android NDK
- Downloaded to `/tmp/android-ndk-r21e/`
- Clang at `toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang`
- Need `LD_LIBRARY_PATH=$NDK/lib64` to run

**Why:** Testing Westlake engine on real ARM64 hardware
**How to apply:** Use dalvikvm + framebuffer file approach. Viewer copies file to app-readable location.
