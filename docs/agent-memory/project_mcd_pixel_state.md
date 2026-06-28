---
name: McDonald's on Pixel 7 Pro — Current Blocker Analysis
description: Boot image compatibility matrix between dalvikvm, JARs, and dex2oat versions. Runtime init crashes.
type: project
---

## Pixel 7 Pro (Android 15)

## Three Build Variants
| Build | dalvikvm | libc | Image Version | JARs | Status |
|-------|----------|------|---------------|------|--------|
| arm64-a15 | 17.9MB static | musl | v114 | art-latest (5.8MB core-oj) | ✅ Boots, reaches main() |
| build-ohos-arm64 | 16MB static | musl | v085 | AOSP 11 (5.0MB core-oj) | ❌ Runtime.availableProcessors crash |
| build-bionic-arm64 | 27MB static | bionic | v085 | AOSP 11 (5.0MB core-oj) | ❌ main_thread_group_ null |

## arm64-a15 (BEST so far)
- Reaches WestlakeLauncher.main() in 200ms
- Without boot image: stuck forever in interpreter mode (33 DEX files too slow)
- With boot image: path mismatch `$HOME/art-latest/core-jars/` vs `/data/local/tmp/westlake/`
- Binary-patched v114 boot images: hang after loading (format incompatible with binary patching)
- **Fix needed**: recompile v114 boot images with correct `--dex-location` paths
- v114 dex2oat only exists as ARM64 binary (segfaults on Pixel) or as art-latest x86_64 (produces v114 images that `--compiler-filter=verify` generates .art+.oat but missing classes)

## OHOS build (v085)
- AOSP 11 JARs match perfectly (no class mismatches)
- v085 boot images compile and load
- BUT `Runtime.availableProcessors()` crashes during ConcurrentHashMap static init
- Root cause: musl sysconf/_SC_NPROCESSORS_ONLN works but some deeper class init chain fails

## Bionic build (v085)
- Missing patches from OHOS build (well_known_classes, runtime.cc standalone mode)
- Even with all patches applied, main_thread_group_ is null
- Likely need MORE patches that were applied directly to AOSP source in previous sessions

## What Actually Worked (April 2, Mate 20 Pro)
The previous working setup on Mate 20 Pro used:
- arm64-a15 dalvikvm with art-latest JARs
- Boot images with v114 format compiled from art-latest dex2oat
- adb shell execution (not from host app) for boot image PROT_EXEC access
- The boot images had correct deploy paths because they were compiled on the Mate

## Path Forward Options
1. **Fix art-latest x86_64 dex2oat** to produce v114 boot images with correct paths — needs art-latest source patches
2. **Fix OHOS build** for sysconf/availableProcessors crash — add Runtime.availableProcessors native stub
3. **Fix bionic build** by applying ALL patches systematically from the OHOS build
4. **Run from adb shell** on the phone (boot images work there) — but only works with USB connected
