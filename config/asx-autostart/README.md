# appspawn-x boot auto-start (fixes white-screen-on-tap after reboot)

appspawn-x (the AOSP-adapter app spawner) does not reliably auto-start on boot: the stock
`appspawn_x.cfg` ondemand service fails at tap time (its `writepid` dir
`/dev/memcg/perf_sensitive/cgroup.procs` doesn't exist, and `AT_SECURE` in the appspawn domain strips
`LD_PRELOAD` so the network/substrate shims never load). Result: tap an adapter app icon after reboot
→ stuck white loading window.

This adds a **separate, non-critical oneshot** init service that brings appspawn-x up correctly.

## Files → device path

- `asx_boot.sh`            → `/system/bin/asx_boot.sh`  (chmod 0755, `chcon u:object_r:system_file:s0`)
- `zz-asx-autostart.cfg`   → `/system/etc/init/zz-asx-autostart.cfg`  (chmod 0644, `chcon u:object_r:system_etc_file:s0`)
- also copy `bpfgrant`     → `/system/bin/bpfgrant`  (so it survives a `/data` wipe)

`asx_boot.sh` mirrors the proven manual bringup: `setenforce 0`, wait for `foundation`, kill any
appspawn-x, `mkdir /dev/memcg/perf_sensitive`, export `APPSPAWNX_NO_JIT` + the 6-lib `LD_PRELOAD`
(setgidhook/w14supp/dnshook/netlog/jdnshook/v4force), `setsid appspawn-x …` (backgrounded → reparents
to init), wait for the socket then `chmod 0666` + `chcon u:object_r:appspawn_socket:s0` it, then a
background loop `bpfgrant 13731 oh_sock_permission_map` (netsys may reset the eBPF map).

## Why these three details are load-bearing

1. **`secon: u:r:su:s0`** in the cfg — runs the script in the *shell* domain (like the manual hdc
   bringup), so the sh→appspawn-x exec does **not** domain-transition to `appspawn` → `AT_SECURE`
   stays 0 → `LD_PRELOAD` is honoured (verify: `grep -c setgidhook /proc/$(pidof appspawn-x)/maps`).
   `hdcd.cfg` is the only other su-domain service — it's a valid service domain.
2. **`start-mode: condition` + a `post-fs-data` job** (mirrors the device's proven `check_module_update.cfg`).
   A bare `boot` job with no `start-mode` silently never ran.
3. **non-critical oneshot** (`once: 1`, no `critical`) — cannot bootloop or brick; worst case it just
   fails to start appspawn-x (white screen, recoverable). NEVER mark it `critical`.

After reboot, wait ~30–60s (the script waits for `foundation`, then starts appspawn-x), then tap the
icon. Manual fallback: `setsid sh /system/bin/asx_boot.sh </dev/null >/dev/null 2>&1 &`.
Revert: `rm /system/etc/init/zz-asx-autostart.cfg`.

> Adjust the `13731` uid in `asx_boot.sh` to the noice uid on your board (`bm dump -n
> com.github.ashutoshgngwr.noice | grep '"uid"'`), and add lines for any other adapter app uids.
