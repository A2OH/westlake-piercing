# libart-build — the ART (JVM) runtime build for the westlake adapter

The OHOS `appspawn-x` adapter runs Android app dex through a **patched ART**
(`libart.so`). This directory is the **buildable form** of that runtime: the
patched sources + the exact build/link recipe that produces the deployed
`libart.so` (`ba40f173`, OAT v230). The reviewable unified diffs live in
[`../libart-patches/`](../libart-patches/); this is what you compile.

## Where it comes from

There is **no single upstream repo** for this libart — it is patched AOSP ART:

- **Base:** pristine AOSP `platform/art` @ `814cc93` (Android 15 / **24Q4**, OAT v230).
- **Patches:** the 6 source files in `src/` (W-series vtable fixups, IMT guards,
  fault-handler NPE-recover, nterp gate).
- **Built for:** `arm-linux-ohos` (32-bit) against the OHOS clang-15 / musl toolchain.

`A2OH/art-latest` and `A2OH/art-universal` are *different* ART patch lines and are
**not** this build — see [`BASE-MANIFEST.md`](BASE-MANIFEST.md).

## Why ART is patched

AOSP apps' class linking, vtable/IMT layout, and fault handling don't survive
unmodified on this adapter. The fixes here are what make catalog + noice link and
run — e.g. the Date Picker hard-crash (W9 vtable gate) and the proxy-LinkMethods
hang (W22) are both in `class_linker.cc`. Background:
[`../CATALOG-REPRODUCE.md`](../CATALOG-REPRODUCE.md),
[`../docs/REPRODUCTION-GUIDE.md`](../docs/REPRODUCTION-GUIDE.md).

## Layout
```
libart-build/
├── BASE-MANIFEST.md          exact pins: base commit, toolchain, bulky inputs, output md5
├── build_libart_pathA.sh     the build + link script (as used; absolute paths inside)
└── src/                      the genuinely-patched translation units
    ├── class_linker.cc            W-series vtable fixups + Fix I.§5 IMT guard
    ├── entrypoint_utils-inl.h     Fix H IMT guard (inline header)
    ├── entrypoints/
    │   └── entrypoint_utils-inl.h (same patched header at the #include path)
    ├── fault_handler.cc           W15 fault naming + NPE-recover
    ├── fault_handler_arm.cc       W15-NPE-RECOVER (arm)
    └── nterp_helpers.cc           Fix-J2B CanMethodUseNterp gate
```

## Rebuild

> ⚠️ **This is an incremental relink, not a from-zero ART build.** It recompiles the
> ~11 patched/affected units and relinks against a **230-object baseline cache** plus
> prebuilt `.a` libs. You need the bundle + cache from `BASE-MANIFEST.md` (bulky,
> provenance-pinned, not in git), or a full ART build of the base first.

1. **Assemble the bundle** per `BASE-MANIFEST.md`:
   - AOSP `platform/art @ 814cc93` (24Q4) + the other AOSP subtrees → `$A`
   - OHOS clang-15.0.4 + musl sysroot → `$OH`
   - adapter `bionic_compat` + `aosp_lib/*.a` → `$ADAPTER`
   - the 230-file baseline `cache/art/*.o`
2. **Place these sources** at `$WORK/src/` (keep the `src/entrypoints/` subdir).
3. **Edit the path vars** at the top of `build_libart_pathA.sh`
   (`WORK`, `OH`, `A`, `ADAPTER`) to your layout.
4. **Build:** `./build_libart_pathA.sh` → `out/libart.so`.
   Expect md5 **`ba40f173…`**, OAT v230, `JNI_CreateJavaVM` exported, and the
   `FIX-H-RANGE / FIX-I / FIX-J2B` marker strings (the script verifies these).
5. **Deploy:**
   ```
   # stage to device, then:
   cp libart.so /system/android/lib/libart.so
   chcon u:object_r:system_lib_file:s0 /system/android/lib/libart.so
   # reboot
   ```
   `/system/android/lib/libart.so` is the **only** path that matters (there is no
   `/system/lib/libart.so` override). libart affects **Android-app processes only** —
   OHOS + hdc boot regardless, so a bad libart is recoverable, not a brick.

## Honesty note

A fully self-contained ART reproduction would mean committing/hosting the multi-GB
AOSP+OHOS bundle and the 230-object cache. Per repo convention those stay
provenance-pinned in `BASE-MANIFEST.md`; the **patches and the exact recipe here are
complete** — given the pinned inputs, the build is deterministic to `ba40f173`.
