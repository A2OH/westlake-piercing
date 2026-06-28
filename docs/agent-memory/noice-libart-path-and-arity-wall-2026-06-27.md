---
name: noice-libart-path-and-arity-wall-2026-06-27
description: New-board noice bring-up — the /system/lib libart-path discovery + the unsolved Function2-arity wall + 42d2d8e8 source-loss
metadata: 
  node_type: memory
  type: project
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

New-board (2026-06-27) noice install effort. Hard-won, non-obvious findings:

**★ LIBART LOADS FROM `/system/lib/libart.so`, NOT `/system/android/lib/`.** The appspawn-x (`dlopen libart.so`) maps `/system/lib/libart.so` (verified via `/proc/<appspawn-x-pid>/maps`). Deploys to `/system/android/lib/libart.so` (what `dep7b_deploy.sh` does) are **silent no-ops** — wasted a long stretch swapping 7b856a2d/ba40f173 there with zero effect. **To change libart, write `/system/lib/libart.so`** (chcon system_lib_file). Framework/boot deploys to `/system/android/framework[/arm]` DO take effect (BOOTCLASSPATH + boot load from there).

**★ The actually-running libart = `42d2d8e8` — most-advanced, SOURCE LOST.** It carries `FIX-VTABLE-A-POSTPASS` + `FIX-VTABLE-A-IFTABLE-PRECISE` + `FIXA-DIAG` + `GETARITY-PROBE` (a prior session debugging the exact Function2-arity bug). NONE of those markers exist in any host source — `libart-pathA-work/src/class_linker.cc` (current + ALL `.pre-*`/`.orig` backups) has only FIX-VTABLE-A RELOC, no POSTPASS/IFTABLE-PRECISE/GETARITY. Not in any transcript either. Binary backed up: host `$HOME/libart-42d2d8e8-DEVICE-RUNNING.so` + device `/data/local/tmp/libart-42d2d8e8.bak`.

**★ noice's wall = an UNSOLVED Function2-arity bug (42d2d8e8 does NOT fully run noice).** On 42d2d8e8 noice runs deep (past SIGBUS, ConnectivityManager NPE, kotlin coroutine classes, Room, Fragment) then dies in a `Dispatchers.IO` worker: `ClassCastException: androidx.room.RoomDatabaseKt$withTransaction$transactionBlock$1 cannot be cast to kotlin.jvm.functions.Function2`. NOT a plain checkcast — it's Kotlin `TypeIntrinsics.beforeCheckcastToFunctionOfArity(obj,2)` throwing a HARDCODED "Function2" message because `getFunctionArity(transactionBlock$1) != 2`. Root: `getArity()` mis-dispatch on the R8 `SuspendLambda → ContinuationImpl → BaseContinuationImpl`(clone-shadow) chain. transactionBlock$1 implements `s7.p` (= R8-renamed Function2) and `s7.p` IS in its iftable, so the cast itself is fine — the arity probe is what fails. The prior session probed it (GETARITY-PROBE) but never solved it; no available libart solves it.

**★ ba40f173 (host's best buildable libart, `libart-pathA-work` → `build_libart_pathA.sh`) is BEHIND 42d2d8e8.** Deploying it to `/system/lib` regresses noice EARLIER: `InflateException` at `androidx.fragment.app.FragmentContainerView` → `IllegalArgumentException: protected java.lang.Object.clone()` — i.e. `FragmentManager.y()` stuck at the clone-shadow slot0. 42d2d8e8 fixes this via POSTPASS (`method=y relocated slot0->64 "general clone-collision multi; any path"`) which ba40f173 lacks.

**Re-derivation scope (user chose "re-derive on ba40f173"):** write POSTPASS + IFTABLE-PRECISE in `libart-pathA-work/src/class_linker.cc` (FIX-VTABLE-A block ~line 8944-9160; main reloc loop only catches `slot < super_vtable_length` name-mismatch) using 42d2d8e8's diagnostic `c2463.stderr` as the behavioral spec — JUST to reach 42d2d8e8's state — THEN solve the novel arity bug. High-risk, many ~15-min build/deploy(/system/lib)/reboot/test cycles. `kLogVtableFixup` (class_linker.cc:8972) is `false` in ba40f173 (perftrim) → flip true to see FIX-VTABLE-A logs.

**Deploys that DID land this session:** framework.jar ConnectivityManager-NPE smali patch (`if-eqz v3` skip getDefaultProxy in `ActivityThread.handleBindApplication`) + regen'd boot at `/system/android/framework[/arm]` (md5 fw cb2b71bb). appspawn-x (10-jar BCP) built from source.

**Catalog** is Java (no Kotlin arity path) → the achievable app; its blocker is BMS/HAP registration, not libart. See [[catalog-ime-search-fix-2026-06-26]].
