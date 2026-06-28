---
name: adapter-app-launch-bringup
description: How to launch adapter (Android) apps on OHOS so they render (appspawn-x bringup recipe); CRITICAL never set persist.sys.usb.config none (kills hdc); cold-boot auto-start memcg fix
metadata: 
  node_type: memory
  type: project
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

Launching an adapter (Android) app (e.g. io.material.catalog) on the OHOS launcher so it RENDERS:

**Blank-screen cause = appspawn-x (the adapter app-spawner) NOT running.** Launcher tap → AMS → "routing to appspawn-x for Android app" → spawns the Android child. If appspawn-x isn't up, spawn fails (`NotifyStartProcessFailed` / `sendOhBinaryResponse … Broken pipe`) → semi-transparent blank start-window. The catalog itself is fine: with appspawn-x up it renders the Library screen (drew=1), and the date-picker/morph/icon fixes are intact.

**Reliable in-session bring-up:** `setenforce 0` (device boots ENFORCING; appspawn-x run from su/hdc domain needs Permissive) → `setsid /system/bin/sh /data/local/tmp/start_asx.sh` (wait for `Listening on /dev/unix/socket/AppSpawnX … Entering event loop`) → `aa start -a io.material.catalog.main.MainActivity -b io.material.catalog`. Saved as `/data/local/tmp/catalog_up.sh`.
**GOTCHA: never run `setenforce 0` in the SAME hdc shell command as a `kill -9 appspawn-x/io.material` loop** — killing those disrupts the hdc shell (output lost, setenforce doesn't stick, command "returns nothing"). Run `setenforce 0` ALONE, verify `getenforce`=Permissive, THEN start_asx.sh.

**★★ CRITICAL MISTAKE — NEVER REPEAT: `param set persist.sys.usb.config none`** (used to dismiss the USB "USB连接方式" 传输文件/仅充电 dialog) DISABLES hdc-over-USB on the next reboot (persist → charge-only). Device becomes UNREACHABLE: `hdc list targets`=[Empty] (only UART COM3-7 show; `-t COMx` = "Device not found"). Recovery needs a PHYSICAL power-cycle + the user toggling USB-debugging on-device → sets `persist.sys.usb.config=hdc_debug`. Keep it = **hdc_debug**. To dismiss the USB dialog, tap 确定 (uinput) — do NOT touch persist.sys.usb.config.

**Cold-boot auto-start (tap works with zero commands):** the cfg `ondemand:true` auto-start is unreliable (didn't auto-start this session). ROOT CAUSE found: the `/system/etc/init/appspawn_x.cfg` appspawn-x SERVICE has `writepid:["/dev/memcg/perf_sensitive/cgroup.procs"]` but `/dev/memcg/perf_sensitive` was MISSING (start_asx.sh `mkdir -p`'s it as its "G5" fix; the cfg jobs did NOT) → init never starts the service (parent_appspawnx.stderr stays stale = no boot exec; pidof appspawn-x empty post-boot). FIX: added `mkdir /dev/memcg/perf_sensitive 0711 root system` to the cfg **init** job + changed service `ondemand:true`→`start-mode:"boot"` (eager start in u:r:appspawn:s0, may spawn under Enforcing w/o global Permissive). Earlier failed attempt (reverted): a boot-job `exec /system/bin/asx_boot.sh` worker — fails because under Enforcing a restricted init-spawned domain can't `setenforce`/`pidof`. Backups: `/data/local/tmp/appspawn_x.cfg.bak-preauto` (clean ondemand), `.eager`, `.memcg`. [STATUS: verifying memcg+eager auto-start + whether appspawn domain spawns under Enforcing; if not, need a working permissive-at-boot.]

11% battery → aggressive lockscreen (上滑解锁) confounds snapshots: `power-shell wakeup` + `power-shell timeout -o 600000` + `uinput -T -m 360 1150 360 250 400` (swipe-up unlock). Output truncation in my earlier cmds was my own `head -N` cutting past verbose uinput lines — suppress uinput with `>/dev/null`. See [[adapter-launcher-icon-entryhap-fix]].
