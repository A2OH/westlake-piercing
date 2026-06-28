---
name: catalog-reflash-restore-2026-06-26
description: "The DAYU200 /system got reflashed overnight (wiping the catalog demo: libs+fontconfig reverted, catalog bundle unregistered, Enforcing) but framework/boot/appspawn-x + the .app→.apk patch + /data all SURVIVED; full RESTORE recipe (all artifacts on /data+host) that brought the demo back to verified DEMO-READY — re-run if it reverts again"
metadata:
  node_type: memory
  type: project
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

2026-06-26: after the catalog demo-prep was done + validated, the board's **/system got REFLASHED** (overnight, cause unconfirmed — likely a user reflash during the USB-link recovery). It reverted **/system/android/lib/** + **/system/fonts/** + the **bundle DB**, but left framework/boot/appspawn-x + /data intact.

**What the reflash REVERTED:** libhwui→`65f81263`, libart→`42d2d8e8`, bridge→`21b68190`, fontconfig→original `6ed9f4d6`; catalog bundle **unregistered** (bm dump lost it; bundle DIR + base.apk survived on /data); SELinux back to **Enforcing** (live), clock reset, `getprop` (android tool) absent.
**What SURVIVED (do NOT need restore):** framework.jar `e6f9e1a3`, adapter-runtime-bcp `c026e80c`, boot-framework.oat `290e4499`, appspawn-x `3abe3bde` (running), **.app→.apk patch libappexecfwk_common `4d2c6399`**, `/system/etc/selinux/config`=`permissive` (applies on boot), and ALL of /data (the catalog APK, the entry.hap copy, the agent backups).

**RESTORE RECIPE (verified working; re-run if it reverts):**
1. `mount -o rw,remount /` (there is NO separate /system mount; / is rw).
2. Deploy the 4 reverted components (chcon after each):
   - libart `275eb104`: `cp /data/local/tmp/libart-275eb104.so /system/android/lib/libart.so` → `chcon u:object_r:system_lib_file:s0`
   - bridge `9b2a9727`: `cp /data/local/tmp/demo-prep-bak/bridge.so.orig-9b2a9727` → **both** `/system/lib/liboh_adapter_bridge.so` AND `/system/android/lib/liboh_adapter_bridge.so` → `chcon system_lib_file`
   - fontconfig `425290bd`: `cp /data/local/tmp/demo-prep-bak/hm_symbol_config_next.json.orig-425290bd /system/fonts/hm_symbol_config_next.json` → `chcon u:object_r:system_fonts_file:s0`
   - libhwui `1d04a56e`: host `$HOME/bridge-build/out/aosp_lib/libhwui.so` → stage to `C:\Users\dspfa\Dev\ohos-tools\` → `/tmp/h file send` → `cp` to `/system/android/lib/libhwui.so` → `chcon system_lib_file`
3. Re-register catalog: `bm install -p /data/local/tmp/catalog.apk` (works via the surviving .app→.apk patch; it prints a non-fatal "internal error" code 9568260 but DOES register — confirm via `bm dump -a | grep material`).
4. Launcher icon (entry.hap, wiped by reflash → blank icon): `cp /data/local/tmp/catalog-entry.hap` (== host `/tmp/catalog-icon/entry.hap`, md5 `c43d81c9`) → `/data/app/el1/bundle/public/io.material.catalog/entry.hap`; `chown installs:installs`; `chmod 644`; `restorecon` (→ `u:object_r:data_app_el1_file:s0`); then **clear** `/data/app/el1/100/database/com.ohos.launcher/phone_launcher/rdb/Launcher.db*`.
5. **Reboot.** Comes up **Permissive** on its own (config persists). Catalog cold-launches + renders the M3 grid; launcher shows the Material Catalog logo + "Material…" label.

**VERIFIED DEMO-READY (2026-06-26):** both crash-fixes PASS (Date Picker calendar — libart 275eb104 W9; Adaptive List View — libhwui 1d04a56e), widgets + X-close + BACK all pass, single pid, 0 crashes, config persisted across 2 reboots. Backups: `/data/local/tmp/pre-redeploy-bak/` (the reverted state) + `/data/local/tmp/demo-prep-bak/` (pre-fix).

**Presenter demo:** power on → swipe-up unlock (no PIN) → pre-warm (tap catalog once, ~7s) → tap **Material Catalog** icon → navigate; pause ~2-3s between taps; close via X (top-left) or BACK; skip Adaptive sub-demos; don't type. Cold ~7s / warm ~3.8s.
**Quirks:** `uinput` taps do NOT register on the launcher (MMI limit) — verify launch via `aa start`, the user's FINGER tap works. Device loses `awk` after reboot (parse host-side). Screenshot: prime RenderService → device-sleep → snapshot_display → recv via `C:\Users\dspfa\Dev\ohos-tools\`. The **Noice launcher icon is still wrong** (shows the Material logo — separate deferred issue). Related: [[catalog-perf-jit-aot-findings]], [[catalog-badboot-is-fontconfig-not-aeskeygenprobe]], [[adapter-launcher-icon-entryhap-fix]], [[adapter-bootloop-wipe-recovery]].
