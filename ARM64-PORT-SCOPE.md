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
