---
name: Bitness is a parameter not an assumption
description: 2026-05-14 CR60 decision — keep both aarch64 and ARM32 dalvikvm builds alive; native code uses intptr_t/size_t; driver auto-detects board bitness
type: feedback
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---
When working on Westlake OHOS code, treat target bitness as a per-board parameter, not a fixed architectural choice.

**Why:** DAYU200 OHOS 7.0.0.18 ships 32-bit ARM userspace on aarch64 kernel — no `/system/lib64/`, no `/vendor/lib64/`, all OHOS native libs are 32-bit ELF. Our 64-bit dalvikvm couldn't `dlopen` any OHOS lib, forcing M6 daemon + AF_UNIX cross-arch bridge work that the 32-bit dalvikvm wouldn't need. We pivoted to 32-bit but kept 64-bit alive for boards (Android phones, future 64-bit OHOS ROMs) where userspace matches. See `docs/engine/CR60_BITNESS_PIVOT_DECISION.md`.

**How to apply:**

1. **Native code (JNI, daemons, helpers):** use `intptr_t` / `uintptr_t` / `size_t` for pointer-sized integers. Never `(int)pointer` or `(long)pointer`. The aarch64 `u4` cast widening bug is the canonical anti-example.
2. **CI / build:** keep both `dalvik-port/build-ohos-aarch64/` and `dalvik-port/build-ohos-arm32/` building from the same source. Cheap to maintain (~30 sec each); catches regressions instantly.
3. **No `#ifdef __aarch64__` / `__arm__` branches in shim Java or JNI bridge** unless unavoidable. Prefer runtime-detected behavior. If absolutely needed, document why in the surrounding comment.
4. **Driver scripts (`scripts/run-ohos-test.sh`):** detect target bitness via `hdc shell getconf LONG_BIT` and select the matching binary automatically. Allow `--arch arm32` / `--arch aarch64` to override for testing.
5. **Existing artifacts to know about:**
   - `dalvik-port/build-ohos-arm32/dalvikvm` — 32-bit static ELF (may be stale; rebuild as needed)
   - `dalvik-port/build-ohos-aarch64/dalvikvm` — 64-bit static ELF (current primary)
   - `dalvik-port/ohos-sysroot-arm32/usr/lib/libc_static_fixed.a` — patched musl ARM32 with `dynlink.o` rename + custom TLS init (from Phase 1 ArkUI work)
   - `dalvik-port/build-ohos.sh` — supports both arches via flags
6. **Macro-shim contract is unchanged** on the Java side. JNI / native code is exempt from the contract (existing rule). Both 32-bit and 64-bit JNI bridges must compile from the same C++ source.

**Anti-pattern (avoid):**

- Picking a bitness "because that's what we've been using." Inertia is how we got into the M6-daemon-for-cross-arch-IPC situation. Always re-verify against the deployment target.
- Deleting one arch's build artifacts to "clean up." Both stay.
- Hard-coding `/system/lib64` or `/system/lib` in any script — use the bitness-detected path.

**Reversibility:** if a future board ships 64-bit OHOS userspace, switching back is ~2-4 days of revalidation, mostly Java/dex (which is arch-neutral). The 32-bit pivot is additive, not a one-way door.
