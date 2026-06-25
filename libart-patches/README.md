# libart patches (the W-series vtable/proxy fixes + the perf-logging trim)

The adapter runs a **vanilla AOSP-14 ART** (`libart.so`) with a focused set of
source patches to `class_linker.cc`. These are the catalog's load-bearing native
fixes (Date Picker, the proxy-LinkMethods hang) plus a performance trim. libart is
**not** a BCP jar, so changing it does **not** require a boot-image regen — but
libart and the boot image are a dex2oat **pair**: regen the boot whenever you
change libart's class-layout assumptions and deploy them together.

- **Deployed md5: `ba40f173`** at `/system/android/lib/libart.so` (this path
  ONLY — there is no `/system/lib/libart.so`). **OAT version 230.**
- **Source tree (deployed-match):** `$HOME/libart-pathA-work` —
  `src/class_linker.cc` + `build_libart_pathA.sh`. That tree's `out/libart.so` is
  the byte-exact match for the deployed libart. Build is ~2 min (recompiles
  `class_linker.cc` + the IMT cascade, relinks).
- **Recoverable, not a hard brick.** libart only affects Android-app
  (`adapter_child`) processes — OHOS + hdc boot regardless. Back up the prior
  libart before deploying.

> The full `class_linker.cc` is ~543 KB; per this repo's convention
> (small patches committed, large/un-rebuildable binaries recorded by md5 +
> provenance) only the **diffs** are committed here. Apply them to a vanilla
> AOSP-14 `class_linker.cc`, or read them as the authoritative description of each
> fix.

## Files

| File | Base | What it is |
|---|---|---|
| `class_linker.cc.westlake-W-series.diff` | vanilla AOSP-14 `class_linker.cc` (`.orig`) | The cumulative Westlake ART source patch: the W-series vtable-fixup/shadow-routing passes, the W9 gate, the W15 NPE-redirect fault path, the W20 bogus-super_vt guard, and the **W22-PROXY-SKIP** + **W9 gate raise to 100000**. |
| `class_linker.cc.perf-logging-trim.diff` | the pre-trim source (`2813065e`, i.e. `.bak-preperf`) | The performance trim only: gates the FIX-VTABLE-A reloc/summary logging behind `constexpr kLogVtableFixup=false` and comments the 62 `[*_CP]` checkpoint `fprintf(stderr)+fflush` calls. ~124 changed lines. |

## The catalog-relevant fixes (what to look for in the diff)

### Date Picker crash — the W9 gate raise (`> 500` → `> 100000`)
libart's W12G perf optimization SKIPS the W9 virtual-method shadow-routing when
`super_vtable_length > 500`. `MaterialCalendarGridView` (the Date Picker calendar
grid, super_vt **1267**) NEEDS that routing; skipping it mis-routes its vtable →
the first virtual dispatch hits the wrong slot → a hard native crash that logs
nothing. The gate is raised so W9 runs for every realistic class while staying in
place for any pathological super_vt:

```cpp
// class_linker.cc ~line 9316
if (super_vtable_length > 100000 || klass->IsProxyClass()) {   // was: > 500
  ... skip W9 ...
}
```
Verify on device: `W12G-W9-SKIP=0`, `VTA-1-W9 routed=342`; the Date Picker renders
its calendar instead of hard-crashing.

### Proxy-LinkMethods O(n²) hang — W22-PROXY-SKIP
The three vtable-fixup passes are O(num_virtual × super_vt × GetSignature); a
dynamic `Proxy` implementing a big interface explodes there (>10 s in LinkMethods
→ AMS LIFECYCLE timeout kills the catalog). ART-generated proxies already have
correct vtables, so the fixup is skipped for them — `&& !klass->IsProxyClass()`
added to the pass gates (~8956, ~9217) and `|| klass->IsProxyClass()` to the W9
gate (~9316):

```cpp
if (UNLIKELY(klass->GetClassLoader() != nullptr &&
             !class_linker_->IsBootClassLoader(klass->GetClassLoader()) &&
             !klass->IsProxyClass())) {                          // W22-PROXY-SKIP
  ... FIX-VTABLE-A relocation / abstract pass ...
}
```

### Performance — the logging trim (`kLogVtableFixup=false`)
The custom libart was emitting ~43,000 lines of synchronous `fprintf(stderr) +
fflush()` per launch during class loading (1014+ blocking writes). Gating them
(`constexpr kLogVtableFixup = false`) + commenting the `[*_CP]` checkpoints cut
the per-launch child stderr **43,000 → ~800 lines** with zero functional change.
This is the `class_linker.cc.perf-logging-trim.diff`. (Wall-clock gain is modest —
cold start is dominated by the prefork + the fixup work itself, not the logging.)

## Build & deploy

```bash
cd $HOME/libart-pathA-work
# (apply / verify the diffs against src/class_linker.cc)
bash build_libart_pathA.sh          # ~2 min
md5sum out/libart.so                # expect ba40f173 for the demo-ready set
# back up the deployed libart, then deploy to /system/android/lib/libart.so:
#   chcon u:object_r:system_lib_file:s0 /system/android/lib/libart.so ; reboot
```
No boot regen for a libart-only change — but if you change libart's class-layout
assumptions, regen the boot image too and deploy the pair together (see
`CATALOG-REPRODUCE.md` §boot-regen).
