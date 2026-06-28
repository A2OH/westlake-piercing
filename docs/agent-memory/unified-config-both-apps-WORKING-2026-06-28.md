---
name: unified-config-both-apps-working-2026-06-28
description: "★★★ ONE unified adapter config runs BOTH catalog AND noice on the new board (both launch + navigate, validated). The libart-branch + ContextImpl-CM-graft + bpfgrant solution. Supersedes the catalog/noice 'conflict'."
metadata:
  node_type: memory
  type: project
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

2026-06-28: **GOAL MET — a SINGLE unified config runs BOTH apps on the new board, both validated by navigation test.** Catalog: Material3 grid → Buttons detail (back works). noice: 声音库/Sound Library → 闹钟/Alarm → 账户/Account (5-tab bottom-nav routes between distinct fragments, no crash). Screenshots: scratchpad `noice_uni.jpeg`, `noice_alarm.jpeg`, `noice_profile.jpeg`, `catalog_uni.jpeg`, `catalog_buttons_uni.jpeg`.

## THE UNIFIED CONFIG (all deployed + validated)
| component | md5 | path | notes |
|---|---|---|---|
| libart | **7b856a2d** | /system/lib/libart.so | serves BOTH (host `/mnt/c/.../ohos-tools/libart.so` + `westlake-baseline/lib/libart.so`) |
| framework | **1c334902** | /system/android/framework/framework.jar | = f5fd86ef + ContextImpl graft (host backup `/mnt/c/.../ohos-tools/framework-uni-1c334902.jar`) |
| boot-framework.oat | **4376897e** | /system/android/framework/arm/ | regen, paired with the unified framework |
| adapter-runtime-bcp | c026e80c | …/framework/ | catalog metaData fix |
| adapter-mainline-stubs | 41834c1f | …/framework/ | has the stub `android/net/ConnectivityManager` + SSLSockets |
| oh-adapter-framework | 300581d1 | …/framework/ | |
| runtime | 16e08711 | /system/android/lib/liboh_android_runtime.so | the noice linchpin (NOT lost — in westlake-baseline) |
| libhwui | 1d04a56e | /system/android/lib/ | |
| bridge | 60126181 | /system/lib + /system/android/lib | |
| fontconfig | 425290bd | /system/fonts/hm_symbol_config_next.json | cold-boot anti-hang |

Device boot backups: `/data/local/tmp/uni-bak/` (framework.jar.f5fd86ef + arm-catalog-regen/), `/data/local/tmp/libart.live-2813065e.bak`.

## WHY ONE CONFIG WORKS — the two "conflicts" were both myths
1. **libart: the noice Function2-arity wall was a `42d2d8e8`-branch artifact, NOT noice's real need.** noice's PROVEN old-board libart was **7b856a2d** (catalog branch). On 7b856a2d the arity wall is GONE. The reflashed board shipped 42d2d8e8 and the /system/lib path bug silently dropped my 7b856a2d deploys → I was fighting the wrong branch. **Do NOT chase 42d2d8e8 source.**
2. **libart W22-PROXY-SKIP (in `2813065e`) BROKE noice.** On 2813065e noice cleared arity but died at `AnnotationFactory.invoke→$Proxy4.value()` `IllegalArgumentException: ...Object.clone()` (the "clone-shadow") in AndroidX-Navigation reading `@Navigator.Name` → FragmentContainerView InflateException @MainActivity:93. W22 skips vtable fixup for ALL proxy classes incl. annotation proxies → broke clone() dispatch. **7b856a2d (pre-W22) serves both**; W22 was only a catalog PERF opt (LinkMethods O(n²)), not correctness — catalog renders fine on 7b856a2d (state:5, no hang; it ran on 275eb104=7b856a2d-branch before).
3. **framework: catalog `f5fd86ef` and noice `8524dc56` differ ONLY in `android/app/ContextImpl.smali`.** noice's MainActivity does `getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager` (Kotlin non-null) → catalog fw returns null → NPE. noice fw adds a fallback in `getSystemService(String)` + `getSystemServiceName(Class)`: name=="connectivity"→`new ConnectivityManager()` (stub lives in adapter-mainline-stubs, safe), and "jobscheduler"→`new JobSchedulerStub()`. Catalog's fw only null-GUARDS its own proxy block (insufficient for app code). **Unified fw = graft 8524dc56's ContextImpl.smali + JobSchedulerStub.smali into f5fd86ef** (pure-add diff, catalog's metaData is in the bcp not the fw, so no catalog regression). SystemServiceRegistry/ContentResolver-guard are byte-identical in both.

## REBUILD the unified framework + boot (recipe)
- apktool 2.9.3 (`$HOME/apktool.jar`) `d` decode (its bundled baksmali Main has no standalone entrypoint — use `apktool d`/`b`, NOT `java -cp ... baksmali.Main`).
- decode f5fd86ef + 8524dc56 (`/mnt/c/.../ohos-tools/bcpjars/framework.jar`); `cp` 8524's `smali/android/app/ContextImpl.smali` + `smali/android/app/job/JobSchedulerStub.smali` into f5's tree (both in classes.dex/`smali/`); `apktool b` → framework.jar (1c334902).
- Boot regen: `$HOME/openharmony/docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh` (WORK=/tmp/5app-v2-build; out/aosp_fwk = core-oj/core-libart/core-icu4j/okhttp/bouncycastle/apache-xml + the unified framework.jar; out/adapter = adapter-mainline-stubs/adapter-runtime-bcp/oh-adapter-framework). Pull the 10 live BCP jars from device to guarantee consistency. dex2oat64 `$HOME/tools/dex2oat64`.
- Deploy: overwrite IN PLACE (`cat tmp/file > /system/.../file`) to preserve SELinux ctx; **DANGER: if the source push silently failed, `cat` ZEROES the live file** — always verify md5 of the pushed tmp file BEFORE the cat. Restart appspawn-x (start_asx.sh) to reload boot+fw+libart (no full reboot needed — only appspawn-x children use the AOSP boot image).

## noice NETWORK — the launch crash was the documented INET-GID gate (see [[noice-network-inet-gid-fix]])
After the fw graft, noice RENDERED then crashed on a bg OkHttp thread: `FATAL ... SecurityException: missing INTERNET permission → getaddrinfo EPERM`. Fix = the eBPF socket grant: noice uid=**13731**; `libsetgidhook` (in start_asx.sh LD_PRELOAD) already adds inet gid 3003 (setgid.log `out=5`); the missing piece was **`/data/local/tmp/bpfgrant 13731 oh_sock_permission_map`** (host `westlake-baseline/tmp/bpfgrant` → set uid 13731 val=1 in map id=18; BMS had it =0=no-internet). After grant noice no longer crashes — it shows its own graceful "网络无法访问/Retry" offline UI and the bottom-nav fully navigates. (Live library fetch still blocked by the deeper INetConnService-code-11 / connectivity-flow issue — out of scope; app is WORKING + navigable regardless.) **bpfgrant must be (re)applied per boot, ideally in a loop during launch (netsys may reset).**

## Bringup — NOW AUTOMATIC ON BOOT (✅ 2026-06-28, survives reboot, validated)
**The white-screen-on-tap-after-reboot is FIXED** with a durable init autostart. Files (all in /system, deployed):
- `/system/bin/asx_boot.sh` (system_file ctx) — mirrors start_asx.sh: setenforce 0, wait for `foundation` up, `pkill appspawn-x`, mkdir /dev/memcg/perf_sensitive, export APPSPAWNX_NO_JIT + the 6-lib LD_PRELOAD, `setsid appspawn-x …` (backgrounded → reparents to init), wait for socket then `chmod 0666`+`chcon appspawn_socket` it, then a bg loop `bpfgrant 13731 oh_sock_permission_map` ×150 @2s (covers the launch window; netsys may reset). Logs `/data/local/tmp/asx_boot.log`.
- `/system/bin/bpfgrant` (copied from /data/local/tmp so it survives /data wipes).
- `/system/etc/init/zz-asx-autostart.cfg` (system_etc_file ctx) — **non-critical oneshot** service `asx-autostart` (`"start-mode":"condition"`, `"once":1`, `path:["/system/bin/sh","/system/bin/asx_boot.sh"]`) started by a **`post-fs-data` job** (`"start asx-autostart"`).
**★ WHAT MADE IT WORK (3 things, each load-bearing):** (1) **`secon":"u:r:su:s0"`** — runs the script in the SHELL domain (like the manual hdc bringup), so the sh→appspawn-x exec does NOT domain-transition to `appspawn` → **AT_SECURE stays 0 → LD_PRELOAD is honored** (verified: `grep -c setgidhook /proc/$(pidof appspawn-x)/maps` = 4; child setgid.log `out=5`). hdcd.cfg is the only other su-domain service — it's a valid service domain. (2) **`start-mode":"condition"` + a `post-fs-data` job** (mirrors the proven `check_module_update.cfg`) — a bare `"boot"` job with NO start-mode silently never ran (asx_boot.log stayed stale = service never started). (3) **non-critical oneshot** = cannot bootloop (worst case = white screen, recoverable), so it's safe on this brick-history board.
After reboot: wait ~30–60s (script waits for `foundation` then starts appspawn-x), then tap the icon — noice/catalog launch cold, no white screen. Manual fallback if ever needed: `setsid sh /system/bin/asx_boot.sh </dev/null >/dev/null 2>&1 &`. To revert: `rm /system/etc/init/zz-asx-autostart.cfg`. Host backups of all three in `/mnt/c/.../ohos-tools/{asx_boot.sh,zz-asx-autostart.cfg}`. The OLD `appspawn_x.cfg` ondemand service still fails on tap (writepid `/dev/memcg/perf_sensitive/cgroup.procs` missing + no LD_PRELOAD) — harmless; our autostart's appspawn-x owns the socket first.
Both apps share the WMS-focus oscillation (cosmetic; keyguard/launcher steals foreground) — unlock + relaunch to foreground.

## noice first-run WIZARD was un-clickable → BYPASSED (✅ 2026-06-28)
On a fresh launch noice sometimes shows its onboarding wizard (`AppIntroActivity`, "欢迎/Welcome") instead of the Sound Library — a RACE on the intro-flag read. **That wizard is a 2nd-level Activity → hits the adapter input-routing/WMS-focus wall: uinput taps (Skip / › / swipe) are NOT delivered → stuck/un-clickable.** The MainActivity (Sound Library) itself IS tappable (bottom-nav Library/Alarm/Account proven). FIX = skip the intro: noice's `MainActivity` reads `getSharedPreferences(<pkg>_preferences).getBoolean("has_user_seen_app_intro", false)` (plain SharedPreferences, NOT DataStore — found in smali MainActivity line ~604/216). Wrote `/data/app/el2/0/base/com.github.ashutoshgngwr.noice/shared_prefs/com.github.ashutoshgngwr.noice_preferences.xml` = `<map><boolean name="has_user_seen_app_intro" value="true" /></map>` (chown 13731:13731, chcon `u:object_r:appdat:s0`; force-stop noice first). → noice now boots STRAIGHT to MainActivity (only MainActivity AbilityTransitionDone, no AppIntroActivity), navigable. Persists across reboot (el2 /data). General lesson: any adapter app gated by a first-run/2nd-Activity wizard that won't accept touch → pre-seed its "seen" pref/flag to skip it rather than fight the input wall. The deeper 2nd-Activity input routing remains the unsolved foundational wall (same as catalog's separate-Activity demos). Related: [[catalog-newboard-WORKING-2026-06-27]], [[noice-libart-path-and-arity-wall-2026-06-27]] (NOW SOLVED — arity was a 42d2d8e8 artifact), [[battery-power-not-relevant]].
