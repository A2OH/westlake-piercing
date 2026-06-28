---
name: Westlake VM renders on phone
description: MockDonalds app renders via our own ART11 on Mate 20 Pro - full pipeline validated 2026-03-26
type: project
---

Westlake VM end-to-end pipeline WORKING on Mate 20 Pro as of 2026-03-26.

**What works:**
- ART11 dalvikvm boots with AOT boot image in <5 seconds
- Boot image: core-oj + core-libart + core-icu4j AOT compiled (speed filter), shim runs in interpreter
- OHBridge 170 methods + 11 Typeface natives
- MiniActivityManager: full Activity lifecycle (create/start/resume)
- View tree rendering: LinearLayout, TextView, ScrollView, ListView
- Canvas rendering: drawText, drawRect, drawLine, setColor, measureText
- PNG output: 480x800 RGBA frame via stb_image_write
- Compose host polls PNG at 5fps and displays it

**Key fixes applied:**
- Boot image version: must use art-universal-build dex2oat (v085), not art-latest (v114)
- Boot image ISA directory: ART expects `arm64/boot.art` subdirectory
- dex-location flags: boot image paths must match runtime classpath paths
- Typeface.<clinit> NPE: getSystemDefaultTypeface returned null DEFAULT during static init
- Character.isDigitImpl: added JNI stubs for Character natives
- MockDonaldsApp: WestlakeActivity reference changed to reflection (class not in app.dex)
- Boot image without shim: generate with only core JARs to avoid dex2oat abort on shim class conflicts

**Why:** Validates the Unity-model architecture — one engine, multiple platforms, apps don't know what's underneath.

**How to apply:** The same dalvikvm + shim + OHBridge stack works on OHOS by swapping the 25 OHBridge C functions.

**60fps achieved (2026-03-27):**
- Shared memory double buffer (3MB mmap'd file) replaces PNG file IPC
- Quicken OAT for shim DEX: pre-resolved fields/methods via dalvik-cache
- Sleep reduced to 1ms, layout cached (skip measure/layout on scroll-only frames)
- 12fps (PNG) → 32fps (shm) → 52fps (sleep+cache) → 61fps (quicken OAT)
- dalvik-cache path: `/data/local/tmp/westlake/dalvik-cache/arm64/data@local@tmp@westlake@aosp-shim.dex@classes.{dex,vdex}`
- Generate quicken OAT: `dex2oat --instruction-set=arm64 --compiler-filter=quicken --boot-image=... --runtime-arg -Xbootclasspath:...`

**Files on phone:** `/data/local/tmp/westlake/` — dalvikvm, core-oj.jar, core-libart.jar, core-icu4j.jar, aosp-shim.dex, app.dex, arm64/boot*.{art,oat,vdex}, dalvik-cache/arm64/*.{dex,vdex}, westlake_shm

**Windows ADB:** `/mnt/c/Users/<user>/Dev/platform-tools/adb.exe` (phone connected via USB to Windows, accessible from WSL)
