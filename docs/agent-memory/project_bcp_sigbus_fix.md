---
name: PF-arch-053 — bootclasspath PathClassLoader SIGBUS resolved
description: Historical SIGBUS at 0xfffffffffffffb17 when aosp-shim.dex was on dalvikvm -Xbootclasspath is no longer reproducible; verified 2026-05-12. M4 unblocked.
type: project
originSessionId: pf-arch-053
---

# PF-arch-053 — BCP PathClassLoader SIGBUS Resolution (2026-05-12)

## Status: RESOLVED (verified, regression-tested)

The SIGBUS at `fault_addr=0xfffffffffffffb17` during PathClassLoader
initialization with `-Xbootclasspath:aosp-shim.dex` (and the same
behavior with `framework.jar` on BCP) is no longer reproducible.

**Acceptance test:**
`aosp-libbinder-port/test/bcp-sigbus-repro.sh` — three modes, all PASS:
- baseline (M3 shim on -cp)
- `--bcp-shim` (shim on BCP)
- `--bcp-shim --bcp-framework` (shim + framework.jar/ext.jar/services.jar on BCP)

In each mode HelloBinder reports `PASS`, `listServices()` returns
`manager` + `westlake.test.echo`, `getService` returns a non-null
`NativeBinderProxy`, `dalvikvm exit code: 0`.

## Why it works now (no code change in this commit)

The sentinel `0xfffffffffffffb17` is Westlake's `kPFCutStaleNativeEntry`
marker (`art-latest/patches/runtime/class_linker.cc:169`).  It indicates
a JNI entry point never bound to real code.

Historical mechanism (now broken):
1. Fat aosp-shim.dex (4.8 MB, 3835 classes) had ~3000 classes whose names
   duplicated framework.jar.
2. BCP ordering put shim before framework.jar; duplicate-but-incomplete
   shim class won.
3. Shim native methods stayed un-bound; `EntryPointFromJni` = sentinel.
4. `ClassLinker::LinkCode` stomped to `dlsym_stub` (which returns NULL
   on bionic-static), and dispatch BLR'd through the sentinel → SIGBUS.

What killed the bug (already in tree before this task):
- **Slim-shim work** (2026-05-07): `scripts/framework_duplicates.txt` +
  `scripts/build-shim-dex.sh` strip 1813 duplicate class names from the
  shim.  Slim shim: 1.4 MB / 754 classes.  No name collision on BCP.
- **PF-arch-019** (2026-05-11): `ClassLinker::LinkCode` preserves valid
  existing `EntryPointFromJni` instead of stomping unconditionally.
- **PF-arch-004 + framework_register_stubs.cpp** (2026-05-11): direct
  extern table for `register_android_*` so registration walks succeed
  without dlsym.
- **PF-arch-013** (2026-05-11): 29 VMRuntime stubs + NAR null-guard.

The current task confirmed (via repro on the OnePlus 6 hardware) that
the cumulative effect closes the BCP PathClassLoader SIGBUS.  No
runtime code change was needed.

## Deliverables (this task)

- `aosp-libbinder-port/test/bcp-sigbus-repro.sh` (new) — 3-mode
  regression test, used as the acceptance gate.
- `aosp-libbinder-port/m3-dalvikvm-boot.sh` (modified) — `--bcp-shim`
  and `--bcp-framework` flags.  Default is M3 baseline.
- `art-latest/patches/PF-arch-053-bootclasspath-pathclassloader-fix.patch`
  (new) — documents the resolution + verification artifacts.
- `docs/engine/PF-arch-053-NOTES.md` (new) — self-contained explainer.
- `aosp-libbinder-port/M3_NOTES.md` (modified) — append "now obsolete"
  note for the M3 NOTE 1 BCP warning.

## What this DOESN'T change

- `art-latest/Makefile.bionic-arm64` — not edited (W1-B's concurrent
  scope on `binder_jni_stub.cc`).
- Any source under `art-latest/` proper — not edited.
- `shim/java/` — not edited.
- `aosp-libbinder-port/native/` — not edited.

## Regression contract for future agents

Re-run `bcp-sigbus-repro.sh` after any of:
- Adds to aosp-shim.dex (especially names overlapping framework.jar).
- Edits to `scripts/framework_duplicates.txt` (removing entries
  re-exposes the historical fault).
- Edits to `art-latest/patches/runtime/class_linker.cc::LinkCode`,
  `art_method.cc`, or anything touching `kPFCutStaleNativeEntry`.
- Edits to `art-latest/stubs/ohbridge_stub.c` /
  `framework_register_stubs.cpp` / `binder_jni_stub.cc`.

## M4 implications

M4 (real AOSP IServiceManager/Binder/Parcel on BCP) can proceed.
When the shim's `android.os.{ServiceManager,IServiceManager,
ServiceManagerNative,IBinder,IInterface,Parcel,Binder,RemoteException}`
get re-added to `scripts/framework_duplicates.txt` (so framework.jar's
versions win), the `--bcp-framework` acceptance test must be re-validated.

## Files / paths

- Test:  `$HOME/android-to-openharmony-migration/aosp-libbinder-port/test/bcp-sigbus-repro.sh`
- Boot:  `$HOME/android-to-openharmony-migration/aosp-libbinder-port/m3-dalvikvm-boot.sh`
- Patch: `$HOME/art-latest/patches/PF-arch-053-bootclasspath-pathclassloader-fix.patch`
- Doc:   `$HOME/android-to-openharmony-migration/docs/engine/PF-arch-053-NOTES.md`
- Dalvikvm (unchanged): `$HOME/art-latest/build-bionic-arm64/bin/dalvikvm` (27 MB)
- aosp-shim.dex: `$HOME/android-to-openharmony-migration/aosp-shim.dex` (1.4 MB, 754 classes)
