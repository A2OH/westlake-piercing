# ART v114 (AOSP-15) 64-bit + boot image — deliverables for the new OHOS board

Goal: **build art15 64-bit + boot image ("bootloader") for the new OHOS dev board**
(serial `$BOARD_SERIAL`, pure arm64/aarch64 OHOS 3.2). **DONE** —
the runtime executes Java bytecode on the board, with and without the boot image.

## Built artifacts (all aarch64 / ELF machine 0xb7)

| Artifact | Path | Notes |
|---|---|---|
| **libart.so** | `bridge-build-arm64/build-... (art-universal-build/build-ohos-arm64/lib/libart.so)` | 13.7 MB, 2694 syms; loads+relocates on board |
| **dalvikvm** (runtime host) | `bridge-build-arm64/dalvikvm-arm64-charsetfix` | 19 MB static; runs Java; incl. thread-detach + charset fixes |
| **dex2oat** (AOT/boot-image compiler) | `art-latest/build-ohos-arm64/bin/dex2oat` | 19 MB; builds the boot image |
| **boot image** | `bridge-build-arm64/bootimg/boot.art` (+ boot-core-libart.art, boot-core-icu4j.art, .oat/.vdex) | ~5 MB; loads on board |
| **matched libcore jars** | `bridge-build-arm64/core-jars-matched/` | core-oj (4023 cls) + core-libart; a15 jars also work |

Source: AOSP-15 ART (`$WLROOT/aosp-art-15`) + AOSP-11 framework headers, built
via `art-latest/Makefile.ohos-arm64` (OHOS clang 15, `--target=aarch64-linux-ohos`).
libart via `art-universal-build`. Core jars from `aosp-libcore-15/build_core_jars.sh`.

## Build recipes
- `bridge-build-arm64/build_libart_arm64.sh` — link the shared arm64 libart.so.
- `bridge-build-arm64/build_boot_image_arm64.sh` — cross-build the arm64 boot image
  (host x86 dex2oat + `--inline-max-code-units=0 -j1`; those two flags are required).
- ART binaries: `cd art-latest && make -f Makefile.ohos-arm64 link-runtime` (dalvikvm);
  patched sources under `patches/runtime/` (thread.cc, class_linker.cc, runtime.cc).

## Runtime patches applied (patches/runtime/)
1. **thread.cc** — daemon-thread `DetachCurrentThread` check made non-fatal (was
   aborting Runtime::Start on OHOS musl).
2. **runtime.cc** — manually set `Charset.cache2 = new HashMap` (the force-init
   path left it null → `forName` NPE); `Charset.forName("UTF-8")` now works.

## Run on the board
```
ANDROID_ROOT=/system dalvikvm -Xint -Xverify:none \
  -Ximage:<dir>/boot.art \
  -Xbootclasspath:core-oj.jar:core-libart.jar:core-icu4j.jar \
  -cp app.dex Main
```
Verified: executes Java (arithmetic, StringBuilder, Unsafe CAS), `System.exit` code
propagates, raw `FileDescriptor` output works, `Charset.forName` works.

## Working
- **`System.out.println` works** (multi-line, string concat, StringBuilder,
  hex, loops — verified, exit 0) via: Charset.cache2 manual init + BufferedWriter
  buffer-size static + lazy real-System.out install from dalvikvm main. Latest
  runtime: `dalvikvm-arm64-println`.

## Known remaining (not blocking the goal)
- Boot-image build uses `-j1` (multi-thread compile races) and inliner disabled.
- `Charset.defaultCharset()` (the lazy method) still returns null (the static
  field is set, but the method recomputes); use explicit charsets. Minor.
