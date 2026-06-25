# Westlake — running the Material Components Catalog on OpenHarmony (OHOS)

This document captures running the **Material Components Catalog**
(`io.material.catalog`, an **unmodified** Android APK from
`material-components/material-components-android`) as a first-class OHOS app on an
**OpenHarmony DAYU200 / RK3568** board (32-bit ARM, app uid **16371**) via the
**appspawn-x** AOSP-app adapter.

It is the catalog counterpart to `REPRODUCE.md` (which covers the app *noice*).
The adapter, build host, hdc/staging quirks and boot-regen cycle are shared with
noice — this file documents the **catalog-specific** registration, launcher icon,
and the root-cause fixes that took the catalog from "crashes at init" to
"demo-ready: launches from the icon, all 32 widget categories navigable, Date
Picker + dialogs + side-sheets work, soft keyboard appears."

> **Read first:** `STATUS.md` (honest end-to-end status, both apps), then this
> file. The full self-contained engineering write-up is
> `docs/REPRODUCTION-GUIDE.md` (the 800-line adapter+catalog guide copied in from
> the project memory); this file is the distilled, repeatable catalog procedure +
> the per-fix reference. `BUILD-FROM-SOURCE.md` covers building the stock APK.

---

## 0. What this is

The catalog is a **stock** Android APK (no source changes). OHOS has no Android
app runtime; the project runs it via the same **AOSP-app adapter** as noice — a
fork of OHOS `appspawn` (`appspawn-x`) that preloads the AOSP-14 framework + ART
once and `fork()`s per Android app. Each child is a real OHOS scene session,
indistinguishable to OHOS WindowManager / launcher from a native OHOS app, while
presenting a normal AOSP environment to the APK.

The catalog exercises far more of the framework than noice (32 widget
categories, Date/Time pickers, dialogs, drawers, side sheets, the soft keyboard,
shared-element transitions), so it surfaced a different set of walls. **Almost
every fix below is universal** (any adapter app benefits), not catalog-specific.
A recurring family: *OHOS lacks an Android system service →
`ServiceManager.getService(...)` returns null → an unguarded interface call NPEs
→ process death.* The fix is a **null-guard** (subtraction), not a shim.

### Components (catalog-relevant) and their deployed device paths
| Component | Role | Device path | Deployed md5 |
|---|---|---|---|
| `libart.so` | AOSP-14 ART + the W-series vtable/proxy fixes + perf-logging trim (§D, §E, §perf). **OAT version 230.** | `/system/android/lib/libart.so` (this path ONLY) | `ba40f173` |
| `framework.jar` | AOSP framework, smali-patched (ConnectivityManager + ContentResolver null-guards, §C, §G) | `/system/android/framework/framework.jar` | `e6f9e1a3` |
| `adapter-runtime-bcp.jar` (arb) | preload orchestrator + the **active** `PackageInfoBuilder` (metaData fix, §B) | `/system/android/framework/adapter-runtime-bcp.jar` | `c026e80c` |
| `oh-adapter-framework.jar` (ohaf) | the ~30 Java adapter bridge classes incl. the IME `OhImeBridge` (§H) | `/system/android/framework/oh-adapter-framework.jar` | `4690cae1` |
| `liboh_adapter_bridge.so` (bridge) | input/window/surface + the IME-summon loader (§H) | `/system/lib/` + `/system/android/lib/` | `9b2a9727` |
| `liboh_ime_helper.so` | IME helper — InputMethodController calls; dlopen'd lazily (§H) | `/system/lib/` + `/system/android/lib/` | `e4880759` |
| `libappexecfwk_common.z.so` | OHOS bundle-mgr; the `.app`→`.apk` byte-patch (§A) | `/system/lib/platformsdk/` + `/system/android/lib/` | `4d2c6399` |
| `appspawn-x` (10-jar) | Zygote-equivalent for Android apps | `/system/bin/appspawn-x` | `3abe3bde` |
| `boot-framework.oat` | dex2oat boot image segment (one of 30) | `/system/android/framework/arm/` | (see ARTIFACT-INVENTORY) |
| `hm_symbol_config_next.json` (minimal) | cold-boot fontconfig fix (§font) | `/system/fonts/` | `425290bd` |
| catalog `base.apk` + `entry.hap` | the Android app + the OHOS launcher-icon resource HAP (§2) | `/data/app/el1/bundle/public/io.material.catalog/{android/base.apk,entry.hap}` | apk `a9df5518` |

> **md5 reconciliation caveat.** Across sessions the *same* BCP jar shows
> competing md5s per feature (e.g. ohaf appeared as `300581d1` → `4690cae1` once
> the IME bridge landed; framework.jar `f5fd86ef` → `e6f9e1a3`; arb `d5d39a05` →
> `6e32a253` → `c026e80c`; libart `2813065e` → `ba40f173` once the perf trim
> landed). A from-scratch build must apply **all** the smali/source patches to
> each component and do **one** final boot regen — do not deploy per-session md5s
> in isolation. The table above is the live demo-ready set; the boot image must be
> regenerated to match whatever jar set you assemble (see §boot-regen).

### Device access
Same as noice (`REPRODUCE.md` §0): reach the board over `hdc` from WSL via the
Windows `hdc.exe`, strip the CRLF it adds, and **stage all file sends/recvs
through a Windows dir** (`C:\Users\dspfa\Dev\ohos-tools\`) because hdc.exe
mangles WSL absolute paths and silently drops large files.

---

## 1. App registration — the `.app`→`.apk` byte-patch + `bm install`  [§A]

**Symptom.** `bm install -p /data/local/tmp/catalog.apk` is rejected
*client-side* ("file is not hap, hsp…") before it even reaches the bundle
manager.

**Root cause.** `libappexecfwk_common.z.so`
(`bundle_file_util.cpp::CheckFilePath`) carries a literal `.app` in its
allowed-extension list at file offset **0x1a40**; `.apk` is not in the list.

**Fix.** One byte: offset `0x1a40`, `0x70` (`'p'`) → `0x6b` (`'k'`) — turns the
allowed `.app` into `.apk`. Patched lib md5 **`4d2c6399`**. Deploy to **both**
`/system/lib/platformsdk/` (the copy foundation/BMS loads) **and**
`/system/android/lib/`; `chcon u:object_r:system_lib_file:s0`; reboot. (The
deployed `libbms` `7d7f508a` is already APK-capable via
`oh_adapter_install_apk_with_manifest`; this byte was the only gate.)

**Then:**
```bash
bm install -p /data/local/tmp/catalog.apk      # "install bundle successfully"
bm dump -n io.material.catalog                 # bundleType 10 (APP_ANDROID), MainActivity + abilities, userId 100
aa start -a io.material.catalog.main.MainActivity -b io.material.catalog
```
Registration survives reboot. **`bm install` WIPES the bundle dir** → redeploy the
`entry.hap` (§2) *and* any patched `base.apk` *after* every install.

---

## 2. Launcher icon + label — the `entry.hap`  [§icon]

**Symptom.** The OHOS launcher shows a blank/default icon and the raw package
name instead of the Material Catalog logo + "Material Catalog".

**Root cause.** The launcher resolves icon/label **client-side via
resourceManager** (the ability's `iconId`/`labelId`), **not** via
`bundleResource.db` (proven: editing `bundleResource.db` — even a native app's
LABEL — has zero launcher effect). Adapter apps register an ability whose
`resourcePath` is `…/<pkg>/entry.hap` — but that file is never created, so
resourceManager can't resolve the ids.

**Fix (per-app; no system patch, no signing — deploy the HAP directly).** The
build inputs are committed under **`entry-hap/`**:
- `entry-hap/src/module.json` — the stage-model module (filename **must** be
  `module.json`); declares the `EntryAbility` with `icon: $media:app_icon` +
  `label: $string:EntryAbility_label`.
- `entry-hap/src/resources/base/media/app_icon.png` — the Material Catalog logo.
- `entry-hap/src/resources/base/element/string.json` — `app_name` /
  `EntryAbility_label` = "Material Catalog".
- `entry-hap/id_defined.json` — forces media→**iconId** and string→**labelId**.

Get the ids from the bundle (catalog: **iconId `16777221` = 0x01000005**,
**labelId `16777219` = 0x01000003**):
```bash
bm dump -n io.material.catalog | grep -iE '"iconId"|"labelId"'
```
Build with `restool --defined-ids`
(`$HOME/openharmony/out/sdk/ohos-sdk/linux/toolchains/restool`, v4.105):
```bash
cd entry-hap
restool -i src -j src/module.json -p io.material.catalog -o out \
        -r out/ResourceTable.h --defined-ids id_defined.json -f
(cd out && zip -r ../../entry.hap module.json resources.index resources)   # module.json + resources.index + resources/ at the zip ROOT
```
Deploy + clear the launcher's frozen layout:
```bash
# send entry.hap via the Windows path, then:
sh "cp /data/local/tmp/entry.hap /data/app/el1/bundle/public/io.material.catalog/entry.hap
    chown installs:installs /data/app/el1/bundle/public/io.material.catalog/entry.hap
    chmod 644 /data/app/el1/bundle/public/io.material.catalog/entry.hap
    chcon u:object_r:data_app_el1_file:s0 /data/app/el1/bundle/public/io.material.catalog/entry.hap"
sh "rm /data/app/el1/100/database/com.ohos.launcher/phone_launcher/rdb/Launcher.db*"   # clears the frozen layout
sh "reboot"   # (or restart com.ohos.launcher)
```
**Verify:** launcher shows the Material Catalog logo (white square + green circle)
+ "Material Catalog" label (evidence
`docs/engine/V3-CATALOG-LAUNCHER-ICON-EVIDENCE/`). Repeat per-app for other apps
(each its own logo + iconId/labelId).

> Setting the ability `launchMode` to `singleton` in `module.json` also prevents
> the multiton "zombie mission" recents entries (see §honest); the deployed
> catalog ability is STANDARD/multiton, so every launch makes a new mission.

---

## 3. The startup-and-interaction fixes (file → what it solves → status)

The fixes below are in roughly the order the catalog hits them at runtime:
3 startup NPEs (§B/§C/§G) → render → Date Picker (§D) → proxy hang (§E) →
Search/IME (§G/§H). Each is **universal** unless noted.

### B. metaData NPE — the first-render fix  (arb `c026e80c`)
**Symptom.** Catalog crashes deterministically at `CatalogApplication.onCreate →
overrideApplicationComponent`:
`getApplicationInfo(GET_META_DATA).metaData.getString(...)` NPEs before any UI.
**Root cause.** `ApplicationInfo.metaData` was **null**. The catalog falls back to
its default Dagger component when that `getString` returns null — so a **non-null
empty Bundle** suffices (real values not needed).
**Fix.** The ACTIVE `PackageInfoBuilder` is in **`adapter-runtime-bcp.jar`** — it
BCP-shadows the ohaf copy (first-jar-wins, see §boot-regen). Smali-patch
`adapter/packagemanager/PackageInfoBuilder.buildApplicationInfo`: after
`new ApplicationInfo()`, add `new Bundle()` → `iput-object …->metaData` before
return. **Patching the ohaf copy does NOTHING** (wrong/shadowed copy). BCP →
boot regen. **Status: FIXED** — Adaptive grid renders (`drew=1`,
`docs/engine/V3-CATALOG-DREW1-EVIDENCE/`).

### C. ConnectivityManager NPE in `handleBindApplication`  (framework.jar `e6f9e1a3`)
**Symptom (early).** Catalog crashes in `ActivityThread.handleBindApplication`
before any UI.
**Root cause.** AOSP code:
`if (getService("connectivity")!=null) Proxy.setHttpProxyConfiguration(((ConnectivityManager)ctx.getSystemService(ConnectivityManager.class)).getDefaultProxy())`.
The adapter's ServiceManager returns a non-null connectivity binder (passes the
`if`), but `getSystemService(ConnectivityManager.class)` returns **null** (its
service-fetcher was never registered —
`ConnectivityFrameworkInitializer.registerServiceWrappers()` is a return-void stub
in adapter-mainline-stubs) → `cm.getDefaultProxy()` NPEs.
**Fix.** Smali-patch `framework.jar` `ActivityThread.handleBindApplication`: add
`if-eqz <cm>` after the `check-cast ConnectivityManager` so a null `cm` skips the
proxy block. BCP → boot regen. **Status: FIXED** — bind completes.

### D. Date Picker crash — the libart W9 vtable-fixup gate  (libart `ba40f173`)
**Symptom.** "Click date picker crashed catalog." A hard native crash that logs
**nothing** (vtable corruption crashes too hard for a tombstone — no SIGSEGV in
the child stderr, no faultlog).
**Root cause.** libart's W12G perf optimization in
`class_linker.cc::LinkMethods` (`[W12G-W9-SKIP]`) SKIPS the W9 virtual-method
shadow-routing when `super_vtable_length > 500`. The calendar grid
`MaterialCalendarGridView` (super_vt **1267**, from
GridView→AbsListView→AdapterView→ViewGroup→View) NEEDS W9 routing; skipping it
leaves its vtable mis-routed → the first virtual dispatch hits the wrong slot →
hard crash.
**Fix.** Raise the gate `super_vtable_length > 500` → `> 100000` in
`$HOME/libart-pathA-work/src/class_linker.cc` (~line 9316), so W9 runs for
every realistic class while the guard stays in place for any pathological
super_vt. Rebuild via `build_libart_pathA.sh`; deploy `/system/android/lib/libart.so`
(this path ONLY); reboot. The committed source patch is
`libart-patches/class_linker.cc.westlake-W-series.diff` (the relevant hunk is the
`if (super_vtable_length > 100000 || klass->IsProxyClass())` gate). **Status:
FIXED + VERIFIED** — `W12G-W9-SKIP=0`, `VTA-1-W9 routed=342`; the Date Picker
renders its calendar (month nav + 1–31 grid + 取消/确定) and is modal-interactive
to L5 (`docs/engine/V3-CATALOG-SWEEP-2026-06-25/`). General fix — also unblocks
Time Picker and any deep-super-vtable widget.

> **libart is recoverable, not a hard brick.** libart only affects Android-app
> (`adapter_child`) processes — OHOS + hdc boot regardless. Back up the prior
> libart before deploying.

### E. Proxy-class LinkMethods O(n²) hang  (libart `ba40f173`)
**Symptom.** After the metaData/connectivity fixes the app reaches UI load and
**hangs ~12.5 s** (`LinkMethods+… ← CreateProxyClass`) → AAFWK LIFECYCLE timeout.
**Root cause.** libart's vtable-fixup passes (FIX-VTABLE-A reloc + the W9
continued-scan + the abstract pass) are O(num_virtual × super_vt × GetSignature).
A dynamic `Proxy` implementing a big interface explodes there. ART-generated
proxies already have correct vtables — the fixup is needless for them.
**Fix (W22-PROXY-SKIP).** Add `&& !klass->IsProxyClass()` to the three LinkMethods
pass gates in `class_linker.cc` (~8956 and ~9217), and `|| klass->IsProxyClass()`
to the W9 gate (~9316). Rebuild libart. (The marker may not print — the costly
path is skipped before logging; the hang being gone is the proof.) Captured in the
same `libart-patches/class_linker.cc.westlake-W-series.diff`. **Status: FIXED** —
no hang; UI loads.

### F. Morph animation — the `ValueAnimator` durationScale  (catalog APK `a9df5518`)
**Symptom.** The shared-element container-transform "morph" snapped instead of
animating.
**Root cause.** `android.animation.ValueAnimator.sDurationScale == 0` in OHOS app
processes — **all animations globally disabled** (AOSP default is 1.0f, but a
caller in framework `classes4.dex` sets it to 0 at init; the adapter's
`animator_duration_scale` prime isn't wired to it — proven by deploying scale="10"
via boot-regen with zero effect). scale 0 → one-shot animators jump to end on
frame 1.
**Fix (app-level — the ONE catalog APK patch).** Inject
`ValueAnimator.setDurationScale(1.0f)` at the top of the catalog's
`io.material.catalog.transition.ContainerTransformConfigurationHelper.configure()`
(runs right before each morph). The patched smali (class is in **classes3.dex**)
is `catalog-smali-patches/ContainerTransformConfigurationHelper.smali`; the
injected instructions are:
```smali
const/high16 v0, 0x3f800000    # 1.0f
invoke-static {v0}, Landroid/animation/ValueAnimator;->setDurationScale(F)V
```
**★ Deploy lesson (cost hours):** the catalog APK loads from
`/data/app/el1/bundle/public/io.material.catalog/android/base.apk` (+ `oat/arm/`
cache), **NOT** `/data/app/android/io.material.catalog/base.apk` — patching the
latter does nothing. Deploy to the el1/bundle path, `chmod 0644`, clear
`oat/arm/*`, relaunch. Verify a patch is live via a marker written by the patched
method to a **pre-created 0666** file (the app uid 16371 can write but not create
in `/data/local/tmp`). `bm install` WIPES the bundle dir → re-patch after install.
**Status: FIXED** — the Container Transform "View" demo morphs frame-by-frame
(`docs/engine/V3-CATALOG-L3-MORPH-EVIDENCE/`).

> The shared-element morph also required carrying the `ActivityOptions` Bundle
> from the source activity to the destination's `EnterTransitionCoordinator`. That
> plumbing — a static handoff class
> **`adapter.activity.TransitionOptionsHolder`** added to `arb`, plus stash/resolve
> edits in **`adapter.activity.ActivityTaskManagerAdapter`** (NOT
> `ActivityManagerAdapter` — the catalog's launch routes through the *Task* manager
> JNI `OH_ATMJNI`) and `AppSchedulerBridge` — is the L1/L2 work. Its source +
> smali are committed under `framework-smali-patches/catalog/TransitionOptionsHolder.*`.
> The FINAL clean demo state runs with the durationScale APK fix as the
> load-bearing piece (arb `fda6948c` carried the L1 options-carry; the deployed
> demo-ready arb is `c026e80c`). See `docs/REPRODUCTION-GUIDE.md` §6.12 and the
> `catalog-2nd-level-canvascontext-wall` memory for the full L1→L2→L3 arc.

### G. Search-focus crash — ContentResolver null-guard  (framework.jar `e6f9e1a3`)
**Symptom.** Tapping the Material SearchView to focus it killed the catalog
(process death, fell back to launcher).
**Root cause.** On focus, SearchView registers a `DeviceConfig` change-observer →
`ContentResolver.registerContentObserver` → `getContentService()` =
`getService("content")`, but OHOS has **no "content" service** → null →
`IContentService$Stub.asInterface(null)` = null → `invoke-interface` on null →
NPE (only `RemoteException` was caught) → unhandled → death.
**Fix.** Smali null-guard in `framework.jar`
`android/content/ContentResolver.smali` — in
`registerContentObserver(Uri,Z,Observer,I)` add `if-eqz <service>, :return` after
`getContentService()` (and the symmetric guard in `unregisterContentObserver`).
DeviceConfig observers just don't register; OHOS has no ContentService to notify
anyway. BCP → boot regen. **This is the SAME `ContentResolver` patch noice uses**
— the committed smali is `framework-smali-patches/android_content_ContentResolver.smali`
(see the `:smfix_reg_ret` label in `registerContentObserver`; it serves both
apps). **Status: FIXED + VERIFIED** — Search focuses (cursor + suggestions), 5/5
taps survive, `FATAL/SIGSEGV/SIGABRT=0` (`docs/engine/V3-CATALOG-IME-FIX/`).

### H. IME bridge — Android IMM → OHOS InputMethodController  (bridge `9b2a9727` + helper `e4880759` + ohaf `4690cae1`)
**Symptom.** Even with the Search-focus crash fixed (§G), focusing a text field
summoned no keyboard.
**Root cause.** The adapter's `adapter.window.InputMethodManagerAdapter` (in ohaf,
registered as the `"input_method"` `IInputMethodManager$Stub`) was an intentional
no-op stub: `showSoftInput()`→false,
`startInputOrWindowGainedFocus()`→`InputBindResult.NO_IME`. So adapter apps were
told no IME is bound → the installed OHOS keyboard (`com.example.kikakeyboard`) was
never summoned.
**Fix (implemented + deployed):**
- **bridge** (`9b2a9727`): `bridge-src/input_method_bridge.cpp` registers the JNI
  natives `nativeShowKeyboard` / `nativeHideKeyboard` on
  `adapter/window/OhImeBridge`; on show, **lazily `dlopen("liboh_ime_helper.so")`**
  + dlsym `oh_ime_show` / `oh_ime_hide` / `oh_ime_set_vm`.
- **`liboh_ime_helper.so`** (`e4880759`): `bridge-src/oh_ime_helper.cpp` — the
  `OnTextChangedListener` 22-virtual ABI shim +
  `InputMethodController::Attach/ShowSoftKeyboard/HideSoftKeyboard`; links
  `libinputmethod_client.z.so`. Routes typed text →
  `OhImeBridge.nativeOn{InsertText,DeleteBefore,DeleteAfter,EnterAction}`.
- **ohaf** (`4690cae1`): `adapter.window.OhImeBridge`
  (`framework-smali-patches/catalog/OhImeBridge.smali` [+ `$1`]) — show/hide →
  natives; `nativeOn*` post to the UI-thread Handler → focused-view
  `InputConnection` via reflection
  (`WindowManagerGlobal.mRoots → ViewRootImpl.mView → findFocus →
  onCreateInputConnection → commitText/etc.`); plus 3 forwarding edits in
  `InputMethodManagerAdapter`. ohaf is BCP → boot regen.

**Two bring-up bugs (critical lessons — §gotchas):** (a) making
`libinputmethod_client.z.so` a **DT_NEEDED of the bridge** aborts the appspawn-x
**prefork** (its INIT_ARRAY + UBSan dep crashes libart) → must split into the
lazily-dlopen'd helper, never loaded into the prefork; (b) declaring
`register_InputMethodBridge` `extern "C"` (defined) vs C++-mangled (declared) left
it UND → bridge fails at `JNI_OnLoad`.

**Status: keyboard APPEARS + persists on a plain Text Field** — verified, full
OHOS QWERTY (`docs/engine/V3-CATALOG-SWEEP-2026-06-25/textfield-L3-typed.jpeg`).
On a SearchView it flashes then is torn down ~65 ms later by
`PerUserSession::OnFocused` because the adapter window doesn't hold durable OHOS
**WMS focus** (the §honest wall). **Text *entry* via synthetic input is
unconfirmed** — OHOS `uinput` keystrokes aren't bridged to
`InputConnection.commitText` (same wall as the ignored BACK key). A physical
keyboard is the untested real test.

---

## 4. Cold-boot reliability — the fontconfig O(n²) parse  [§font]

**Symptom.** ~25% (2/8) of cold boots: the first catalog launch freezes (AAFWK
`LIFECYCLE_TIMEOUT`), the watchdog kills it.
**Root cause (long-believed wrong).** **NOT** `AESKeyGenProbe` (a red herring — it
completes in ~15 ms; it was just the last named init marker before the freeze).
The real cause: `SkFontMgr::RefDefault()` (a per-fork singleton) parses the
**6.3 MB `/system/fonts/hm_symbol_config_next.json`** on the app's first
`android.graphics.fonts.Font.Builder.build()`; the deployed
`libskia_canvaskit.z.so` cJSON parser walks array items with `cJSON_GetArrayItem`
(O(index) linked-list walk) inside per-symbol loops → effectively **O(n²)** over
thousands of symbols. Under cold-boot 4-core contention it intermittently exceeds
the ~10 s watchdog. Caught with `dumpcatcher -p <catalogpid>` on the **live** hang
(the watchdog sysfreeze dump captures the wrong process).
**Fix.** Replace the 6.3 MB json with a structurally-valid minimal one (empty
`common_animations` / `special_animations` / `symbol_layers_grouping` arrays),
committed as **`config/hm_symbol_config_next.json`** (md5 **`425290bd`**, 144 B):
```bash
sh "mount -o remount,rw /
    cp /data/local/tmp/hm_symbol_config_next.json /system/fonts/    # preserves ctx system_fonts_file
    chmod 644 /system/fonts/hm_symbol_config_next.json"
```
No binary patch, no boot regen, fully reversible (back up the 6.3 MB original
first; orig md5 `6ed9f4d6`). Only decorative HM-symbol glyphs are lost; normal
text is unaffected.
**Verify.** 25% (2/8) → **0% (16/16)** bad-boot
(`docs/engine/V3-CATALOG-DEMO-READY/`). This is a **generic OHOS-app cold-boot
risk** (any fresh-fork app's first text render hits this parse), not
catalog-specific.

---

## 5. Performance (JIT/AOT gap + the logging trim)  [§perf]

The catalog is **interpreted + JIT** for its own dex (no app-AOT); only the
framework/boot-image is AOT (dex2oat `speed`). Proven 3 ways: no `oat/arm/*.odex`
for `/data/app` apps; `base.apk` maps `r--s` (raw dex, non-executable); a JIT
thread + `dalvik-jit-code-cache` + `libart-compiler.so` are present.

- **Naive app-AOT is a DEAD END here.** AOT-compiling `base.apk` with stock
  `dex2oat64 --compiler-filter=speed` against the deployed boot image crashes
  deterministically (SIGBUS in AOT-compiled jacoco after the W9/FIX-VTABLE-A
  relocations): the deployed libart **rewrites vtable slots at runtime**, but stock
  dex2oat compiled call sites against the *stock* vtable layout → wrong vtable
  indices after relocation → garbage ref → SIGBUS. 312 UI classes get relocated →
  broad, not excludable. `speed` AOT would only work if dex2oat were rebuilt from
  the same W-fixup libart source. The safe `--compiler-filter=verify` deploys
  cleanly but gains only ~6 % warm start (noise).
- **The cheap safe win (APPLIED — libart `ba40f173`).** The custom libart was
  emitting ~43,000 lines of per-class checkpoint logging per launch
  (`[*_CP]` + `[FIX-VTABLE-A-RELOC]`), each a synchronous `fflush(stderr)` — 1014+
  blocking file writes during class loading. Gated the FIX-VTABLE-A reloc/summary
  logging behind `constexpr kLogVtableFixup = false` and commented the 62
  single-line `[*_CP]` checkpoints (`// [PERF-2026-06-25 gated]`). Per-launch
  child stderr **43,000 → ~800 lines** (~98 % fewer synchronous writes); catalog
  still `drew=1`, 0 crash (logic untouched). The committed patch is
  **`libart-patches/class_linker.cc.perf-logging-trim.diff`** (124 changed lines,
  vs the pre-trim `2813065e` source). Wall-clock gain is modest — cold start is
  dominated by the appspawn-x prefork + the vtable-fixup work itself, not the
  logging; the real cold-start lever (a warm appspawn-x zygote) is not done.

Cold start: warm relaunch ≈ **3.7 s** to first draw; the first-after-boot launch
is ~25–30 s = prefork-from-cold-cache + the W-series vtable fixups (312 classes /
1014 relocations) at class-link time, which AOT would not remove.

---

## 6. Boot-image regeneration (the boot-regen cycle)  [§boot-regen]

**Required after ANY byte change to ANY BCP jar** — even if the jar list/order is
unchanged (dex2oat bakes cross-jar layout offsets; new bytes → new offsets →
load-time `Class mismatch`). The catalog BCP fixes that need a regen: §B
(arb metaData), §C/§G (framework.jar), §H (ohaf IME). §D/§E (libart) and §font do
**not** need a boot regen; §A/§2/§F are not BCP.

The **10-jar BOOTCLASSPATH** order (Scope C, baked into `appspawn-x`'s
`kBootClasspath`):
```
1 core-oj  2 core-libart  3 core-icu4j  4 okhttp  5 bouncycastle
6 apache-xml  7 adapter-mainline-stubs  8 framework
9 adapter-runtime-bcp        <- arb BEFORE ohaf so it SHADOWS ohaf (first-jar-wins)
10 oh-adapter-framework
```
- **First-jar-wins.** BCP class resolution picks the FIRST jar containing a class.
  Scope C deliberately puts `adapter-runtime-bcp` before `oh-adapter-framework` so
  arb's `PackageInfoBuilder` (§B) "wins". **Patching a shadowed copy does nothing**
  — patch the *definer* (`unzip -p <jar> classes.dex | grep -ac '<Class>'` to find
  it; iterate `classesN.dex`).
- **Two places MUST agree:** the runtime `kBootClasspath` (in `appspawn-x`) and the
  dex2oat JARS list. A mismatch yields a loadable-but-mislaid boot image that
  aborts at load with `runtime.cc … Class mismatch for L<class>;`. Confirm the live
  order: `strings /system/bin/appspawn-x | grep 'framework/.*\.jar'`.

Regen all 30 segments with the host `dex2oat64` (OAT 230, `--base=0x70000000`,
`--instruction-set=arm`, `--compiler-filter=speed`): stage all 10 deployed jars,
swap in only the patched one, run dex2oat over all 10 in the order above
(produces `boot.{art,oat,vdex}` + `boot-<jar>.{art,oat,vdex}` for 9 jars = 30
files in ~14–16 s). Send all 30 + the patched jar to
`/system/android/framework/arm/` + `/system/android/framework/`, chcon each boot
segment `system_lib_file:s0`, `rm -rf /data/misc/appspawnx/dalvik-cache/*`,
**reboot**. The repo wrapper is referenced in `docs/REPRODUCTION-GUIDE.md` §4.5.

**Brick-safety:** byte-compare a regenerated `boot-framework.oat` against the
deployed one when only an unrelated jar changed — they should match, proving your
dex2oat pairs with the deployed libart. libart and the boot image are a dex2oat
**pair** — regen the boot whenever you change libart's class-layout assumptions
and deploy them together.

---

## 7. Demo setup (hands-off, no laptop)  [§demo]

1. **Persistent Permissive SELinux.** The board boots **Enforcing** by default;
   the catalog `normal_hap` domain is then denied the adapter access it needs (avc:
   getattr `/system/android`, search `/data/misc` + `/data/local/tmp`,
   dac_override) → a tap does nothing. Fix:
   `mount -o remount,rw /` → edit `/system/etc/selinux/config`
   `SELINUX=enforcing` → `SELINUX=permissive` (back up first). OHOS honors it: the
   board boots `enforce=0` on its own (no `setenforce`), and the `ondemand`
   appspawn-x auto-spawns on the icon-tap — **no manual bring-up**. Details +
   one-liner: `notes/selinux-permissive.md`. Revert by restoring the backup.
2. **Cold-boot reliability** — apply §4 (fontconfig) so the first launch never
   freezes.
3. **Flow:** power on → board boots Permissive → **swipe up to unlock**
   ("上滑解锁", no PIN) → **tap the Material Catalog icon** (launcher row 2,
   ~x500 y320) → wait ~25–30 s (vtable fixups + dex linking are slow; a
   semi-transparent loading window shows first) → the Material 3 grid paints →
   navigate. The first tap right after unlock can miss (launcher settles ~2 s) —
   tap again.
4. **Mock battery** — the board is **DC-powered with NO battery hardware**
   (`/sys/class/power_supply/` is empty); any "low battery / 11%" warning is
   **fake** and **rebooting is always safe**. Set a level for screenshots:
   `hidumper -s 3302 -a "--capacity 95"`.

Evidence of the full flow:
`docs/engine/V3-CATALOG-DEMO-READY/{02-coldboot-unlock-launcher-catalog-icon,03-tap-icon-catalog-renders}.jpeg`.

---

## 8. Verification

In order:
1. **appspawn-x up:** after boot (Permissive), `ps -A | grep appspawn-x`; on
   launch, the child stderr
   `/data/service/el1/public/appspawnx/adapter_child_<pid>.stderr` reaches the
   event loop with **0** of `mark_sweep | Fatal | cppcrash | Class mismatch |
   ValidateOatFile | InitWithoutImage`.
2. **Registration:** `bm dump -n io.material.catalog` shows `bundleType 10`
   (APP_ANDROID), codePath `…/android`, MainActivity + abilities, userId 100 — and
   survives a reboot.
3. **Renders:** `aa start -a io.material.catalog.main.MainActivity -b io.material.catalog`
   → child stderr `drew=1 width=720 height=1280`, process stays alive
   (`docs/engine/V3-CATALOG-DREW1-EVIDENCE/`). Catalog pid name truncates to
   `io.material.cat`; `pidof` FAILS (empty cmdline) → use `ps -A | grep material`.
   Catalog uid is **16371**.
4. **The 32-category sweep:** drive `uinput -T -c x y` taps (or direct-launch the
   activity demos `aa start -a <ability>`), capture `snapshot_display -f X.jpeg`;
   expect all 32 categories navigable to L3, every widget type driven with a
   visible result, Date Picker L5 modal interaction, **0** tombstone/cppcrash/
   Fatal-signal (`docs/engine/V3-CATALOG-SWEEP-2026-06-25/COVERAGE.md`).
5. **Cold-boot demo flow:** from a cold power-on, swipe-unlock → tap the icon →
   grid paints, first pid stable, sysfreeze == 0 — repeat ≥8× (expect 0 bad-boots
   with the fontconfig fix).
   - A "drew=1 / newest-stderr" check FALSE-PASSES a bad-boot (the watchdog kills
     the frozen 1st launch, AMS respawns a drawing 2nd) — verify the **FIRST pid is
     stable + sysfreeze count == 0**.
   - Direct-launch (`aa start -a <ability>`) composites more reliably than
     fragment-tap for the 19 activity-backed demos. Fresh composite ≈ 40–130 KB;
     stale / launcher ≈ 20–34 KB.

---

## 9. ★ Critical gotchas (read before deploying)  [§gotchas]

### Brick / device-loss footguns (survive reboot)
- **NEVER add `critical` to a fail-prone init service.** `"critical":[…]` in
  `appspawn_x.cfg` makes init *reboot* the device if the service fails; with
  `start-mode:boot` that is before recovery → **bootloop brick** → the USB
  endpoint disappears → reflash + full wipe. **This happened.** Ship appspawn-x
  **non-critical** (`"critical":[]`, `"start-mode":"ondemand"`) and reboot
  **manually** after staging (no `--reboot` from a deploy script).
- **NEVER `param set persist.sys.usb.config none` (or to *anything*).** It
  disables the USB gadget → **hdc-over-USB dies on the next reboot**, and because
  it's `persist.` the reboot does NOT undo it → the user must physically toggle USB
  debugging on-device. This broke the link 2–3 times. The safe value is
  `hdc_debug`. To dismiss the connection-mode popup:
  `param set persist.usb.setting.gadget_conn_prompt false`. **If you delegate any
  device work, put this prohibition as the literal first line of the brief.** Full
  detail: `notes/device-safety.md`.

### Diagnosis traps (the visible symptom is downstream of the cause)
- **The bad-boot is NOT `AESKeyGenProbe`** — it's the 6.3 MB fontconfig O(n²)
  parse (§4). The probe completes in 15 ms. Don't nop it expecting a fix.
- **Sysfreeze / watchdog stack dumps LIE** — they capture the downstream zombie or
  the wrong process. For an AAFWK `LIFECYCLE_TIMEOUT`, get the stack with
  `dumpcatcher -p <live-pid>` on the **actual** hung process (poll ~1–2 s after
  launch, before the ~10 s kill).
- **R8-inlined stacks hide root causes** — inlining collapses distinct bugs into
  one `method:Unknown Source:NNN`. Record `:NNN` for every frame; if it *shifted*
  after a fix, you fixed a symptom and unmasked a different bug.

### JNI / bring-up traps
- **Verify JNI signatures against the on-device framework smali, not AOSP
  source.** Signatures drift across AOSP versions; a wrong signature makes
  `RegisterNatives` return **-1 silently** for the whole table →
  `NoSuchMethodError` on first call.
- **Never make an inputmethod/UBSan-bearing `.so` a DT_NEEDED of the bridge** —
  dlopen it lazily from the forked app (§H). A crashing `.so` loaded into the
  appspawn-x prefork poisons **all** subsequent forks until **reboot** — reboot
  between `.so` swaps when isolation-testing.
- **Java logging is dead in the app process.** `Log.i` / `System.err` from the
  catalog don't reach the child stderr — only native `fprintf` does. To observe
  Java behaviour, write to a **pre-created 0666** file.

---

## 10. Honest limitations — the two foundational walls

Neither is a per-app catalog bug; both are adapter-level and the highest-leverage
remaining items.

1. **Synthetic input → `InputConnection` is not bridged.** OHOS `uinput` /
   synthetic keys (and the BACK key, `uinput -K -d 2`) don't reach the Android
   `InputConnection.commitText`. So IME **text-entry via injection is
   unconfirmed** — the keyboard *appears* and the field *focuses* (cursor +
   placeholder), but typed characters don't commit through any synthetic path
   tried (`uinput -K` keycodes, `uinput -t "..."`, tapping on-screen key
   positions). The catalog renders as ONE OHOS surface, so its internal Android
   `EditText` is invisible to OHOS `uitest dumpLayout` too. A **physical** keyboard
   tap (genuine touch→IME→InputConnection path) is the untested real test; the IME
   window + focus path it would use is proven. Same family as the ignored BACK key
   and the noice VelocityTracker wall.
2. **Adapter app windows render on top but don't hold OHOS WMS focus.**
   `EntryView`/SceneBoard (the launcher) keeps focus even though the adapter calls
   `RequestFocus(windowId)`. Consequence: some modals/popups composite
   intermittently per boot, and focus-sensitive views (SearchView) get their
   keyboard torn down ~65 ms after it shows (`PerUserSession::OnFocused`). The
   plain Text Field keyboard *does* persist; the Date Picker / AlertDialog /
   nav-drawer / side-sheet *do* composite and are drivable — it's per-instance
   flaky, not a hard failure. Fixing durable WMS focus for adapter windows is the
   single highest-leverage item (a deep window-manager-adapter / SceneBoard
   focus-arbitration change).

A related cosmetic artifact: **OHOS recents shows zombie catalog entries** (names,
no thumbnails). Cause: mission persistence is stubbed on this build
(`mission_data_storage.cpp` is a no-op), the catalog ability is STANDARD/multiton
(every launch = a new mission), and the WMS-focus/compositing wall means the
window never sets `firstFrameAvailable_` so no snapshot is captured. Cleanup:
recents "Clear All" or a full reboot (in-memory only). Prevention: set the ability
`launchMode` to `singleton` in the `entry.hap` `module.json`. Neither restores
thumbnails (needs un-stubbing persistence AND fixing compositing).

---

## 11. Across both apps

What works end-to-end across both: the catalog (this file) and noice
(`REPRODUCE.md`). The two walls above (synthetic-input→InputConnection,
WMS-focus) are shared, adapter-level, and not per-app. The full self-contained
catalog write-up — with the architecture, the from-scratch build, every fix in
depth, and the evidence-directory index — is `docs/REPRODUCTION-GUIDE.md`.
