# Reproduction Guide — Running Unmodified Android Apps on OpenHarmony via `appspawn-x`

**Goal of this document:** let a fresh engineer/agent reproduce, *from a factory OpenHarmony
DAYU200 / RK3568 board*, the full result this project achieved — the **Material Components
Catalog** (`io.material.catalog`, an unmodified Android APK) running as a first-class OHOS app:
**cold power-on → swipe-unlock → tap the launcher icon → navigate 32 widget categories, open the
Date Picker / dialogs / side sheets, summon the soft keyboard — with no laptop attached.**

This is **self-contained**: the key facts from the project's private engineering memory are inlined
here. It is long; use the table of contents.

> **Status / honesty note.** What works: launcher-icon launch, 32/32 categories navigated to L3 with
> every widget type driven, Date Picker calendar + modal interaction (L5), dialogs/drawers/side-sheets
> compositing, IME keyboard summoned on a plain Text Field, 0 functional crashes across a multi-hour
> sweep. What does **not** fully work (two foundational walls, documented in §8): (1) OHOS synthetic
> input (`uinput`) is not bridged to the Android `InputConnection`, so injected keystrokes/BACK don't
> reach app text fields — a *physical* keyboard is the untested real test; (2) adapter app windows
> render on top but do **not** hold OHOS WMS focus, so some modals/popups composite intermittently and
> the keyboard is torn down on focus-sensitive views (e.g. SearchView). Neither is a per-app bug.

---

## Table of contents
1. [Overview](#1-overview)
2. [Prerequisites](#2-prerequisites)
3. [Architecture](#3-architecture)
4. [From-scratch build](#4-from-scratch-build)
5. [Deploy](#5-deploy)
6. [The fixes](#6-the-fixes)
7. [Demo setup](#7-demo-setup)
8. [★ CRITICAL GOTCHAS](#8--critical-gotchas-read-first)
9. [Verification](#9-verification)
10. [Key paths / artifacts / md5s](#10-key-paths--artifacts--md5s)
11. [Pointers](#11-pointers-evidence--deeper-detail)

---

## 1. Overview

### What this is
Westlake runs unchanged Android APKs on OpenHarmony as **first-class OHOS app processes** — same
memory footprint as on real Android, same process model, **no per-APK patches** (the few app-level
smali tweaks in this project were for cosmetic transition polish, not for basic functionality).

The architecture rests on a single decision: **Zygote-style in-process fork, not container
isolation.** OHOS already ships `appspawn` — a Zygote-equivalent that preloads the OHOS framework once
and `fork()`s per HAP. The project's **`appspawn-x`** is a fork of `appspawn` that instead preloads the
**AOSP-14 framework + ART runtime** and forks per *Android* app. Each forked child is a real OHOS scene
session — indistinguishable to OHOS WindowManager / launcher / MultimodalInput from a native OHOS app —
while simultaneously presenting a normal AOSP environment to the APK. Where AOSP needs to talk to OHOS
(services, surface, input, lifecycle), a thin Java adapter layer + native bridges translate at the
boundary; `LD_PRELOAD`/musl shims handle bionic↔musl ABI differences.

### Achieved end state (the Material Catalog)
- **Launches from the OHOS launcher icon** (correct logo + "Material Catalog" label), cold-boot
  reliable (0% bad-boot after the fontconfig fix, §6.11).
- **All 32 grid categories navigable** L1→L3, every Material widget type driven with a visible result
  (buttons, switches, checkboxes, radios, sliders w/ drag, tabs, chips, badges, FABs, long-press
  multi-select, container-transform morph, progress, text fields).
- **Date Picker** renders its calendar and is modal-interactive to L5 (pick a date → header updates →
  OK dismisses); Time Picker clock loads; dialogs / nav-drawer / side-sheets composite and are
  drivable; the shared-element **container-transform morph animates** frame-by-frame.
- **Soft keyboard appears** on a plain Text Field (the IMM→OHOS-IME bridge); Search focus no longer
  crashes.
- **0 functional failures / 0 tombstones** across a full multi-hour sweep.

### Hardware
- **DAYU200 developer board, SoC RK3568** (ARM Cortex-A55 ×4, 32-bit `arm` userspace for the adapter).
- **DC-powered with a MOCK battery** — `/sys/class/power_supply/` is empty; any "low battery / 11%"
  warning is fake; **reboot freely** (see §8). Set a simulated level with
  `hidumper -s 3302 -a "--capacity 95"`.
- Transport: **hdc** over USB (the OHOS equivalent of adb).

---

## 2. Prerequisites

### Host build environment
The reference host is **WSL2 (Ubuntu) on Windows**, driving the board through a Windows `hdc.exe`.
A native Linux host works too; the WSL-specific quirks (path mangling, large-file drops, a `/tmp/h`
hdc wrapper) are called out where they matter.

| Need | Reference location on the build host | Notes |
|---|---|---|
| OHOS source tree | `$HOME/openharmony` | Provides the OHOS SDK, `restool`, headers. ~100 GB for a full build. |
| OHOS SDK `restool` | `$HOME/openharmony/out/sdk/ohos-sdk/linux/toolchains/restool` (v4.105) | Compiles the launcher-icon `entry.hap` (§6.2). |
| Bridge / AOSP-native source | `$HOME/bridge-build` | Builds `appspawn-x`, the bridges, libhwui, the BCP jars, the boot image. Build scripts in `build/`. |
| libart source (deployed-match) | `$HOME/libart-pathA-work` | `src/class_linker.cc` + `build_libart_pathA.sh`. Its `out/libart.so` is the byte-exact match for the deployed libart. |
| `dex2oat64` (boot image) | `$HOME/tools/dex2oat64` + `$HOME/tools/lib64/{libsigchain.so,libc++.so}` | The host x86_64 dex2oat64 that produces a **deployable** BCP boot image. **Do NOT** use `art-universal-build/.../dex2oat` (SIGSEGVs on BCP jars). Emits **OAT version 230**. |
| `oatdump` (diagnostics only) | `$HOME/tools/oatdump` (v247), `oatdump-local` (v183) | Both SEGV on the device's OAT-230 boot; use the capstone/symtab disasm path instead for live OATs. |
| Java / dexers | `$HOME/miniconda3/bin/javac` (21, always `--release 17`); **r8.jar 8.2.33** at `$HOME/android-sdk/cmdline-tools/latest/lib/r8.jar` | Use **r8.jar D8**, not `build-tools/.../d8` (NPEs on anonymous inner classes). |
| Android SDK platform | `$HOME/android-sdk/platforms/android-34/android.jar` | Compile classpath for adapter/app Java. |
| smali round-trip | `$HOME/apktool.jar` + helper dir `/tmp/fwktools` (Baksmali2 / SmaliAssemble dexlib2 wrappers) | apktool.jar has Baksmali but **not** a smali *assembler* convenience class — the `SmaliAssemble` wrapper IS the assembler. The toolchain is wiped by host reboot; reconstruct `/tmp/fwktools`. |
| hdc transport | `/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe` (Windows) + a `/tmp/h` wrapper filtering WSL `UtilAcceptVsock` noise | hdc.exe mangles WSL abs paths and silently drops large files → stage all sends/recvs through a Windows dir `C:\Users\dspfa\Dev\ohos-tools\`. |

### Deploy bundle
`$HOME/westlake-complete/` is the assembled from-zero deploy set (the `v3-hbc` self-consistent
generation: `appspawn-x` + 56 libs + 14 jars + boot image + configs + scripts), with `MANIFEST.md5`
and `overlay/scripts/DEPLOY_SOP.md` (the **authoritative** deploy procedure). After flashing the OHOS
base image, deploying this overlay gives a working (older-generation) adapter; the current fixes (§6)
are applied on top.

### Factory board
A DAYU200 flashed with the project's OHOS base image (`system.img` + `updater.img`, ~2 GB — a separate
release asset). `/system/android` does **not** exist on a factory board; the deploy creates it.

---

## 3. Architecture

```
init (pid 1)
  ├── appspawn      (OHOS native apps: com.ohos.launcher, com.ohos.settings, …)
  └── appspawn-x    (Android apps: io.material.catalog, …)   ← the fork of appspawn
        ├── Preloads the 10-jar BOOTCLASSPATH + boot image (CoW-shared with all children)
        ├── Initializes ART (libart.so), runs AppSpawnXInit.preload()
        │     (eager class resolution + 8 reflective AOSP-singleton stamps:
        │      ServiceManager.sServiceManager←OHServiceManager, AssetManager.sSystem, …)
        ├── Listens on /dev/unix/socket/AppSpawnX  ("ondemand" = socket-activated)
        └── On launch from OHOS ams:  fork() → child inherits CoW BCP
              specialize uid/sandbox → load APK PathClassLoader
              → ActivityThread / handleBindApplication → Application.onCreate → Activity
```

### Components
| Component | Role | Device path |
|---|---|---|
| **`appspawn-x`** | Zygote-equivalent for Android apps. Preload BCP once, fork per launch. Socket-activated (`ondemand`). | `/system/bin/appspawn-x` |
| **10-jar BOOTCLASSPATH** | The shared framework, dex2oat'd into the boot image. | `/system/android/framework/*.jar` |
| **`libart.so`** | Vanilla AOSP-14 ART + 3 small patches (interpreter gate for non-boot CL) + the W-series vtable/proxy fixes (§6). **OAT version 230.** | `/system/android/lib/libart.so` (this path ONLY) |
| **`liboh_android_runtime.so`** | AOSP runtime/JNI glue. **Dual-path** — must deploy to BOTH paths. | `/system/lib/` **and** `/system/android/lib/` |
| **`libhwui.so`** | AOSP rendering into OHOS graphics buffers (incl. the createHardwareBitmap shim, §6.3). | `/system/android/lib/libhwui.so` |
| **`liboh_adapter_bridge.so`** (the bridge) | Input / window / surface / IME-summon glue (JNI). | `/system/lib/` (+ mirrored to `/system/android/lib/`) |
| **`liboh_ime_helper.so`** | IME helper — `InputMethodController` calls + the `OnTextChangedListener` shim. dlopen'd lazily from the forked app (never the prefork). | `/system/lib/` + `/system/android/lib/` |
| **`libbms` / `libappexecfwk_common`** | OHOS bundle-manager — APK install/registration (incl. the `.app`→`.apk` byte-patch, §6.1). | `/system/lib/` ; `/system/lib/platformsdk/` |
| **boot image** | dex2oat output from the 10 jars: `boot.{art,oat,vdex}` + `boot-<jar>.{art,oat,vdex}` = **30 segments**. | `/system/android/framework/arm/boot*` |
| **catalog APK + `entry.hap`** | The Android app + a minimal OHOS resource HAP supplying the launcher icon/label. | `/data/app/el1/bundle/public/io.material.catalog/{android/base.apk,entry.hap}` |
| **SELinux** | Board boots Enforcing by default; the autonomous demo sets it Permissive (§6.10). | `/system/etc/selinux/config` |

### The 10-jar BOOTCLASSPATH — exact order, and why it matters
**Authoritative order (the current 10-jar / "Scope C" generation, baked into `appspawn-x`'s
`kBootClasspath`):**

```
1. core-oj
2. core-libart
3. core-icu4j
4. okhttp
5. bouncycastle
6. apache-xml
7. adapter-mainline-stubs
8. framework
9. adapter-runtime-bcp        ← Scope C: arb BEFORE ohaf so it shadows ohaf
10. oh-adapter-framework
```

- The last two are HBC additions: **`adapter-runtime-bcp.jar`** (the preload orchestrator
  `AppSpawnXInit` + `AppSchedulerBridge` + the active `PackageInfoBuilder`) and
  **`oh-adapter-framework.jar`** (the ~30 Java adapter bridge classes: activity/window/contentprovider/
  IME, e.g. `IntentWantConverter`, `InputMethodManagerAdapter`, `OhImeBridge`).
- **Why order matters — first-jar-wins.** BCP class resolution at dex2oat time picks the FIRST jar
  containing a class. **Scope C deliberately swapped the last two** (vs the older Scope B order
  `…framework, oh-adapter-framework, adapter-runtime-bcp`) so that `adapter-runtime-bcp` shadows
  `oh-adapter-framework` — this is how `PackageInfoBuilder` (the metaData fix, §6.5) "wins". Patching
  the ohaf copy of a shadowed class does nothing.
- **Two places MUST agree:** the runtime `kBootClasspath` (in `appspawn-x`, source
  `framework/appspawn-x/src/main.cpp`) **and** the dex2oat JARS list (boot-image build). A mismatch
  yields a loadable-but-mislaid boot image that aborts at load with
  `runtime.cc:699 … Class mismatch for L<class>;` (often surfaces as `Class mismatch for Ljava/lang/String;`).
- **★ Build-script caveat:** the bundled `overlay/scripts/gen_boot_image.sh` and
  `build_boot_image.sh` still default to the **9-jar Scope-B** list (`… framework.jar
  oh-adapter-framework.jar`, no `adapter-runtime-bcp`). For the 10-jar generation you **must override
  `--jars`** with the exact 10-jar order above, matching the deployed `appspawn-x` (`3abe3bde`).
  Confirm the live order from the binary: `strings /system/bin/appspawn-x | grep 'framework/.*\.jar'`.

A historical corollary of first-jar-wins: an *earlier* jar carrying an **incomplete** duplicate class
won resolution, left native methods unbound (sentinel `EntryPointFromJni = 0xfffffffffffffb17`), and
dispatch BLR'd through the sentinel → SIGBUS (`project_bcp_sigbus_fix`). First-jar-wins cuts both ways.

---

## 4. From-scratch build

All builds run **locally** (not on any remote/HBC server). Each build script lives in
`$HOME/bridge-build/build/` unless noted.

### 4.1 `appspawn-x`
```bash
cd $HOME/bridge-build
bash build/build_appspawn_x.sh            # → out/adapter/appspawn-x
```
Verify the 10-jar `kBootClasspath` order in `framework/appspawn-x/src/main.cpp` matches §3 before
building. The known-good 10-jar binary is `3abe3bde`.

### 4.2 libart (ART runtime)
```bash
cd $HOME/libart-pathA-work
# edit src/class_linker.cc as needed (the W-series fixes, §6.4 / §6.6)
bash build_libart_pathA.sh                # ~2 min: recompiles class_linker.cc + IMT-cascade, relinks
md5sum out/libart.so                      # new tag
```
`out/libart.so` is the byte-exact match for the deployed libart. **Deploy path is
`/system/android/lib/libart.so` ONLY** (there is no `/system/lib/libart.so`). libart only affects
Android-app (`adapter_child`) processes — OHOS + hdc boot regardless, so a bad libart is **recoverable,
not a hard brick**; back up first.

### 4.3 The bridges + IME helper + libhwui (native .so)
```bash
cd $HOME/bridge-build
# the bridge (input/window/surface/IME-summon) + the IME helper (built separately):
OH_ROOT=$HOME/openharmony ADAPTER_ROOT=$HOME/bridge-build \
  AOSP_ROOT=$HOME/bridge-build/aosp \
  bash build/build_adapter.sh --target=liboh_adapter_bridge.so
#   → out/adapter/{liboh_adapter_bridge.so, liboh_ime_helper.so}
#   (the script excludes oh_ime_helper.cpp from the bridge glob and links the
#    helper separately against libinputmethod_client.z.so — see §6.7 for WHY)

# libhwui (AOSP-side):
bash build/build_aosp_lib.sh --target=libhwui.so          # → out/aosp_lib/libhwui.so
```
**Brick-safety gate for native libs:** before deploying libhwui, diff its undefined symbols
(`nm -D | grep ' U '`) against the deployed known-good — the new lib must introduce **0 new UND**
(same symbol-set, ~1006). The Phase-4 UND whitelist gate can FALSE-FAIL on an unsorted whitelist
(`comm: not in sorted order`); sort it, then trust the real `nm -D` diff. The bridge must be a strict
**superset** of the prior good bridge (all input/scroll/key/drag/control-channel symbols intact).

### 4.4 The BCP jars (adapter Java + smali patches)
Two ways to change a BCP jar:

**(a) Full Java build** (when adding/replacing classes, e.g. the IME `OhImeBridge`):
```bash
javac --release 17 -cp $HOME/android-sdk/platforms/android-34/android.jar:<adapter-classes> \
      -d out/classes <sources>
java -cp $HOME/android-sdk/cmdline-tools/latest/lib/r8.jar com.android.tools.r8.D8 \
      --output out/dex/ --release out/classes        # r8.jar D8, NOT build-tools d8
jar cf out/<jar>.jar -C out/dex classes.dex
```
`build_aosp_fw.sh --target=<jar>` is the one-shot HBC chain that rebuilds a BCP jar **and** triggers
the boot-image regen.

**(b) smali round-trip** (1-method patches — most of §6):
```bash
# disassemble the deployed jar, edit the .smali, reassemble, repack a COPY (keep META-INF):
java -cp $HOME/apktool.jar:/tmp/fwktools Baksmali2 <jar> outdir
#   ...edit outdir/.../Foo.smali (mind .registers/.locals headroom)...
java -cp $HOME/apktool.jar:/tmp/fwktools SmaliAssemble outdir classes.dex 39
zip <jarcopy>.jar classes.dex
# ALWAYS re-baksmali the new jar to confirm the edit landed.
```
- Find which dex/jar **defines** (not merely references) a class:
  `unzip -p <jar> classes.dex | grep -ac '<Class>'` (iterate `classes2.dex`, … for multi-dex). E.g.
  `IntentWantConverter` is *defined* in `oh-adapter-framework.jar` though `adapter-runtime-bcp` only
  references it — patch the definer.

### 4.5 Boot image regeneration
**Required after ANY byte change to ANY BCP jar** — even if the jar list/order is unchanged
(dex2oat bakes cross-jar layout offsets; new bytes → new offsets → load-time `Class mismatch`).

```bash
# Stage all 10 deployed jars (recv from device), swap in only the patched one, regen ALL segments:
LD_LIBRARY_PATH=$HOME/tools/lib64 \
LD_PRELOAD=$HOME/tools/lib64/libsigchain.so \
$HOME/tools/dex2oat64 \
  --android-root=/system --instruction-set=arm \
  <--dex-file=… --dex-location=… for EACH of the 10 jars, IN THE §3 ORDER> \
  --oat-file=boot.oat --image=boot.art \
  --base=0x70000000 \
  --runtime-arg -Xms64m --runtime-arg -Xmx512m \
  --compiler-filter=speed
```
- **`--base=0x70000000`, `--instruction-set=arm`** (32-bit). OAT version **230**.
- Produces **30 files in ~14–16 s**: `boot.{art,oat,vdex}` (3) + `boot-<jar>.{art,oat,vdex}` for 9 of
  the jars (27).
- The repo has a wrapper, e.g. `WORK=/tmp/regen bash docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh`
  (mirror the 10 device jars into `$WORK`, swap, run). Use an **absolute** `WORK` path — a `cd` inside a
  compound command breaks the relative path (rc 127).
- **Consistency rule:** libart and the boot image are a dex2oat **pair** — regen the boot whenever you
  change libart's class-layout assumptions, and deploy them together. Validate brick-safety by
  byte-comparing a regenerated `boot-framework.oat` against the currently-deployed one when only an
  unrelated jar changed — they should match, proving your dex2oat pairs with the deployed libart.

---

## 5. Deploy

**The authoritative procedure is `$HOME/westlake-complete/overlay/scripts/DEPLOY_SOP.md`**
(Chinese; staging rules, backups, ordering, SELinux labels). Summary of the load-bearing rules:

### Global rules (always)
1. **Never `kill <pid>`; never touch `com.ohos.launcher`.** Stop services with
   `begetctl stop_service <name>` or `killall <name>`. (A bare `kill <launcher-pid>` got the pid
   recycled by hdcd → hdc lost → reflash.)
2. **Never `hdc file send <src> /system/...` directly.** Always stage:
   `send → /data/local/tmp/stage/<basename>` → `ls -la` to confirm it is a `-rw-` *file* (not a `drwx`
   dir from the hdc auto-mkdir quirk) → `cp` into `/system`.
3. **Abort immediately** on `connect-key` / timeout / `[Fail]Not a directory` / a `drwx` where a file
   should be.

### Sequence (factory board)
1. **Stage 0 — preflight:** `hdc list targets` non-empty; `/system/android` absent (factory);
   `mount -o remount,rw /`.
2. **Backups:** copy each target's original to `<file>.orig_<date>` *on the device* (device-side
   rollback is sufficient; no host recv needed).
3. **Stop only `foundation` + `render_service`** (`begetctl stop_service`). **Do NOT stop `appspawn`**
   (OH-native appspawn stop breaks the hdc link). The launcher exits when foundation stops.
4. **Push (per `DEPLOY_SOP.md` §3b–3f), staging each file:**
   - OH-service `.so` → `/system/lib/` + `/system/lib/platformsdk/` (incl. the `libbms` symlink).
   - AOSP native `.so` → `/system/android/lib/`; the adapter shims
     (`liboh_android_runtime`, `liboh_hwui_shim`, `liboh_skia_rtti_shim`) **dual-path** to
     `/system/android/lib/` **and** `/system/lib/`.
   - framework jars + ICU + fonts → `/system/android/framework/` (+ `/system/android/etc/`).
   - boot image (30 segments) → `/system/android/framework/arm/`; the patched jar → `/system/android/framework/`.
   - `appspawn-x` → `/system/bin/appspawn-x` (755); bridge/installer libs → `/system/lib/`;
     `appspawn_x.cfg` → `/system/etc/init/`; `appspawn_x_sandbox.json` → `/system/etc/`;
     `ld-musl-namespace-arm.ini` → `/system/etc/`; `file_contexts` → `/system/etc/selinux/targeted/contexts/`.
   - 3 symlinks: `libc_musl.so`→`/lib/ld-musl-arm.so.1`; `libshared_libz.z.so`,
     `libappexecfwk_common.z.so` → their platformsdk targets.
5. **SELinux labels (critical — `cp` inherits the parent-dir label, NOT `file_contexts`):**
   ```bash
   # boot + libs:
   for f in /system/android/framework/arm/boot*.{art,oat,vdex}; do chcon u:object_r:system_lib_file:s0 $f; done
   chcon u:object_r:system_lib_file:s0 /system/android/lib/liboh_*.so /system/lib/liboh_*.so
   find /system/android/lib -exec restorecon {} \;          # toybox restorecon has no -R; use find -exec
   # appspawn-x must relabel to appspawn_exec or the init domain-transition is denied (EACCES):
   restorecon /system/bin/appspawn-x
   # fonts must be system_fonts_file or the app domain can't read them (EACCES → NPE → fork loop):
   chcon u:object_r:system_fonts_file:s0 /system/etc/fonts.xml /system/android/etc/fonts.xml
   ```
   Each boot segment must be `system_lib_file:s0`, else the `appspawn:s0` domain's `flock()` is denied →
   ART `JNI_CreateJavaVM` Phase-2 SIGABRT storm.
6. **Integrity:** for every pushed file, device `md5sum` == host `md5sum`; no stray `drwx` under
   `/system/android/lib/` or `…/arm/`. The big `boot-framework.*` (~51/37/23 MB) are the likeliest to
   be silently truncated — verify them especially.
7. **Reboot + health:** `sync; reboot` (or `hdc target boot`). After reboot:
   `pidof foundation / render_service / com.ohos.launcher / hdcd` all non-empty; `hilog | grep -i 'BMS.*ready'`.
8. **App registration:** see §6.1 — `bm install -p <apk>` (needs the `.app`→`.apk` patch first),
   `bm dump -n <pkg>`, then `entry.hap` for the icon (§6.2).

> **Never deploy with a `--reboot` flag from the script and never add `critical` to `appspawn_x.cfg`**
> (§8). Reboot manually after staging so a bad artifact doesn't bootloop.

---

## 6. The fixes

These are the root-cause fixes that took the catalog from "crashes at init" to "demo-ready." Most are
**universal** (any adapter app), not catalog-specific. A recurring family: *OHOS lacks an Android system
service → `ServiceManager.getService(...)` returns null → an unguarded interface call NPEs → process
death.* The fix is a null-guard, not a shim (subtraction, not addition — §8).

### Summary table
| # | Symptom | Root cause | Fix (artifact → device path) | Boot regen? | Verify |
|---|---|---|---|---|---|
| 6.1 | `bm install -p X.apk` rejected "file is not hap…" | `libappexecfwk_common` allows only `.app`/`.hap` ext (client-side check) | **1 byte @ 0x1a40 `.app`→`.apk`** in `libappexecfwk_common.z.so` → `/system/lib/platformsdk/` + `/system/android/lib/` | No | `bm install` succeeds; `bm dump -n …` shows record |
| 6.2 | Blank launcher icon + pkg-name label | adapter never creates the bundle's `entry.hap`; launcher resolves icon/label via resourceManager (iconId/labelId), not bundleResource.db | minimal **`entry.hap`** via `restool --defined-ids` → `/data/app/el1/bundle/public/<pkg>/entry.hap` | No | logo + "Material Catalog" on launcher |
| 6.3 | Opening any demo Activity → whole app dies (SIGBUS) | `createHardwareBitmap` reads an **uninitialized `ANativeWindow*`** (OH has no AImageReader) → `setupPipelineSurface` derefs garbage | **libhwui** (off-screen OHOS consumer-surface readback) + **bridge** (`oh_imagereader_*` / AImageReader shim) | No | demo activities open; `SIGBUS=0` |
| 6.4 | Date Picker tap → hard crash, no log | libart W12G perf opt skips W9 vtable-routing for `super_vtable_length > 500`; `MaterialCalendarGridView` (super_vt 1267) needs it | **libart** `class_linker.cc` gate `> 500` → `> 100000` → `/system/android/lib/libart.so` | No (libart) | calendar renders; `W12G-W9-SKIP=0` |
| 6.5 | Catalog crashes at `CatalogApplication.onCreate` | `getApplicationInfo(GET_META_DATA).metaData` is **null** → NPE | **adapter-runtime-bcp** `PackageInfoBuilder.buildApplicationInfo` → `metaData = new Bundle()` | **Yes** | Adaptive grid renders (`drew=1`) |
| 6.6 | App hangs ~12 s at UI load (LIFECYCLE timeout) | libart's vtable-fixup passes are O(n²) over a dynamic Proxy's big interface | **libart** add `!klass->IsProxyClass()` to the 3 LinkMethods gates (W22-PROXY-SKIP) | No (libart) | no hang; UI loads |
| 6.7 | First text-field focus → no keyboard | adapter `InputMethodManagerAdapter` was a no-op stub | **bridge** + **`liboh_ime_helper.so`** + **ohaf** `OhImeBridge` (IMM→OHOS InputMethodController) | **Yes (ohaf)** | keyboard appears on Text Field |
| 6.8 | Tapping the Search field → app dies | `ContentResolver.registerContentObserver` → null `IContentService` (OHOS has no "content") → NPE | **framework.jar** `ContentResolver.smali` null-guard (register + unregister) | **Yes** | Search focuses, no crash |
| 6.9 | (early) `handleBindApplication` crash before UI | AOSP code calls `cm.getDefaultProxy()` but `getSystemService(ConnectivityManager)` is null | **framework.jar** `ActivityThread.handleBindApplication` `if-eqz cm` skip-proxy guard | **Yes** | bind completes |
| 6.10 | Hands-off demo: tap does nothing (Enforcing) | catalog `normal_hap` domain denied adapter access | **`/system/etc/selinux/config`** `enforcing` → `permissive` (persistent) | No | board boots `enforce=0`; tap launches |
| 6.11 | ~25% cold boots freeze (LIFECYCLE_TIMEOUT) | `SkFontMgr::RefDefault()` O(n²)-parses the **6.3 MB** `hm_symbol_config_next.json` on first text render | **minimal `hm_symbol_config_next.json`** (empty arrays) → `/system/fonts/` | No | 0/16 bad-boot |
| 6.12 | Morph snaps instead of animating | `ValueAnimator.sDurationScale == 0` (all animations globally disabled) | **catalog APK** inject `ValueAnimator.setDurationScale(1.0f)` in `ContainerTransformConfigurationHelper.configure()` | No (APK) | morph animates frame-by-frame |

Per-fix detail follows for the load-bearing ones.

---

### 6.1 App registration — the `.app`→`.apk` byte-patch + `bm install`
**Symptom.** `bm install -p /data/local/tmp/catalog.apk` → rejected *client-side* ("file is not hap,
hsp…") before reaching libbms.
**Root cause.** `libappexecfwk_common.z.so` `bundle_file_util.cpp::CheckFilePath` has a literal `.app`
in its allowed-extension list at file offset **0x1a40**; `.apk` is not allowed.
**Fix.** One byte: offset `0x1a40`, `0x70` (`'p'`) → `0x6b` (`'k'`) — turns `.app` into `.apk`. Patched
lib md5 **4d2c6399**. Deploy to **both** `/system/lib/platformsdk/` (the copy foundation/BMS loads) and
`/system/android/lib/`; `chcon u:object_r:system_lib_file:s0`; reboot. The deployed `libbms`
(`7d7f508a`) is already APK-capable (`oh_adapter_install_apk_with_manifest`); this byte was the only
gate.
**Then:**
```bash
bm install -p /data/local/tmp/catalog.apk      # "install bundle successfully"
bm dump -n io.material.catalog                 # bundleType 10 (APP_ANDROID), MainActivity + abilities
aa start -a io.material.catalog.main.MainActivity -b io.material.catalog
```
Registration survives reboot. **`bm install` WIPES the bundle dir** → redeploy the `entry.hap` (§6.2)
*after* every install.

### 6.2 Launcher icon + label — the `entry.hap`
**Symptom.** Launcher shows a blank/default icon and the raw package name.
**Root cause.** The launcher resolves icon/label **client-side via resourceManager** (the ability's
`iconId`/`labelId`), **not** via bundleResource.db (proven: editing bundleResource.db has zero launcher
effect). Adapter apps register an ability whose `resourcePath` is `…/<pkg>/entry.hap` — but that file
is never created, so resourceManager can't resolve the ids.
**Fix (per-app; no system patch, no signing — deploy the HAP directly):**
1. Get the ids: `bm dump -n io.material.catalog | grep -iE '"iconId"|"labelId"'` →
   **iconId `16777221` = 0x01000005**, **labelId `16777219` = 0x01000003**.
2. Build a minimal resource module with `restool --defined-ids` forcing media→iconId and string→labelId:
   ```
   src/module.json                              (filename MUST be module.json → stage model)
   src/resources/base/media/app_icon.png        (the logo)
   src/resources/base/element/string.json       ({ "string":[{"name":"app_name","value":"Material Catalog"}] })
   id_defined.json = {"record":[
       {"id":"0x01000005","name":"app_icon","type":"media"},
       {"id":"0x01000003","name":"app_name","type":"string"}]}
   restool -i src -j src/module.json -p io.material.catalog -o out \
       -r out/ResourceTable.h --defined-ids id_defined.json -f
   ```
3. `zip` `out/{module.json,resources.index,resources/}` → `entry.hap`.
4. Deploy → `/data/app/el1/bundle/public/io.material.catalog/entry.hap`; `chown installs:installs`;
   `chmod 644`; `chcon u:object_r:data_app_el1_file:s0`.
5. `rm /data/app/el1/100/database/com.ohos.launcher/phone_launcher/rdb/Launcher.db*` (clears the frozen
   layout) → reboot (or restart `com.ohos.launcher`).

**Verify:** launcher shows the Material Catalog logo + "Material Catalog" label (evidence
`docs/engine/V3-CATALOG-LAUNCHER-ICON-EVIDENCE/`). Repeat per-app for other apps.

### 6.3 `createHardwareBitmap` SIGBUS (universal — any demo Activity)
**Symptom.** Tapping any item that opens a new Activity (or any transition / RenderEffect) → `Fatal
signal 7 SIGBUS (BUS_ADRALN)` on RenderThread → whole catalog process dies → falls back to launcher.
**Root cause.** `android_view_ThreadedRenderer_createHardwareBitmapFromRenderNode`
(`HardwareRenderer.nCreateHardwareBitmap`) declares `ANativeWindow* window;` **uninitialized**, then
`AImageReader_getWindow(reader, &window)` — but **OHOS has no functional AImageReader**, so `window`
stays stack garbage. `proxy.setSurface(garbage)` → `CanvasContext::setupPipelineSurface` derefs it →
SIGBUS. (Symbolize the deterministic crash address with the load_bias from a known runtime vtable; it
lands on a leftover thunk on the stack — an uninitialized read, not heap corruption.)
**Fix (2 native libs, NO boot regen):**
- **libhwui** — real `createHardwareBitmap`: render the RenderNode into an off-screen OHOS
  `IConsumerSurface` (producer `ANativeWindow` wrapped like the on-screen path), `AcquireBuffer` +
  fence-wait + CPU-map the dmabuf, copy pixels into a heap `Bitmap`; null/software fallback if the
  bridge is absent. Uses the real `AImageReader_*` ABI via **dlsym** (avoids DT_NEEDED surgery).
- **bridge** — adds `oh_imagereader_{create,get_window,acquire,destroy}` in
  `surface/jni/surface_oh_helper.cpp`.

Build-fix: `surface_oh_helper.cpp` had a local `OH_NativeWindow_NativeWindowHandleOpt(void*,…)` decl
conflicting with the included `external_window.h` — remove the decl,
`reinterpret_cast<OHNativeWindow*>(nw)` at the call sites. md5 progression libhwui
`0c82b1db`→`1d04a56e`, bridge `20ab65a6`→`0a18c72b` (final after a code review hardened per-image buffer
ownership, leak-free destroy, thread-safe typed dlsym, format validation).
**Scope (honest):** this is **crash-prevention / reachability** — demo Activities open at all. The
readback is correct and hands a valid Bitmap to `MaterialContainerTransform`, but the OHOS adapter does
**not** composite the shared-element overlay (the displayId/WMS-focus wall, §8).
**Verify:** demo Activities render (e.g. `AdaptiveListViewDemoActivity` Inbox); stderr `[OH-HWBMP] OK…`;
`SIGBUS=0 SIGSEGV=0 setupPipelineSurface=0`, process alive (`docs/engine/V3-CATALOG-L2FIX-EVIDENCE/`).

### 6.4 Date Picker crash — libart W9 vtable-fixup gate
**Symptom.** "Click date picker crashed catalog." Hard native crash that logs nothing (vtable
corruption crashes too hard for a tombstone).
**Root cause.** libart's W12G-OPT-B perf optimization in `class_linker.cc::LinkMethods`
(`[W12G-W9-SKIP]`) SKIPS the W9 virtual-method shadow-routing when `super_vtable_length > 500`. The
calendar grid `MaterialCalendarGridView` (super_vt 1267 from GridView→AbsListView→AdapterView→
ViewGroup→View) NEEDS W9 routing; skipping it leaves the vtable mis-routed → first virtual dispatch
hits the wrong slot → hard crash.
**Fix.** Raise the gate `super_vtable_length > 500` → `> 100000` in
`$HOME/libart-pathA-work/src/class_linker.cc` (~line 9286); `build_libart_pathA.sh`; deploy
`/system/android/lib/libart.so`; reboot.
**Verify.** `W12G-W9-SKIP=0`, `VTA-1-W9 routed=342`; Date Picker renders calendar (month nav + 1–31 grid
+ Cancel/OK) (`docs/engine/V3-CATALOG-SWEEP-2026-06-25/datepicker-L4-calendar.jpeg`). General fix — also
unblocks Time Picker and any deep-super-vtable widget.

### 6.5 metaData NPE — the first-render fix
**Symptom.** Catalog crashes deterministically at `CatalogApplication.onCreate →
overrideApplicationComponent`: `getApplicationInfo(GET_META_DATA).metaData.getString(...)` NPEs.
**Root cause.** `ApplicationInfo.metaData` was null. The catalog falls back to its default Dagger
component when that getString returns null — so a **non-null empty Bundle** suffices.
**Fix.** The ACTIVE `PackageInfoBuilder` is in **`adapter-runtime-bcp.jar`** (it BCP-shadows the ohaf
copy — first-jar-wins, §3). Smali-patch `adapter/packagemanager/PackageInfoBuilder.buildApplicationInfo`
to `new Bundle()` → `iput-object …ApplicationInfo->metaData` before return. (Patching the ohaf copy does
nothing.) BCP → boot regen.
**Verify.** Adaptive demos grid renders, `drew=1` (`docs/engine/V3-CATALOG-DREW1-EVIDENCE/`).

### 6.6 Proxy-class LinkMethods O(n²) hang
**Symptom.** After 6.5/6.9, the app reaches UI load and **hangs ~12.5 s** (`LinkMethods+… ←
CreateProxyClass`) → LIFECYCLE timeout.
**Root cause.** libart's vtable-fixup passes (FIX-VTABLE-A reloc + W9 continued-scan + A2-ABSTRACT) are
O(num_virtual × super_vt × GetSignature). A dynamic `Proxy` implementing a big interface explodes there.
ART-generated proxies already have correct vtables — the fixup is needless for them.
**Fix (W22-PROXY-SKIP).** Add `&& !klass->IsProxyClass()` to all three pass gates in `class_linker.cc`
(LinkMethods ~8956/9210, + W9 gate `|| klass->IsProxyClass()` ~9291); rebuild libart. (The marker may
not print — the costly path is skipped before logging; the hang being gone is the proof.)

### 6.7 IME bridge — IMM → OHOS InputMethodController
**Symptom.** Even with Search-focus fixed (6.8), focusing a text field summoned no keyboard.
**Root cause.** The adapter's `adapter.window.InputMethodManagerAdapter` (in ohaf, registered as the
`"input_method"` `IInputMethodManager$Stub`) was an intentional no-op stub: `showSoftInput()`→false,
`startInputOrWindowGainedFocus()`→`InputBindResult.NO_IME`.
**Fix (implemented + deployed):**
- **bridge** (`9b2a9727`): new `input_method_bridge.cpp` registers JNI `nativeShowKeyboard/
  nativeHideKeyboard` on `adapter/window/OhImeBridge`; on show, **lazily `dlopen("liboh_ime_helper.so")`**
  + dlsym `oh_ime_show/hide/set_vm`.
- **`liboh_ime_helper.so`** (`e4880759`): the `OnTextChangedListener` 22-virtual ABI shim +
  `InputMethodController::Attach/ShowSoftKeyboard/HideSoftKeyboard`; links
  `libinputmethod_client.z.so`. Routes typed text → `OhImeBridge.nativeOn{InsertText,DeleteBefore,
  DeleteAfter,EnterAction}`.
- **ohaf** (`4690cae1`): `adapter.window.OhImeBridge` (show/hide → natives; `nativeOn*` post to the
  UI-thread Handler → focused-view `InputConnection` via reflection: WindowManagerGlobal.mRoots →
  ViewRootImpl.mView → findFocus → onCreateInputConnection → commitText/etc.); 3 forwarding edits in
  `InputMethodManagerAdapter`.

**Two bring-up bugs (critical — see §8):** (a) making `libinputmethod_client.z.so` a **DT_NEEDED of the
bridge** aborts the appspawn-x **prefork** (its INIT_ARRAY + UBSan dep crashes libart) → split into the
lazily-dlopen'd helper, never loaded into the prefork; (b) declaring `register_InputMethodBridge`
`extern "C"` (defined) vs C++-mangled (declared) left it UND → bridge fails at `JNI_OnLoad`.
**Result.** Summon chain fires on Search focus (`Attach rc=0`, IMSA `ShowSoftKeyboardInner`,
kikakeyboard foregrounds) — and the keyboard **appears + persists on a plain Text Field**
(`docs/engine/V3-CATALOG-SWEEP-2026-06-25/textfield-L3-typed.jpeg`). On SearchView it flashes then is
torn down ~65 ms later by `PerUserSession::OnFocused` because the adapter window doesn't hold WMS focus
(the §8 wall). **Text *entry* via synthetic input is unconfirmed** (OHOS `uinput` isn't bridged to
`InputConnection.commitText`) — a physical keyboard is the real test.

### 6.8 Search-focus crash — ContentResolver null-guard
**Symptom.** Tapping the Material SearchView to focus it killed the catalog.
**Root cause.** On focus, SearchView registers a `DeviceConfig` change-observer →
`ContentResolver.registerContentObserver` → `getContentService()` = `getService("content")`, but OHOS
has **no "content" service** → null → `IContentService$Stub.asInterface(null)` = null →
`invoke-interface` on null → NPE → death (the W15 handler converts the SIGSEGV to an unhandled NPE).
**Fix.** Smali null-guard in `framework.jar` `android/content/ContentResolver.smali` — in
`registerContentObserver(Uri,Z,Observer,I)` add `if-eqz <service>, :return` after `getContentService()`
(and the symmetric guard in `unregisterContentObserver`). BCP → boot regen (boot-framework.oat
`54001902`).
**Verify.** Search focuses (cursor + suggestions), 5/5 taps survive, `FATAL/SIGSEGV/SIGABRT=0`
(`docs/engine/V3-CATALOG-IME-FIX/`).

### 6.9 ConnectivityManager NPE in `handleBindApplication`
**Symptom (early).** Catalog crashed in `ActivityThread.handleBindApplication` before any UI.
**Root cause.** AOSP code: `if (getService("connectivity")!=null)
Proxy.setHttpProxyConfiguration(((ConnectivityManager)ctx.getSystemService(ConnectivityManager.class)).getDefaultProxy())`.
The adapter's ServiceManager returns a non-null connectivity binder (passes the `if`), but
`getSystemService(ConnectivityManager.class)` returns **null** (its service-fetcher was never registered
— `ConnectivityFrameworkInitializer.registerServiceWrappers()` is a return-void stub in
adapter-mainline-stubs) → `cm.getDefaultProxy()` NPEs.
**Fix.** Smali-patch `framework.jar` `ActivityThread.handleBindApplication` to add `if-eqz <cm>` after
the `check-cast ConnectivityManager` so a null `cm` skips the proxy block. BCP → boot regen.

### 6.10 Autonomous-demo SELinux fix
**Symptom.** Hands-off (no hdc): tap on the launcher does nothing.
**Root cause.** The board boots **Enforcing**; the catalog `normal_hap` domain is denied adapter access
(avc: getattr `/system/android`, search `/data/misc` + `/data/local/tmp`, dac_override).
**Fix.** `mount -o remount,rw /` → edit `/system/etc/selinux/config` `SELINUX=enforcing` →
`SELINUX=permissive` (backup first). OHOS honors it: the board boots `enforce=0` on its own (no
`setenforce`), and the `ondemand` appspawn-x auto-spawns on the icon-tap — no manual bring-up. Revert by
restoring the backup.

### 6.11 Cold-boot freeze — the fontconfig O(n²) parse (NOT AESKeyGenProbe)
**Symptom.** ~25% (2/8) of cold boots: the first catalog launch freezes (AAFWK `LIFECYCLE_TIMEOUT`),
watchdog kills it.
**Root cause (long-believed wrong — see §8).** `SkFontMgr::RefDefault()` (a per-fork singleton) parses
the **6.3 MB `/system/fonts/hm_symbol_config_next.json`** on the app's first
`android.graphics.fonts.Font.Builder.build()`; the cJSON parser uses `cJSON_GetArrayItem` (O(index)
linked-list walk) inside per-symbol loops → O(n²) over thousands of symbols. Under cold-boot CPU
contention it intermittently exceeds the ~10 s watchdog. **`AESKeyGenProbe` is a red herring** — it
completes in ~15 ms; it was just the last named init marker before the freeze. Caught with
`dumpcatcher -p <catalogpid>` on the LIVE hang (the watchdog sysfreeze dump captures the *wrong*
process).
**Fix.** Replace the 6.3 MB json with a structurally-valid minimal one (empty `common_animations` /
`special_animations` / `symbol_layers_grouping`), md5 **425290bd** (144 B):
`mount -o remount,rw /` → `cp` (preserves ctx `system_fonts_file`) → `chmod 644`. No binary patch, no
boot regen, fully reversible (backup `/data/local/tmp/pre-symbolfix/`, orig `6ed9f4d6`). Only decorative
HM-symbol glyphs are lost; normal text is unaffected.
**Verify.** 25% (2/8) → **0% (16/16)** bad-boot (`docs/engine/V3-CATALOG-DEMO-READY/`).

### 6.12 Morph animation — ValueAnimator durationScale
**Symptom.** The shared-element container-transform "morph" snapped instead of animating.
**Root cause.** `android.animation.ValueAnimator.sDurationScale == 0` in OHOS app processes — **all
animations globally disabled** (AOSP default is 1.0f, but a caller in framework classes4.dex sets it to
0 at init; the adapter's `animator_duration_scale` prime isn't wired — proven by deploying scale="10"
via boot-regen with zero effect). scale 0 → one-shot animators jump to end on frame 1.
**Fix (app-level).** Inject `ValueAnimator.setDurationScale(1.0f)` into the catalog's
`io.material.catalog.transition.ContainerTransformConfigurationHelper.configure()` (runs right before
each morph): smali `const/high16 v0, 0x3f800000` + `invoke-static {v0},
Landroid/animation/ValueAnimator;->setDurationScale(F)V`.
**★ Deploy lesson (cost hours):** the catalog APK loads from
`/data/app/el1/bundle/public/io.material.catalog/android/base.apk` (+ `oat/arm/` cache) — **NOT**
`/data/app/android/io.material.catalog/base.apk` (patching the latter does nothing). Deploy to the
el1/bundle path, `chmod 0644`, clear `oat/arm/*`, relaunch. Verify a patch is live via a marker written
by the patched method to a **pre-created 0666** file (the app uid can write but not create in
`/data/local/tmp`). **`bm install` WIPES the bundle dir** — re-patch after install.
**Verify.** Container Transform "View" demo morphs frame-by-frame
(`docs/engine/V3-CATALOG-L3-MORPH-EVIDENCE/`).

---

## 7. Demo setup

The hands-off demo (no laptop):

1. **Persistent Permissive** — apply §6.10 once (`/system/etc/selinux/config` → permissive). The board
   then boots `enforce=0` and the `ondemand` appspawn-x auto-spawns on the icon-tap.
2. **Cold-boot reliability** — apply §6.11 (fontconfig) so the first launch never freezes.
3. **Flow:** power on → board boots Permissive → **swipe up to unlock** ("上滑解锁", no PIN) → **tap the
   Material Catalog icon** (launcher row 2, ~x500 y320) → wait ~25–30 s (vtable fixups + dex linking are
   slow; a semi-transparent loading window shows first) → the Material 3 grid paints → navigate.
   - The first tap right after unlock can miss (launcher settles ~2 s) — tap again.
   - No clean lockscreen-disable param exists (`settings` CLI absent); the swipe-up is acceptable.
4. **Mock battery** — if a "low battery" overlay appears, it's fake (DC-powered). Set a level:
   `hidumper -s 3302 -a "--capacity 95"`. **Reboot is always safe.**

Evidence of the full flow: `docs/engine/V3-CATALOG-DEMO-READY/{02-coldboot-unlock-launcher-catalog-icon,
03-tap-icon-catalog-renders}.jpeg`.

---

## 8. ★ CRITICAL GOTCHAS (read first)

These are the highest-value, hardest-won rules. Several survive reboot and cost a hardware recovery.

### Brick / device-loss footguns (survive reboot)
- **NEVER add `critical` to a fail-prone init service.** `"critical":[…]` in `appspawn_x.cfg` makes
  init *reboot* the device if the service fails; with `start-mode:boot` that's before recovery →
  **bootloop brick** → the USB endpoint disappears → reflash + full wipe. This happened. Ship appspawn-x
  **non-critical** (`"critical":[]`, `"start-mode":"ondemand"`) and reboot **manually** after staging
  (no `--reboot` from the deploy script). The brick is also partly environmental (the vendor's own
  procedure bricks this exact board) — bisect single artifacts and get serial/UART logs through the
  reboot; don't assume "I deployed it wrong."
- **NEVER `param set persist.sys.usb.config none` (or to *anything*).** It disables the USB gadget →
  **hdc-over-USB dies on the next reboot**, and because it's `persist.` the reboot does NOT undo it →
  the user must physically toggle USB debugging on-device. This broke the link 2–3 times, including a
  subagent that did it *despite an ALL-CAPS warning*. The safe value is `hdc_debug` — don't touch it.
  To dismiss the **connection-mode popup**: `param set persist.usb.setting.gadget_conn_prompt false`
  (owner `com.usb.right`). To dismiss the **hdc-auth prompt**: authorize once (key caches in
  `/data/misc/hdc/hdc_keys`) or `param set const.hdc.secure 0`. **If you delegate any device work, put
  this prohibition as the literal first line of the subagent's prompt.**

### The board is DC-powered with a MOCK battery
- `/sys/class/power_supply/` is empty; "low battery / 11%" is **fake**. **Reboot freely** — it's the
  recovery move, not a risk. Set a simulated level for screenshots/foreground:
  `hidumper -s 3302 -a "--capacity 95"`. (A low fake level triggers an aggressive lockscreen; wake with
  `power-shell wakeup; power-shell timeout -o 600000`.)

### Diagnosis traps (the visible symptom is downstream of the cause)
- **The bad-boot is NOT `AESKeyGenProbe`** (a long-held wrong belief). It's the 6.3 MB fontconfig O(n²)
  parse (§6.11). The probe completes in 15 ms — it was just the last marker before the freeze. Don't
  nop it expecting a fix.
- **Sysfreeze / watchdog stack dumps LIE** — they capture the downstream zombie or the wrong process,
  not the root cause. For an AAFWK `LIFECYCLE_TIMEOUT`, get the stack with **`dumpcatcher -p <live-pid>`
  on the actual hung process** (poll ~1–2 s after launch, before the ~10 s kill); also pull `hilog -x`
  and read the `[B43-BIND]` / `[B47-SLA]` bind trace — the `InvocationTargetException`'s `Caused by:` is
  the real wall. (`processdump` is disabled.)
- **A "drew=1 / newest-stderr" check FALSE-PASSES bad-boots** — the watchdog kills the frozen 1st
  launch and AMS respawns a drawing 2nd. Verify the **FIRST pid is stable + sysfreeze count == 0**.
- **R8-inlined stacks hide root causes** — inlining collapses multiple distinct bugs into one
  `method:Unknown Source:NNN`. Record `:NNN` for every frame; after a fix, if `:NNN` *shifted* you fixed
  a symptom and unmasked a *different* bug. (One case: fixing a `clone()` at `:132` revealed a
  `ClassCastException` at `:197` — 6 hours lost attacking the wrong line.) Baksmali the synthetic method
  and map each `:NNN`.

### JNI traps (loud-vs-silent)
- **Verify JNI signatures against the on-device framework smali, not AOSP source.** Signatures drift
  across AOSP versions (e.g. `SQLiteConnection.nativeExecute` is `(JJ)V` on AOSP-14 but `(JJZ)V` on
  AOSP-15). A wrong signature makes `RegisterNatives` return **-1 silently** for the whole table →
  `NoSuchMethodError` on first call. Pull + `apktool d` the jar; `grep '\.method.*native'`.
- **Never ship stub JNI bodies.** A no-op native passes `RegisterNatives`, runs, returns — but skips a
  side effect some *other* native depends on, surfacing as a cryptic error 4 layers downstream that
  doesn't name the stub (a stubbed `nativeRegisterLocalizedCollators` → later `REINDEX LOCALIZED` fails
  → looked like a Looper wall; ~2 days lost). Either implement it fully or **OMIT it** (so the caller
  gets a loud `UnsatisfiedLinkError`). Flag suspiciously small bodies: `nm --print-size --defined-only`
  → any `[tT]` under ~16 bytes is suspect.

### Architecture / workflow
- **BCP first-jar-wins.** To shadow a class, put the authoritative jar earlier (Scope C puts
  `adapter-runtime-bcp` before `oh-adapter-framework`). Beware an earlier jar with an *incomplete*
  duplicate (→ unbound-JNI SIGBUS). A class can be *referenced* in one jar but *defined* in another —
  patch the definer.
- **The WMS-focus / displayId compositing wall.** Adapter app windows render on **top** (composited)
  but do **not** hold OHOS WMS focus — `EntryView`/SceneBoard (the launcher) keeps focus even though the
  adapter calls `RequestFocus(windowId)`. Consequence: some modals/popups composite intermittently per
  boot, and focus-sensitive views (SearchView) get their keyboard torn down. This is the single
  highest-leverage remaining item; it is NOT a per-app bug.
- **Synthetic input → InputConnection is not bridged.** OHOS `uinput`/synthetic keys (and the BACK key,
  `uinput -K -d 2`) don't reach the Android `InputConnection.commitText`. Use the on-screen back arrow
  (~55,64); a physical keyboard is the untested real test for text entry.
- **Subtraction, not addition.** Prefer removing / null-guarding over adding shims. Most §6 fixes are
  null-guards for "OHOS lacks service X → null → NPE", not new shim classes. The additive reflex
  (NPE → add a shim → repeat) never converges and accumulates per-app workarounds. When debugging,
  start from the working baseline and *subtract* layers until it first breaks.
- **The smali round-trip + boot-regen workflow.** BCP-jar change → smali patch the **definer** → repack
  → **regen all 30 boot segments** (mandatory after *any* BCP-jar byte change) → atomic deploy (30 segs
  + jar together) → reboot. Native `.so` changes do NOT need boot regen.
- **App APK load path.** `/data/app/el1/bundle/public/<pkg>/android/base.apk` (+ `oat/arm` cache), NOT
  `/data/app/android/...`. Clear `oat/arm/*` after patching. `bm install` wipes the bundle dir.
- **Capture quirks.** Snapshots can show a stale frame or the launcher (displayId compositing) even when
  the app drew — re-snap, or **direct-launch** (`aa start -a <ability>`) for a fresh composite (more
  reliable than fragment-tap for the 19 activity-backed demos). Fresh composite ≈ 40–130 KB; stale /
  launcher ≈ 20–34 KB. The device sometimes loses `awk`/`tr`/`comm` after reboot — text-process
  host-side; use toybox `tar` via `/data/local/tmp` to extract boot files.
- **Prefork poisoning.** A crashing `.so` loaded into the appspawn-x prefork damages *all* subsequent
  forks until **reboot** — reboot between `.so` swaps when isolation-testing. Never make an
  inputmethod/UBSan-bearing `.so` a DT_NEEDED of the bridge (dlopen it lazily from the forked app).
- **Java logging is dead in the app process.** `Log.i`/`System.err` from the catalog don't reach the
  child stderr — only native `fprintf` does. To observe Java behavior, write to a **pre-created 0666**
  file.

---

## 9. Verification

How to confirm a reproduction is correct, in order:

1. **appspawn-x is up:** after boot (Permissive), `ps -A | grep appspawn-x`; on launch, the child stderr
   `/data/service/el1/public/appspawnx/adapter_child_<pid>.stderr` reaches **"Phase 4" / event loop**
   with **0** of `mark_sweep | Fatal | cppcrash | Class mismatch | ValidateOatFile | InitWithoutImage`.
2. **Registration:** `bm dump -n io.material.catalog` shows `bundleType 10` (APP_ANDROID), codePath
   `…/android`, MainActivity + abilities, userId 100 — and survives a reboot.
3. **Renders:** `aa start -a io.material.catalog.main.MainActivity -b io.material.catalog` → child stderr
   `drew=1 width=720 height=1280`, process stays alive (`docs/engine/V3-CATALOG-DREW1-EVIDENCE/`).
   - Catalog pid name truncates to `io.material.cat`; `pidof` fails (empty cmdline) → use
     `ps -A | grep material`. Catalog uid is **16371**.
4. **The 32-category sweep:** drive `uinput -T -c x y` taps (or direct-launch the activity demos),
   capture `snapshot_display -f X.jpeg`; expect all 32 categories navigable to L3, every widget type
   driven with a visible result, Date Picker L5 modal interaction, **0** tombstone/cppcrash/Fatal-signal
   (`docs/engine/V3-CATALOG-SWEEP-2026-06-25/COVERAGE.md`).
5. **Cold-boot demo flow:** from a cold power-on, swipe-unlock → tap the icon → grid paints, first pid
   stable, sysfreeze == 0 — repeat ≥8× (expect 0 bad-boots with the fontconfig fix).

---

## 10. Key paths / artifacts / md5s

### Currently-deployed component md5s (the demo-ready state)
| Component | md5 | Device path |
|---|---|---|
| `libart.so` | `2813065e` | `/system/android/lib/libart.so` |
| `framework.jar` | `e6f9e1a3` | `/system/android/framework/framework.jar` |
| `adapter-runtime-bcp.jar` | `c026e80c` | `/system/android/framework/adapter-runtime-bcp.jar` |
| `oh-adapter-framework.jar` (ohaf) | `4690cae1` | `/system/android/framework/oh-adapter-framework.jar` |
| `liboh_adapter_bridge.so` (bridge) | `9b2a9727` | `/system/lib/` + `/system/android/lib/` |
| `liboh_ime_helper.so` | `e4880759` | `/system/lib/` + `/system/android/lib/` |
| `libappexecfwk_common.z.so` (common) | `4d2c6399` | `/system/lib/platformsdk/` + `/system/android/lib/` |
| `appspawn-x` (10-jar) | `3abe3bde` | `/system/bin/appspawn-x` |
| `boot-framework.oat` | `54001902` | `/system/android/framework/arm/` |
| `hm_symbol_config_next.json` (minimal) | `425290bd` | `/system/fonts/` |

> **md5 reconciliation caveat.** Across sessions the *same* BCP jar shows competing md5s per feature
> (e.g. ohaf appeared as 300581d1 → 4690cae1 for IME; framework.jar f5fd86ef → e6f9e1a3; arb
> d5d39a05 → c026e80c). A from-scratch build must apply **all** the smali patches to each BCP jar and do
> **one** final boot regen — do not deploy per-session md5s in isolation. The IME ohaf descends from
> 300581d1; if you also want the transition-options patch (§6.12 L1 / `TransitionOptionsHolder`),
> reconcile both smali patch sets before the single combined regen. The table above is the live
> demo-ready set; the boot image must be regenerated to match whatever jar set you assemble.

### Build trees / host tools
- `$HOME/bridge-build` (+ `build/`: `build_appspawn_x.sh`, `build_adapter.sh`,
  `build_aosp_lib.sh`, `build_aosp_fw.sh`, `build_boot_image.sh`, `build_adapter_runtime_bcp.sh`,
  `config.sh`)
- `$HOME/libart-pathA-work` (`src/class_linker.cc`, `build_libart_pathA.sh`)
- `$HOME/tools/{dex2oat64, lib64/, oatdump}` — boot-image dex2oat (OAT 230)
- `$HOME/openharmony/out/sdk/ohos-sdk/linux/toolchains/restool` — entry.hap compiler
- `$HOME/apktool.jar` + `/tmp/fwktools` — smali round-trip
- `$HOME/android-sdk/cmdline-tools/latest/lib/r8.jar` (D8) ; `…/platforms/android-34/android.jar`

### Deploy bundle
- `$HOME/westlake-complete/` — `overlay/` (the v3-hbc base adapter), `current-fixes/`,
  `device-tmp/`, `MANIFEST.md5`, `README-COMPLETE.md`, and **`overlay/scripts/DEPLOY_SOP.md`** (the
  authoritative procedure).

### Device paths (quick reference)
- libs: `/system/android/lib/` (AOSP-side, incl. libart/libhwui) ; `/system/lib/` (bridge + dual-path
  shims) ; `/system/lib/platformsdk/` (OHOS service libs).
- framework jars: `/system/android/framework/` ; boot image: `/system/android/framework/arm/boot*`.
- app: `/data/app/el1/bundle/public/io.material.catalog/{android/base.apk,entry.hap}`.
- child stderr: `/data/service/el1/public/appspawnx/adapter_child_<pid>.stderr`.
- SELinux: `/system/etc/selinux/config` ; init: `/system/etc/init/appspawn_x.cfg`.

### hdc / staging (WSL)
- `/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe` + a `/tmp/h` wrapper (filters `UtilAcceptVsock`).
- Stage all sends/recvs through a Windows dir `C:\Users\dspfa\Dev\ohos-tools\` (hdc.exe mangles WSL abs
  paths + silently drops large files). Always size-verify + md5-verify after a send.

---

## 11. Pointers (evidence + deeper detail)

### Evidence directories (under `$HOME/openharmony/docs/engine/`)
- **`V3-CATALOG-DEMO-READY/`** — the cold-boot demo flow + the fontconfig fix (README + the live
  `dumpcatcher` hang stack).
- **`V3-CATALOG-SWEEP-2026-06-25/`** — `COVERAGE.md` (the 32-category sweep) + 100+ screenshots.
- **`V3-CATALOG-DREW1-EVIDENCE/`** — first `drew=1` Material 3 grid.
- **`V3-CATALOG-IME-FIX/`** — Search-focus crash fix (README + screenshots).
- **`V3-CATALOG-IME-BRIDGE/`** — the IMM→OHOS-IME bridge (README + `ime_chain_stderr.txt` + the
  WMS-focus blocker).
- `V3-CATALOG-L2FIX-EVIDENCE/` (createHardwareBitmap), `V3-CATALOG-L3-MORPH-EVIDENCE/` (morph),
  `V3-CATALOG-LAUNCHER-ICON-EVIDENCE/` (icon/label), `V3-CATALOG-EXHAUSTIVE-SWEEP/` (Date Picker fix),
  `V3-CATALOG-UI-PIPELINE-VALIDATION-2026-06-24.md` (the 7-wall/9-fix summary).

### Architecture
- `docs/engine/WESTLAKE-ARCHITECTURE-V2-2026-05-24.md` — the full 7-layer architecture (spawn, class
  load/ART, lifecycle, service brokering, window/surface, rendering, input/IME) with sequence diagrams
  and the memory/CPU model.

### Source recipe (public)
- `https://github.com/A2OH/westlake` (`westlake-noice-ohos/` for the source recipe;
  `REPRODUCE-CLEAN-WSL.md` for the full OHOS-tree rebuild). The catalog work was pushed to A2OH/westlake
  main (commits referenced in the milestone docs).

### Engineering memory (for an agent with access — deeper detail per topic)
`$HOME/.claude/projects/-home-user-openharmony/memory/` — notably `MEMORY.md` (the index +
"START HERE" journal), `catalog-badboot-is-fontconfig-not-aeskeygenprobe.md`,
`catalog-2nd-level-canvascontext-wall.md`, `catalog-ime-bridge-impl.md`, `catalog-ime-search-findings.md`,
`adapter-launcher-icon-entryhap-fix.md`, `adapter-bootloop-wipe-recovery.md`, `never-set-usb-config-none.md`,
`feedback_appspawn_x_critical_boot_brick.md`, `feedback_bcp_first_jar_wins_2026-05-25.md`,
`reference_boot_regen_cycle_2026-05-30.md`.
