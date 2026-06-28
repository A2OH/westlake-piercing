---
name: catalog-newboard-working-2026-06-27
description: NEW board — io.material.catalog INSTALLED + RENDERS the Material 3 grid (drew=1); the full from-scratch recipe + the matched-libart-gen + /system/lib lessons
metadata: 
  node_type: memory
  type: project
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

2026-06-27: On the **new board** (the noice bring-up board), **io.material.catalog now installs + renders the Material 3 grid** (drew=1 720x1280, foreground, screenshot `catgrid2.jpeg`: Material3 header+search+gear, grid Adaptive/Bottom App Bar/Bottom Sheet/Buttons/Cards/Carousel). Goal-half DONE.

**★ Registration WAS the summary's "unsolvable BMS" blocker — SOLVED via the documented `.app→.apk` 1-byte patch.** `libappexecfwk_common.z.so` @ offset 0x1a40 has `.app` (0x70='p'); patch → `.apk` (0x6b='k') = md5 **4d2c6399** (host `/mnt/c/Users/<user>/Dev/ohos-tools/deploy10/libappexecfwk_common.apk-patched.z.so`). Deploy to `/system/lib/platformsdk/libappexecfwk_common.z.so` (the one BMS/foundation loads) + chcon system_lib_file + **reboot** (foundation reloads it). THEN `bm install -p /data/local/tmp/catalog.apk` → "install bundle successfully" (bundleType 10 APP_ANDROID, codePath .../android). `bm install` WIPES the bundle dir → redeploy entry.hap AFTER.

**★ The catalog needs a MATCHED framework+libart GENERATION — a mix NPEs.** Verified-good pairs: **f5fd86ef + libart 2813065e** (the drew=1 set) OR e6f9e1a3 + 42d2d8e8 (catalog-reflash demo). MIXING f5fd86ef + 42d2d8e8 → `CatalogApplication.onCreate:50` `NullPointerException ImmutableList.iterator()` in the Dagger DI build (DaggerCatalogApplicationComponent.build/inject — a null multibinding). Swapping libart→2813065e (keeping f5fd86ef) cleared it → drew=1.

**★ libart loads from `/system/lib/libart.so`** (verified /proc/PID/maps) — NOT /system/android/lib (deploys there = no-ops). libhwui loads from **/system/android/lib/libhwui.so**. Different paths per lib. [[noice-libart-path-and-arity-wall-2026-06-27]]

**★ The drew=1 BOOT (5e8566a6, host `/mnt/c/Users/<user>/Dev/ohos-tools/fixboot/`) is a FULL different gen → its other boot segs MISMATCH this board's appspawn-x BCP → appspawn-x ABORTS (Class mismatch, phase4=0). DON'T deploy the drew=1 boot.** Instead: keep this board's appspawn-x-compatible boot = the regen (`regen_boot.sh` in V3-5APP-V2-EVIDENCE, build dir /tmp/5app-v2-build, jars f5fd86ef framework + c026e80c bcp). libart 2813065e loads that regen boot fine (phase4=1). boot-framework.oat happens to be 5e8566a6 in both (deterministic from f5fd86ef) but the other segs differ.

**EXACT WORKING SET ON THIS BOARD (all deployed, validated drew=1):**
- libart **2813065e** (host `/mnt/c/Users/<user>/Dev/ohos-tools/libart-w22.so`) → `/system/lib/libart.so`
- framework **f5fd86ef** (host `/mnt/c/Users/<user>/Dev/ohos-tools/framework.jar`) → /system/android/framework/
- adapter-runtime-bcp **c026e80c** (host `…/ohos-tools/catbak/adapter-runtime-bcp.jar.c026e80c`) → /system/android/framework/
- regen boot (f5fd86ef + c026e80c) → /system/android/framework/arm/
- libhwui **1d04a56e** (host `$HOME/bridge-build/out/aosp_lib/libhwui.so`) → /system/android/lib/libhwui.so
- fontconfig **425290bd** (host `…/ohos-tools/catbak/hm_symbol_config_next.json.425290bd`) → /system/fonts/
- catalog APK **a9df5518** (`catalog-base.apk`) bm-installed; entry.hap (catalog-entry.hap) at bundle dir
- appspawn-x (this session's 10-jar build) + start_asx.sh bringup (Phase 4) + `aa start -b io.material.catalog -a io.material.catalog.main.MainActivity`

**Quirks:** battery 11% → aggressive lockscreen covers the catalog even though it drew=1 (foreground) — unlock (power-shell wakeup+timeout + swipe-up uinput) BEFORE launching to see the grid; `ps|grep material` unreliable (name truncates io.material.cat) — confirm via `drew=1` in hilog + child stderr + AbilityTransitionDone state:5. backups on device /data/local/tmp/{libart-42d2d8e8.bak, libart.pre-perftrim..., libhwui.pre-catalog.bak, hm_symbol.pre-catalog.bak}.

**noice still BLOCKED** (different need): its deepest is on libart 42d2d8e8 (the Function2-arity wall) — that conflicts with the catalog's 2813065e, and the arity fix needs 42d2d8e8's source (ECS /data/aosp). See [[noice-libart-path-and-arity-wall-2026-06-27]].
