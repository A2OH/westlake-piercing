---
name: bionic-musl-class2-abi-detection
description: How to detect bionic↔musl ABI conflicts in a prebuilt Android .so (the Class-2 struct-layout problem); verified arm32 opaque-type size table + header locations on this box
metadata: 
  node_type: memory
  type: reference
  originSessionId: 55503332-f6c7-4b84-940d-eddd4f4fec78
---

For running PREBUILT bionic native libs (NDK `.so`, e.g. Unity `libunity.so`) on OHOS musl, the bionic→musl gaps split into classes. The `libbionic_compat` link-time bridge only covers OUR recompiled libs — see [[unity-run-attempt-plan]] / [[unity-apk-reachability-probe]] for the Unity wall this explains.

**CURRENT consolidated doc: `docs/engine/V3-BIONIC-MUSL-ANALYSIS-2026-06-24.md`** (supersedes the 05-24/05-27 docs; grounds Catalog=SOLVED / Unity=OPEN + the full Class-0/1/2 detection method + verified arm32 size table + no-board static gate). **Older corpus docs:** `docs/engine/V3-BIONIC-MUSL-ANALYSIS-2026-05-24.md` ([build-host] in-process-shim vs [operator] chroot+IPC peer review — = my recompile-to-musl vs real-bionic-namespace options; [build-host] ~1µs/call, [operator] ~288µs/call), `docs/engine/V3-NDK-BIONIC-ABI-2026-05-27.md` (L1 bionic-libc 2033 syms / L2 NDK higher-layer 2486 syms across 21 .so / L3 runtime behaviors; maps to my Class-1=L1, "framework syms"=L2, Class-2=L3), `docs/engine/V3-BIONIC-COUNT-2026-05-27.md`. Docs call pthread_mutex_t size "the single biggest bionic-musl ABI hazard" + "the only L3 hazard that has bitten in production" but say it's "mostly a framework problem, apps use pthread_mutex_init" — **Unity is the app-side exception (native engine w/ file-scope/global locks).** Doc imprecision: says musl pthread_mutex_t ≈40B (that's aarch64); on rk3568 arm32 I measured 24B. Doc's "dynamic init=OK" holds only for RECOMPILED libs; prebuilt bionic .so corrupts even w/ pthread_mutex_init (sizeof frozen at 4 at app's bionic compile time).

**Analysis surface:** the bionic→musl conflict lives in NATIVE code, NOT the DEX. DEX (Dalvik bytecode, ~126KB husk for a 17MB Unity engine) only shows the Java↔native boundary (`nativeXxx` methods, `loadLibrary`) + framework-API use — useful for JNI/jar shim coverage, BLIND to libc ABI. The ELF `.so` (bundled in the APK at `lib/<abi>/*.so`, real ELF32/ARM/DYN) is the surface: `nm -D -u lib.so` = authoritative libc UND set. libunity = 370 UND total (small ext surface for 17MB = "many internal, few external"), of which 46 are layout-sensitive (Class-2). Blind spots even in ELF: `dlopen/dlsym` (runtime string resolution), inline `svc` syscalls (Go/anti-cheat/static-libc — invisible to DEX AND .dynsym).

**Three classes:** Class-0 = present + ABI-identical (malloc/memcpy/math) → safe. Class-1 = bionic-only names (`__system_property_*`, `pthread_cond_timedwait_relative_np`, `__errno`, `ANativeWindow_*`, `__android_log_*`) → enumerable + shimmable (forward to musl/OHOS). **Class-2 = same name in both libcs, but operates on an OPAQUE struct whose size/layout/encoding differs** → NOT fixable by a name-mapping shim (the layout is frozen into the prebuilt binary's own data).

**VERIFIED arm32 (lp32) opaque-type size table** — both columns measured from headers on THIS box:
- bionic = NDK 25: `$HOME/android-sdk/ndk/25.2.9519653/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/bits/pthread_types.h` (+ signal_types.h, setjmp.h). Also AOSP src at `/mnt/wslg/distro$HOME/aosp-android-11/bionic/`.
- musl = OHOS sysroot: `$HOME/openharmony/out/rk3568/obj/third_party/musl/usr/include/arm-linux-ohos/bits/alltypes.h`

| type | bionic | musl | verdict |
|---|---|---|---|
| pthread_mutex_t | int32[1]=**4** | int[6]=**24** | size 6× — **DEADLY (the Unity static-init deadlock)** |
| pthread_cond_t | int32[1]=**4** | int[12]=**48** | size 12× — DEADLY |
| sigset_t | **4** (sigset64=8) | ulong[32]=**128** | size 32× — DEADLY (sigaction/sigprocmask) |
| pthread_rwlock_t | int32[10]=**40** | int[8]=**32** | size mismatch |
| pthread_barrier_t | int32[8]=**32** | int[5]=**20** | size mismatch |
| pthread_attr_t | **24** | int[9]=**36** | size mismatch (pthread_create) |
| jmp_buf | long[64]=**256** | differs | size mismatch (setjmp/longjmp) |
| pthread_once_t | int=**4** | int=**4** | SAME size, **different sentinel encoding** → subtle Class-2 (size check passes) |
| pthread_key_t | int=4 | int=4 | compatible |

Two Class-2 flavors: (1) **size mismatch** → bionic binary reserved its smaller size; musl writes its larger size → smashes adjacent fields + futex word at wrong offset → corruption+hang. Statically detectable (symbol→type→size-diff). (2) **same-size diff-encoding** (pthread_once_t) → no corruption but state misread → needs runtime/semantic check.

**Static procedure** (needs 2 reference inputs besides the .so): (1) musl exports `M = nm -D --defined-only <OHOS sysroot>/lib/arm-linux-ohos/libc.so` (1901 syms). (2) the size table. Then: `U = nm -D -u lib.so` UND (libunity=370). **Class-1 = comm -23 U M** (U\M, musl doesn't export) — but this MIXES framework syms (EGL*/ANativeWindow_*/ALooper_*/ASensor*/__android_log_*/inflate* → provided by adapter's libEGL/libandroid/liblog/libz, NOT a libc problem) vs TRUE bionic-libc Class-1 (libunity: `__errno`→musl `__errno_location`, `__sF`, `__assert2`, `__system_property_*`, `pthread_cond_timedwait_relative_np`); split them. **Class-2 = (U∩M) ∩ size-table-mismatch** → flag layout-sensitive syms that link silently (pthread_mutex/cond/attr/once, sem_*, setjmp/longjmp, sigaction). **ELF verdict confidence differs: Class-1 = DEFINITIVE (in musl exports or not — binary fact); Class-2 = CANDIDATE ONLY** (necessary not sufficient — `pthread_mutex_lock` import looks identical whether the mutex is a bionic-sized embedded member→deadlock or not; proving the embedding needs decompile/dataflow, proving the hit needs runtime). Static rules problems IN; only runtime rules them OUT.

**Can Class-2 be told WITHOUT running? Substantially YES — static depth ladder:** T0 import×table (suspect list). T1 reloc anchors: `readelf -r` shows `R_ARM_JUMP_SLOT pthread_mutex_lock@LIBC` etc. = proven static bind to musl (the @LIBC ver musl ignores); 53k R_ARM_RELATIVE in libunity = globals where bionic-sized locks embed; `PTHREAD_MUTEX_INITIALIZER` global = 4 zero bytes, NO init call → invisible to runtime init-hook but VISIBLE statically (static beats dynamic here). **T2 GOLD (when symbols/DWARF exist — common for CUSTOMER NDK debug builds, NOT stripped release): `abidiff` (libabigail) diffs the .so's compiled type layouts vs musl libc → every Class-2 type w/ exact offset/size delta, fully automated, no run.** libunity is STRIPPED (.debug=0) so T2 N/A for it. T3 disasm xref (Ghidra/IDA; system objdump can't even decode ARM here = "architecture UNKNOWN!") = trace each Class-2 call's r0 ptr to its .bss-global/malloc(N)/stack-frame size. **Static LIMIT (needs run or namespace-bionic): indirect/data-dependent ptr origins; same-size-diff-encoding subtype (`pthread_once_t` 4==4 — libunity imports pthread_once so this is live); reachability.** Buildable as a no-board portability gate: definitive for Class-1 + Class-2-via-DWARF, flags residue. **Root-cause refinement: libunity does NOT import `__cxa_guard_acquire` (only `__cxa_atexit`) → its static-init deadlock is its OWN explicit pthread_once/pthread_mutex in a global ctor, NOT the C++ ABI guard.**

**Runtime confirmation (the definitive "tell"):** LD_PRELOAD interpose Class-2 funcs + fingerprints: (a) backtrace shows `__pthread_mutex_timedlock`/`cond_timedwait` on an UNCONTENDED, freshly-init'd object, SINGLE-THREADED, in static init (a real deadlock needs ≥2 threads/self-relock) → ABI. (b) differential: works on bionic, hangs only on musl, same binary → ABI (Unity 2015+2023 both hang). (c) canary: init writes guard past musl's sizeof; if app's adjacent field clobbered → proven undersized. (d) type-field sanity: musl reads bogus mutex kind → wrong offset.

**Fix paths** (can't recompile the .so): focused bionic-layout pthread/sync interposer on raw futex/clone syscalls (kernel ABI stable across both libcs; same hook as the detector); OR linker-namespace + real bionic for the app's lib namespace (ARC++/Houdini/Waydroid model — makes structs bionic again, Class-2 vanishes by construction; cost = JNI/EGL/ANativeWindow boundary must pass only C-ABI handles, never a libc struct).
