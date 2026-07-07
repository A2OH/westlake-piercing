# aarch64 (arm64) board port — scoping

New dev board `5cdbf6af00000000000000000923012c` is **pure arm64/aarch64** OpenHarmony
3.2 (API 23, kernel 5.15.180). The entire existing westlake adapter stack is arm32
and **cannot execute** here — there is no `ld-musl-arm.so.1` (32-bit ELF loader),
abilist is `arm64-v8a` only, and the native binaries (`/system/bin/sh`,
`/system/bin/appspawn`) are 64-bit ELF. A port = rebuild the stack for aarch64.

## De-risk step 1 — toolchain proven (2026-07-08) ✅

The OH clang cross-compiles aarch64-linux-ohos binaries that run natively on the board.

- Compiler: `openharmony/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang` (OHOS clang 15.0.4)
- Sysroot: `openharmony/out/sdk/obj/third_party/musl/usr` (has aarch64 CRT objects,
  `libc.so`, and `include/aarch64-linux-ohos` + `include/`)
- Flags (mirror the arm32 recipe, swap arch):
  `--target=aarch64-linux-ohos --sysroot=$SR -I$SR/include/aarch64-linux-ohos -I$SR/include -B$SR/lib/aarch64-linux-ohos -L$SR/lib/aarch64-linux-ohos`
- Output ELF: class 64-bit, machine aarch64, interpreter `/lib/ld-musl-aarch64.so.1`
  (exactly matches the board's loader).
- **Ran on board:** `AARCH64_HELLO ok: pid=... uid=0`, rc=0. (source: `derisk/hello.c`)

## Dependency surface for appspawn-x (the next rung)

`appspawn-x` (arm32) NEEDED list, and each dep's aarch64 availability on the board:

| lib | aarch64 on board | notes |
|---|---|---|
| libc.so | ✅ sysroot + board | musl |
| libc++.so | ✅ `/system/lib64/chipset-sdk-sp/` | |
| libhilog.so | ✅ `/system/lib64/chipset-sdk/` | |
| liblog.so | ✅ (chipset-sdk family) | |
| libipc_single.z.so | ✅ `/system/lib64/platformsdk/` | |
| libsamgr_proxy.z.so | ✅ (platformsdk) | |
| libbegetutil.z.so | ✅ `/system/lib64/chipset-pub-sdk/` | |
| libselinux.z.so | ✅ (board) | |
| libhap_restorecon.z.so | ✅ `/system/lib64/` | |
| libtokensetproc_shared.z.so | ✅ (board) | |
| libnativehelper.so | ❓ NOT FOUND on board | likely ships with ART / adapter — resolve during ART port |
| **libart.so** | ❌ rebuild | the adapter's patched AOSP ART — **the long pole** |
| **libbionic_compat.so** | ❌ rebuild | small adapter shim |

## Next rungs (not started — full port is deferred)

1. **Rebuild libart for aarch64** — the AOSP ART source (arm32 used a patched
   platform/art @814cc93 with the W-fixups). arm64 is ART's primary arch, so the
   build itself is well-trodden; the risk is re-applying the adapter's patches and
   producing a boot image via `dex2oat --instruction-set=arm64`. THE gating task.
2. Rebuild `libbionic_compat` + resolve `libnativehelper` for aarch64.
3. Relink `appspawn-x` for aarch64 against the board's OH libs + the new libart.
4. Then work up the stack: bridge, boot image, app registration, launch.

Track all arm64-board work on this branch; keep it off `main` (which holds the
proven arm32 board setup).

## De-risk step 2 — arm64 libart.so BUILT + loads on board (2026-07-08) ✅

The long pole is done. Source: `art-universal-build` (AOSP Android 11 ART, cross-
compiled for OHOS aarch64 via `Makefile.ohos-arm64`, OHOS clang 15). All ARM64
objects were already compiled (runtime 217/216, compiler 105, libartbase 27,
libdexfile 17, vixl 23, ... — `make -f Makefile.ohos-arm64 all jni-stubs`).

Linked a shared **libart.so** (recipe: `/home/dspfac/bridge-build-arm64/build_libart_arm64.sh`):
- ELF64 AArch64 DYN, ~13.7 MB, **2694 runtime symbols exported**.
- Only **33 undefined symbols**, all a bounded ENVIRONMENT set (zlib, C++ `_Unwind_*`,
  compiler-rt builtins, libbase fd-passing, `jit_load` from libart-compiler, art TLS).
- **On board:** the loader FULLY relocates it — with board `libz` + an env stub
  preloaded, dlopen gets **past all symbol resolution** into libart's C++ static
  constructors (SIGSEGV there = needs the real runtime env, i.e. the appspawn-x
  integration, not a missing-code problem).

★ NOTE: this is **AOSP Android 11 ART**, a DIFFERENT generation than the arm32
board's patched 24Q4 ART. The arm64 port would standardize on the Android 11 ART
(its own dex2oat/dalvikvm/boot-image), consistently — not port the 24Q4 tree.

### Next rungs
1. Relink `appspawn-x` for aarch64 (against board OH libs + this libart.so) — it
   provides the JavaVM env that lets libart's constructors + Runtime::Init run.
2. `libbionic_compat` arm64; resolve `libnativehelper` (art-universal has nativehelper objs).
3. Boot image via the Android-11 `dex2oat --instruction-set=arm64`; then bridge, app reg, launch.

## De-risk step 3 — arm64 ART runtime RUNS on the board (2026-07-08) ✅

Beyond loading, the arm64 ART runtime *executes* on the board:
- **dalvikvm** (art-universal `build-ohos-arm64/bin/dalvikvm`, static ELF64 AArch64,
  16.6MB) runs on the board — parses args, prints usage, rc=0.
- **dex2oat** (`build-ohos-arm64/bin/dex2oat`, 21MB arm64) runs on the board and
  **initializes the ART runtime + starts boot-image compilation** — it got as far as
  loading `java.lang.String` and checking its field layout (`runtime.cc:663`,
  String.count/String.hash) before aborting. Reaching that point = the runtime
  inits, loads the boot classpath, and starts the compiler.
- Args needed: `ANDROID_ROOT=/system --android-root=/system`, ABSOLUTE `--image=`/`--oat-file=` paths.

### Boot-image blocker = core-jar ↔ ART-version mismatch (tractable, not fundamental)
The abort is a `java.lang.String` layout check: I fed art-universal's **Android 11**
ART the art-latest core jars (android-15 lineage, 5.83MB core-oj) → String field
layout mismatch. The westlake deployed jars (5.53MB) are 24Q4 — also wrong for ART 11.
**Next: obtain/build matching AOSP-11 core jars** (core-oj/core-libart/core-icu4j)
for art-universal's ART, then the on-board dex2oat should complete the arm64 boot
image. After that: run dalvikvm with the image (full runtime init) → then relink
appspawn-x aarch64 to host adapter apps.

Recipes captured: `/home/dspfac/bridge-build-arm64/build_libart_arm64.sh`; on-board
dex2oat cmd above. art-universal-build (AOSP 11) is the arm64 ART base.

## De-risk step 4 — boot classpath frontier: core-jar ↔ ART String-layout mismatch (2026-07-08)

The arm64 ART runs on the board at every level tested, but boot-classpath init is
blocked by ONE consistent root cause: **the available core jars don't match any
arm64 ART build's compiled-in `java.lang.String` layout.** Confirmed 3 ways:
- art-universal (AOSP-11) dex2oat: FATAL `class_linker.cc:660 InitWithoutImage:
  Class mismatch for Ljava/lang/String;. Make sure libcore and art projects match.`
- art-latest (aosp-art-15) dex2oat: mismatch downgraded to warning ("continuing for
  standalone dex2oat") then SIGSEGV in the compile-time class-init (`RunRootClinits:
  reinitializing UnstartedRuntime`).
- art-latest dalvikvm bootless (`-Xint -Ximage:/nonexistent -Xbootclasspath:core-*`):
  runtime inits + verifies classes, then `VerifyError: ... String not instance of
  String` on `java.lang.Runtime.initLibPaths` — the classic two-String-types symptom
  of the same mismatch.

Available core jars on host (NONE match an arm64 ART build):
- 5.83MB `core-oj.jar` = Android-15 lineage (`core-oj-a15.jar`; art-latest/core-jars,
  .../art-boot-image, .../arm64-a15).
- 5.53MB = westlake 24Q4/OAT230 deployed jars (match the arm32 board's 24Q4 ART).
No pre-built AOSP-11 or aosp-art-15 libcore jars exist (`aosp-android-11`,
`aosp-art-15` have the source + soong_ui.bash but no built `core-oj`/`core-libart`).

### The well-defined next task
Build matched libcore jars (`core-oj`, `core-libart`, `core-icu4j`) from the ART's
EXACT AOSP source, then dex2oat should get past String and (a) complete the arm64
boot image, or (b) run bootless interpreter. Options:
- `aosp-art-15` + build its libcore → matches art-latest's arm64 dalvikvm/dex2oat
  (which are the newest, May-4 arm64 build); OR
- `aosp-android-11` libcore (soong `m core-oj core-libart`, or targeted javac+d8 of
  the .bp file lists openjdk_java_files.bp / non_openjdk_java_files.bp) → matches
  art-universal.
Pick ONE ART generation and build its matching libcore. That unblocks the runtime.

Artifacts: arm64 dalvikvm/dex2oat under art-latest/build-ohos-arm64/bin (art-15) and
art-universal-build/build-ohos-arm64/bin (android-11). Board-run recipe (ANDROID_ROOT
=/system, absolute --image paths) captured above.
