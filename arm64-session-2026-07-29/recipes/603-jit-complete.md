# §603 — Complete JIT enablement, merged into source (2026-08-10)

Everything needed to build a libart that **compiles, executes and renders** with the JIT on.
Companion patch: `603-jit-complete.patch` (555 lines, 8 files, two trees).

## ⚠️Two trees, and the Makefile is NOT a glob
`Makefile.ohos-arm64` uses an **explicit 25-entry patch list**, not `patches/**.cc`. Anything not on
that list is compiled from `aosp-art-15/`. Consequences that cost real time this session:
- `art-latest/patches/runtime/common_throws.cc` is **DEAD** — in neither Makefile. The live file is
  `aosp-art-15/runtime/common_throws.cc`. Editing the `patches/` copy changes nothing.
- Headers always come from `aosp-art-15/`, so `compiler_options.h` must be edited there.
- ★**Always byte-verify a marker string in `out/libart.so` after editing** (`python3 -c "print(b'MARKER'
  in open('out/libart.so','rb').read())"`). Two rebuilds this session silently contained nothing.
- ★**No header dependency tracking.** After editing a `.h`, `make` reports `231/231` and rebuilds
  nothing. Delete the dependent `.o` by hand (`build-ohos-arm64/compiler/driver/compiler_options.o`).

## The changes

### Part A — `art-latest/patches/`
| § | file | change | why |
|---|---|---|---|
| §601 | `runtime/art_method.cc` | `force_interpreter_path = !has_jit_code` instead of unconditional `true` | gate 2: `ArtMethod::Invoke` interpreted every non-native method, so opening only gate 1 measured *exactly* interpreter speed |
| §601 | `runtime/interpreter/interpreter_common.cc` | bridge no longer force-interprets a method that has live JIT code | gate 1: `Execute → ToCompiledCodeBridge → ToInterpreterBridge → Execute` was infinite recursion (the old StackOverflowError) |
| §601 | `runtime/runtime.cc` | `implicit_suspend_checks_ = false` on arm64 | this port never installs the suspend-check page, so the implicit check faulted at address 0 on **every loop back-edge** (~7 200 SIGSEGV/s) |
| §603f | `runtime/interpreter/interpreter_common.cc` | `PFCutDeclaringClassPlausible()` replaces the bare null test at 8 sites | §436 launch lottery: `declaring_class_ == 5` passes `!= nullptr`, then `DescriptorEquals` faults at `0x45`. ⚠️**INCOMPLETE** — see Open below |
| **§603h** | `runtime/class_linker.cc` | in `CheckSystemClass`, after repointing the String class root, also repoint `String[]`/`Object[]`/`Class[]` whose `GetComponentType()` is the retired class | **the render fix** — see below |
| §602/§603c | `runtime/oat/image.cc` | `kImageVersion` back to stock **118** | the v114 downgrade only accommodated a stale April dex2oat |
| §603 | `runtime/runtime.cc` | `SetOnlyUseTrustedOatFiles()` gated on `WESTLAKE_TRUST_ALL_OAT` | AOT only: `-Xzygote` forces the trusted-oat policy, which CHECK-aborts on a boot oat under `/data` |
| §603d | `stubs/link_stubs.cc` | define `art::interpreter::g_westlake_infl_gate` | **host dex2oat only**; the host Makefile patches `thread.cc` (which references it) but not `interpreter_common.cc` (which defines it), and `--unresolved-symbols=ignore-all` hid it until run time |

### Part B — `aosp-art-15/`
| § | file | change | why |
|---|---|---|---|
| **§603i** | `compiler/driver/compiler_options.h` | `kDefaultGenerateMiniDebugInfo = true → false` | with the JIT on, mini-debug-info accumulates and the repack XZ-compresses it, which fails and CHECK-aborts the app mid-use: `xz_utils.cc:90 Check failed: res == 0`. Nothing here consumes it |
| §603f/g | `runtime/common_throws.cc` | ASE-DIAG in `ThrowArrayStoreException` (env-gated `WESTLAKE_ASE_DIAG`, capped) | diagnostic that found §603h: dumps class-pointer identity + Java stack |

## §603h — the one that mattered
With the JIT on, the app threw an *impossible* `ArrayStoreException: java.lang.String cannot be stored
in an array of type java.lang.String[]`, killing the SVG parse and blanking the sound-library artwork.
Root cause: **two `java.lang.String` Class objects.**
```
element  : 0x1400c800  status=15 (kVisiblyInitialized)   <- the live String
component: 0x10780     status=1  (kRetired)              <- what String[].GetComponentType() returns
```
`mirror::String::ClassSize()` (from aosp-art-15) predicts classSize **784**; this port's `core-oj.jar`
String needs **808** (131 methods, 7 static fields), so `LinkClass` clones it and retires the original.
`FixupTemporaryDeclaringClass` repoints only fields'/methods' declaring class and `UpdateClass` only the
class table — **nothing repoints array classes' `component_type_`**, which `InitWithoutImage:1103` bound
to the pre-clone object. The array-store check was correct; its inputs were stale.
★Latent all along: interpreted `System.arraycopy` uses the **PFCUT intrinsic**, which skips the element
check. Only a JIT-compiled caller reaches ART's real arraycopy. **The JIT exposed it, it did not cause it.**

## Verified
```
                              screenshot  ArrayStoreException  INITCHILD-FAIL
  JIT on, before §603h         113 830 B          3                  2
  JIT on, after  §603h         125 132 B          0                  0
  JIT off control              124 835 B          0                  0
```
Artwork visually confirmed present with the JIT on (`.shots/shot_FIXJIT2.jpeg`).
§603i verified separately: a 5-tab walk that previously SIGABRTed now survives (ALIVE ×5).

## Open
1. **§436 guard is incomplete** — 1 launch in 2 still fails with `addr=0x45` in `interpreter::DoCall`
   via `INVOKE_INTERFACE`. The 8 sites converted use the `X->GetDeclaringClass() != nullptr` idiom; the
   faulting deref uses another. Cross-check what the §551 code cave guards at `0xa89794`.
2. **Tab navigation does not repaint on the source build** — screen freezes after one navigation.
   ★NOT a JIT bug: reproduced identically with the JIT **off** on the same build. It is a
   source-vs-binary-patch gap (the source build lacks the binary-only stack: §589/§591 PFCUT perf,
   §551/§593, etc.).
3. Source build still lacks §589/§591, so app-level perf is not comparable to the deployed §593.
