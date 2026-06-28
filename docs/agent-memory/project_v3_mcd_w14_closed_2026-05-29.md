---
name: project_v3_mcd_w14_closed_2026-05-29
description: V3 McD 2026-05-29 LATE — W14 (bionic-musl/Realm) CLOSED via LD_PRELOAD shim; McD reaches NewRelic init (10k classes); new wall W15 (clobbered-sp fatal NPE in compiled boot code) + W16 (network EPERM)
metadata: 
  node_type: memory
  type: project
  originSessionId: 422676c7-2c64-4e1a-8602-735c79264cef
---

# V3 McD 2026-05-29 LATE — W14 CLOSED, McD into NewRelic init, W15+W16 surfaced

Continues [[project_v3_mcd_w12h_2026-05-29]]. Goal doc: `docs/engine/V3-GOAL.md` (§3/§4 updated).

## W14 CLOSED — bionic↔musl ABI for bundled JNI libs (LIGHT fix, HW-clean)
- **Wall:** `librealm-jni.so` reloc fails `symbol not found s=__errno use_vna_hash=1` → `System.loadLibrary` → relinker `MissingLibraryException`. The `.so` IS present+extracted; OHOS musl can't satisfy bionic's `@LIBC`-versioned libc symbols.
- **Fix:** `libw14supp.so` — 12 bionic→musl thunks (`__errno`→`__errno_location`, `__assert2`, `__android_log_print/write`, `__sF[3*256]`, `_ctype_`, `ALooper_*` stubs, `dl_unwind_find_exidx` stub). Built with `clang++ --target=arm-linux-ohos -x c` (C, not C++ — avoids name mangling) against OH musl sysroot. `LD_PRELOAD`ed into appspawn-x via `/data/local/tmp/start_asx.sh` (`export LD_PRELOAD=/system/android/lib/libw14supp.so`). COW-safe (parent-load, Fix-A RESOLVE archetype) → inherited by forked McD child where librealm is dlopen'd.
- **KEY LESSON: unversioned defs satisfy `@LIBC` versioned needs** under OHOS musl ld.so — NO ELF version script needed (the `LIBC{global:...}` script feared in the goal was unnecessary). librealm loaded + all Realm classes linked cleanly.
- **NO boot regen** (it's a .so + start_asx.sh edit, not a BCP jar) → much lighter than BCP cycles. HW gate passed. `libw14supp.so` md5 `9ee48b46`.
- Engine-clean: the thunk set serves ALL bundled JNI libs (librealmc, Forter libba4e), not McD-specific.

## McD progress after W14
- 10,108 classes linked (W12F-LINKMETHODS), past Realm, deep into **NewRelic agent init** (gson `TypeAdapters$1..$17`, `ReflectiveTypeAdapterFactory`, `JsonAdapterAnnotationTypeAdapterFactory`).
- Then FATAL crash (see W15). McD window only ever the splash/starting placeholder (`startingWindow58`), never real content.

## W15 (NEW, ACTIVE) — fatal null-deref in compiled boot code, fatal-not-catchable
- SIGSEGV(SEGV_MAPERR)@0 on McD main thread at **`boot-framework.oat+0x9f9556`** (caller `+0xb4e1e9`), r1=r2=0 (null-field-access). Deterministic (same offset every run).
- libart W15-PROBE (fault_handler.cc) confirms: **`inGen=1`** (IS generated/boot code) — so NOT an unregistered-range issue. It is **fatal rather than a catchable NPE because the frame is clobbered (`sp[0]==0`)** → `NullPointerHandler::IsValidMethod(*sp)` rejects it → ART can't synthesize the NPE → propagates to OHOS Dfx as fatal.
- Hypothesis on clobbered sp[0]: crash is likely in a runtime stub/reflection-entrypoint in boot.oat (gson `ReflectiveTypeAdapterFactory` is reflection-heavy) where the ArtMethod* isn't at sp[0] by convention — OR an [build-host]-dex2oat vs my-libart frame-convention edge.
- **Method name still UNRESOLVED:** no vdex027-matched oatdump (local oatdump truncated/corrupt; oatdump-local rejects vdex027; no host ART libs to build one; [build-host] key auth rejected + sshpass unavailable). Runtime probe scan being refined (range-filter candidates to boot image [0x70000000,0x73000000) + SafeCopy-validate) to name it without oatdump.

## W16 (network EPERM) — near-term wall, currently CAUGHT
- `android_getaddrinfo failed: EPERM` → `SecurityException: missing INTERNET permission`. appspawn-x-forked child not in the `inet`/network gid (AID 3003 equiv). Phrase SDK + NewRelic swallow it for now, but McD needs real network. Fix = grant child the network gids at fork (appspawn-x child cred setup) — engine-level.

## Substrate state (end of session)
- **Device libart = W15-PROBE build (md5 changes per probe rev; HW-clean).** Production-clean target preserved at `/tmp/w15/libart.CLEAN-bb7a2f97.so` (= `bb7a2f97`, W12-H, no probe). **Restore bb7a2f97 before declaring any production state** — the probe is diagnostic-only.
- framework `f8e878a4` (W13), boot regen from W13, `libw14supp.so` LD_PRELOAD live, start_asx.sh has the LD_PRELOAD line (backup at `start_asx.sh.pre-w14`).
- libart probe pipeline: `fault_handler.cc` added to `build_libart_pathA.sh` compile list; W15-PROBE writes via async-safe `write(2)`.
