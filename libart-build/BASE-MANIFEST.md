# libart build — base manifest (exact pins)

Everything needed to reproduce the deployed ART runtime. The **patched sources**
and the **build script** are committed here; the bulky inputs (AOSP source tree,
OHOS toolchain, adapter prebuilts, baseline object cache) are **pinned by
provenance**, not committed — per repo convention (see `../ARTIFACT-INVENTORY.txt`).

## Output identity (what a correct build produces)
| | |
|---|---|
| File | `out/libart.so` → deploy to `/system/android/lib/libart.so` |
| md5 | `ba40f173bce3b74a45a696603c75b4fa` |
| size | 11,809,056 bytes |
| OAT version | **230** (`runtime/oat.h` → `kOatVersion {'2','3','0'}`) |
| exports | `JNI_CreateJavaVM` |
| marker strings | `FIX-H-RANGE`, `FIX-I`, `FIX-J2B` present |
| SELinux label on device | `u:object_r:system_lib_file:s0` |

## Base ART source (the unpatched tree these patches apply to)
- **Upstream:** `android.googlesource.com/platform/art`
- **Pinned commit:** `814cc9385f8f8eaba6f4bfd1d723160c2132c76e`
  — *"Snap for 12404440 from 2f8927df2e243a094f875d44982eb5a65d6db021 to 24Q4-release"* (2024-09-23)
- **Release:** Android 15 / **24Q4**, OAT v230
- On-disk mirror used for this build: `$HOME/aosp-art-15`

> ⚠️ `A2OH/art-latest` (`PF-noice-*`) and `A2OH/art-universal` are **separate ART
> patch lines** with a different base + different `class_linker.cc` (0 of these W-fix
> markers). They are **NOT** the base for this libart. The base is pristine
> AOSP `platform/art @ 814cc93`.

## Bulky build inputs (provenance-pinned, not committed)
Referenced by the path vars at the top of `build_libart_pathA.sh`
(`$A`, `$OH`, `$ADAPTER`) and its baseline cache:

1. **AOSP source bundle** (`$A` = `…/libart-32arm-pathA-bundle/aosp`) — extracted at the
   pinned 24Q4 base. Subtrees used: `art/ external/ frameworks/ libnativehelper/ system/`.
   Provides headers + pristine-fallback `.cc` (full `-I` list is in the script's `ART_INC`).
2. **OHOS prebuilts** (`$OH` = `…/libart-32arm-pathA-bundle/oh`):
   - clang **15.0.4** — `prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang++`
   - musl sysroot — `out/rk3568/obj/third_party/musl/usr` (libs in `lib/arm-linux-ohos`)
   - `libclang_rt.builtins.a` (arm-linux-ohos)
   - Target: `arm-linux-ohos`, `-march=armv7-a` (32-bit ARM)
3. **Adapter prebuilts** (`$ADAPTER` = `…/libart-32arm-pathA-bundle/adapter`):
   - `framework/appspawn-x/bionic_compat/{include,src}` — the musl↔bionic shim,
     `libcxx_compat.h`, `libcxx_array_aosp`
   - `out/aosp_lib/*.a` — link libs: `bionic_compat log base cutils utils nativehelper
     sigchain dexfile artbase artpalette vixl lz4 ziparchive elffile nativebridge
     nativeloader profile tinyxml2 unwindstack`
4. **Baseline object cache** — `cache/art/*.o`, **230 objects** from a full ART build of
   the base. The script recompiles **11** patched/affected units and **relinks all 230**.
   A clean rebuild needs this cache, or a full ART build of every translation unit first.

## Key build flags (from `build_libart_pathA.sh`)
- `-std=gnu++17 -O2 -fPIC -fno-rtti`, `--target=arm-linux-ohos -march=armv7-a`, `-D__OHOS__`
- `ART_DEFS`: `IMT_SIZE=43`, `ART_BASE_ADDRESS=0x70000000`, `ART_DEFAULT_GC_TYPE_IS_CMS`,
  `ART_FRAME_SIZE_LIMIT=1736`, `ART_TARGET`, `ART_TARGET_LINUX`, `ANDROID_HOST_MUSL`,
  `ART_ARM32_SUPPRESS_LOCKFREE_ASSERT`, stack-overflow gaps
- Link: `-shared -Wl,-Bsymbolic` against the adapter `aosp_lib` `.a` set

## Patched translation units (what's in `src/`)
| file | Δ lines vs AOSP | fix |
|---|---|---|
| `src/class_linker.cc` | 917 | **W-series vtable fixups** — FIX-VTABLE-A, W9 gate→100000, W22 proxy-LinkMethods skip, perf-logging trim; + Fix I.§5 IMT heap-range guard at `FinalizeIfTable` |
| `src/entrypoint_utils-inl.h` + `src/entrypoints/entrypoint_utils-inl.h` | 17 | **Fix H** — consumer-side IMT heap-range guard (inline header → forces recompile of `interpreter*.cc`, `nterp.cc`, `quick_trampoline_entrypoints.cc`) |
| `src/fault_handler.cc` | 323 | **W15** — name faulting method on unhandled SIGSEGV + NPE-recover probe |
| `src/fault_handler_arm.cc` | (patched) | **W15-NPE-RECOVER** — recover frame → catchable NPE (arm) |
| `src/nterp_helpers.cc` | 44 | **Fix-J2B** — `CanMethodUseNterp` gate (disable nterp for app classloaders) |

`interpreter_common.cc` is **unchanged** vs AOSP (recompiled only to pick up the patched
header) — not committed; the script falls back to the pristine bundle copy.

The reviewable unified **diffs** of these are in `../libart-patches/`.
