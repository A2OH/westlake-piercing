# Bridge deployment to the arm64 board — status (2026-07-08)

## ★ CRITICAL FIX: libc++ ABI namespace (__h vs __n1)
The board's OHOS libs (and libart) use libc++ with the **`std::__h`** inline namespace
(they were built with the OHOS prebuilts clang + `libcxx-ohos`). I had built the bridge
with the 6.1 **SDK/NDK** clang, whose libc++ uses **`std::__n1`**. Every OHOS API that
passes `std::string` (or any std type) has the namespace in its mangled name → the __n1
bridge could NOT resolve against the __h board libs (`symbol not found` at load).

**FIX**: build the bridge with the OHOS toolchain (same as libart):
`OHOS_LLVM/bin/clang++ -nostdinc++ -isystem OHOS_LLVM/include/libcxx-ohos/include/c++/v1`.
Verified: output symbols now `NSt3__h...` matching the board. build_bridge_arm64.sh updated.

## What works now
- Bridge compiles **83/90** with the OHOS toolchain (no regression from the NDK build).
- Re-linked with **35 NEEDED board libs** (pulled to out/board_libs/ from /system/lib64/
  {platformsdk,chipset-sdk,chipset-sdk-sp}) so they auto-load.
- On-board load: **OHOS symbols now RESOLVE** (the __h fix). No more `symbol not found`
  for IPCSkeleton::SetCallingIdentity, DataShareHelper::Creator, etc.
- All base OHOS libs (hilog, ipc, ability_manager, render_service, scene_session, want,
  surface) `dlopen` fine individually on the board.

## Remaining before the bridge fully loads standalone
- Bare `dltest` dlopen SIGSEGVs in a **static-init** calling an unresolved symbol
  (ignore-all left it null). Truly-undefined after 35 NEEDED libs = 356:
  - **skia (11)**: SkColorSpace/SkImageInfo/SkBmpDecoder/SkGifDecoder/SkPngDecoder/
    SkCodecs — OHOS bundles skia inside graphic libs; needs the board's skia lib or stubs.
  - **VM (2)**: JNI_GetCreatedJavaVMs — libart provides at runtime.
  - **~343 OHOS NDK (@1.0-versioned)**: OH_Pasteboard_*, OH_HiTrace_*, GetParameter,
    BIO_new_mem_buf/EVP_sha256/SHA256 (crypto) — need ~13 more OHOS NDK libs added to NEEDED
    (libhitrace_ndk, libpasteboard_ndk, libbegetutil, libcrypto/boringssl, ace_ndk...).
- NOTE: standalone `dltest` is stricter than reality — the real load is by the framework
  INSIDE an appspawn-x-forked app process, where the OHOS runtime + libart are already up
  and the full linker namespace resolves everything. That's the real integration test.

## Next (the real integration)
Deploy libart(22M)+framework(fw/)+appspawn-x+bridge → get appspawn-x running with the
framework preload → framework loads the bridge in-process (real namespace) → app
registration (.app→.apk + bm install 6.1 BMS + entry.hap) → aa start → fork → UI.
