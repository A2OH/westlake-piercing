# Westlake — running the Android app *noice* on OpenHarmony (OHOS)

This repo captures the "Westlake" project's work to run the Android app
**noice** (`com.github.ashutoshgngwr.noice`) on an **OpenHarmony (OHOS)
DAYU200 / RK3568** device (32-bit ARM, app uid 13731) using the **appspawn-x**
AOSP-app adapter. It contains the sources, native libs, smali patches, deploy
scripts and a full reproduce procedure so another engineer/agent can redo it.

> **Read first:** `STATUS.md` (honest end-to-end status), then this file, then
> `docs/MEMORY.md` (the index) and the `docs/noice-*.md` topic files for the full
> root-cause history. The memory files are the primary source of truth; this
> document distills the repeatable steps from them.

---

## 0. What this is (architecture)

noice is a stock Android APK. OHOS has no Android app runtime; the project runs
it via an **AOSP-app adapter** layered on OHOS:

- **`liboh_android_runtime.so`** — the adapter's Android runtime (ART + JNI +
  framework natives), loaded into each `appspawn-x` child. **Deployed md5
  `16e08711`.** This binary is the **un-rebuildable base** (see §9): its build
  source no longer matches anything we can compile, so it is treated as a fixed
  blob. **Do NOT attempt to rebuild it.**
- **`liboh_adapter_bridge.so`** — the C++ JNI bridge between the AOSP framework
  and OHOS (windowing, input, surfaces, ability lifecycle). **Locally
  buildable.** Deployed md5 `2967c30c`.
- **`libhwui.so`** — AOSP hwui (Skia/EGL render pipeline) ported to OHOS.
  Locally buildable. Deployed md5 `8b8f84ec`.
- **`framework.jar`** + the boot image — AOSP framework, patched via smali and
  recompiled into a dex2oat boot image. Patched framework.jar md5 `15396933`.
- **noice `base.apk`** — patched via apktool+smali, debug-signed.
- **LD_PRELOAD native shims** + a **BPF socket grant** + a **CA trust store** —
  the connectivity layer.

The fixes below are split by component. Each has a **what it solves** and an
honest **status**.

### Device access

The device is reached over `hdc` **from WSL, invoking the Windows hdc.exe**:

```bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }   # strip CRLF that hdc.exe adds
```

**File-send quirk (important):** `hdc.exe file send` with a *relative* or WSL
(`/home/...`) source path silently mangles the destination (often makes it a
directory) or drops large files. **Always copy the file into the Windows tools
dir first and send the Windows path:**

```bash
WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools     # same dir, WSL view
WINDIR='C:\Users\dspfa\Dev\ohos-tools'       # same dir, Windows view
cp myfile.so "$WSLWIN/x.so"
$HDC file send "$WINDIR\\x.so" /data/local/tmp/x.so 2>&1 | tr -d '\r' | grep -iE 'finish|fail'
# then verify remote size == local size; retry up to 5x for big files (boot-*.* are 20-50MB)
```

---

## 1. Device prerequisites

- OHOS DAYU200 / RK3568 board, rooted hdc shell, SELinux settable to permissive.
- The appspawn-x AOSP adapter already installed on the device under
  `/system/android/` (runtime, framework, boot image) — this repo patches an
  existing adapter install; it does not bootstrap one from scratch.
- noice installed as a bundle at
  `/data/app/el1/bundle/public/com.github.ashutoshgngwr.noice/android/base.apk`
  (app data base: `/data/app/el1/0/base/com.github.ashutoshgngwr.noice`).
- These device-side tools staged in `/data/local/tmp/` (built previously with
  OHOS clang; sources for the ones we ship are in `native-libs/`):
  - `start_asx.sh` — launches a shell-domain appspawn-x with the LD_PRELOAD chain.
  - `bpfgrant` — grants per-uid internet in the netsys eBPF map (§7).
  - `libsetgidhook.so`, `libdnshook.so`, `libjdnshook.so`, `libnetlog.so`,
    `libw14supp.so`, `libv4force.so` in `/system/android/lib/` (LD_PRELOAD shims).
  - `noice-room.db.bak` + `noice-cdn-cache.bak/` — a cached populated sound
    library so the list renders without depending on a live fetch.
- Host build tools (WSL): OHOS clang prebuilts, `dex2oat64` (`$HOME/tools`),
  apktool 2.9.3, smali/baksmali 3.0.3, an Android debug keystore. See the build
  sections for exact invocations.

**USB convenience params** (set once; keeps hdc on and kills the USB-mode dialog
that backgrounds noice on every reconnect):

```bash
$HDC shell "param set persist.sys.usb.config hdc_debug"
$HDC shell "param set persist.usb.setting.gadget_conn_prompt false"
```

---

## 2. Bring-up procedure (run after every reboot)

appspawn-x is on-demand/transient and, when launched by init, runs in an
AT_SECURE SELinux domain that **strips LD_PRELOAD** — so the preload shims never
load and apps fork-fail. The fix is to launch appspawn-x **from the hdc shell
domain** (not AT_SECURE) via `start_asx.sh`, then fix up the socket label.

`start_asx.sh`'s LD_PRELOAD chain (order matters; `libw14supp.so` is the
required substrate):

```
LD_PRELOAD=/system/android/lib/libsetgidhook.so:\
/system/android/lib/libw14supp.so:\
/system/android/lib/libdnshook.so:\
/system/android/lib/libjdnshook.so:\
/system/android/lib/libnetlog.so:\
/system/android/lib/libv4force.so
```

Full bring-up, in order:

```bash
# 1. SELinux permissive + the perf cgroup the adapter expects
sh "mkdir -p /dev/memcg/perf_sensitive 2>/dev/null; setenforce 0"

# 2. Launch appspawn-x detached from the shell domain (preloads survive)
sh "setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"
sleep 14

# 3. MANDATORY: relabel the AppSpawnX socket or AMS can't reach it (silent no-spawn)
sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0"

# 4. Keep the screen awake forever (idle screen-lock steals focus -> 'not clickable')
sh "power-shell wakeup; power-shell timeout -o 86400000"

# 5. Grant noice's uid internet in the netsys eBPF map (re-run; netsys may reset it)
sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map"

# 6. (optional) quiet hilog for readable child stderr
sh "hilog -p off; param set hilog.private.on false"
```

Verify exactly **one** appspawn-x has the preloads loaded:

```bash
sh "grep -l setgidhook /proc/*/maps"   # exactly one match (comm=main, ppid=1)
```

Two appspawn-x = socket conflict = all apps fail. Kill stragglers and re-run.

---

## 3. Launching noice (populated)

```bash
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
# restore the cached library so the list is populated (avoids ~50% blank-list race)
sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null
    cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null
    chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice"
```

Find the child pid (uid 13731):

```bash
sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); [ "$u" = 13731 ] && [ "$c" != sh ] && echo ${d##*/}; done | head -1'
```

**A COLD launch (after force-stop) is the only reliable way to land WMS input
focus on noice** (warm re-`aa start` and the focus heartbeat do NOT reliably
re-focus — see §10). Budget ~4 force-stops per reboot before AMS degrades and
stops spawning; then reboot.

---

## 4. Per-fix reference (file → what it solves → status)

### A. Bridge — `liboh_adapter_bridge.so` (deployed md5 `2967c30c`)

Source in `bridge-src/`:
- `oh_window_manager_client.cpp` — windowing/session/surface management.
- `oh_input_bridge.cpp` / `.h` — input dispatch + the tap control channel.

Fixes:
1. **Sub-window → foreground-activity parent tracking** (`g_fgMainSession`, set
   on main-window create/show in `oh_window_manager_client.cpp createSession`).
   *Solves:* multi-activity dialogs (e.g. the volume bottom-sheet, time picker)
   were parented to the highest-sessionId window instead of the actual
   foreground activity → dialog appeared detached / launcher bled through. Now
   sub-windows parent to the real foreground main. **Status: working.**
2. **Removed the dead focus heartbeat** — an earlier `moveMissionToFront` loop
   never fired (`getMissionIdForBundle` returns -1 on this AMS) and added churn.
   Removed. *Solves:* dead code / churn. **Status: done.** (Focus reliability
   itself is still unsolved — §10.)
3. **In-process D-pad + touch dispatch + tap control channel**
   (`dispatchKeyViaViewRoot`, `dispatchTouchViaViewRoot`,
   `startTapControlChannel` in `oh_input_bridge.cpp`).
   The deployed runtime's InputChannel *consumer* drops KEY events (stub) and
   MMI pointer delivery to noice's window is intermittent (displayId/WMS
   arbitration). The bridge sidesteps both by building a Java `KeyEvent` /
   `MotionEvent` and posting it straight to the focused `ViewRootImpl`'s
   `mInputEventReceiver` (found via `WindowManagerGlobal.mRoots` reflection) on
   the main looper. It also polls **`/data/local/tmp/noice_tap`**:
   `echo "X Y" > /data/local/tmp/noice_tap` = in-process tap at raw coords;
   `echo "N"` (1–5) = bottom-nav tab N. This is **focus-independent** and is the
   reliable way to drive the UI in automated tests.
   *Solves:* D-pad navigation/activation and touch clicks that the runtime/MMI
   path can't deliver. **Status: D-pad nav+activate proven; tap control channel
   proven; raw MMI touch still intermittent.**
   - Note: the touch path also needs the **VelocityTracker JNI stub** the bridge
     registers via `env->RegisterNatives` (the runtime never registers
     `android_view_VelocityTracker`, so `nativeInitialize` threw
     `UnsatisfiedLinkError` and aborted touch dispatch). That stub work is in
     the bridge source history; the deployed `2967c30c` is the consolidated
     bridge. Velocity always reads 0 (no fling momentum), but down/move/up +
     click detection + scroll work.

**Build the bridge** (locally, no runtime rebuild):

```bash
cd $HOME/bridge-build
OH_ROOT=$HOME/openharmony \
AOSP_ROOT=$HOME/bridge-build/aosp \
ADAPTER_ROOT=$HOME/bridge-build \
BRIDGE_TMP=/tmp/bridge_build \
bash build/build_adapter.sh --target=liboh_adapter_bridge.so
# output: out/adapter/liboh_adapter_bridge.so
```

**Deploy the bridge** (loads per-child; no reboot needed):

```bash
# send via Windows path (see §0), then:
sh "mount -o remount,rw / 2>/dev/null
    cp /data/local/tmp/bridge.so /system/lib/liboh_adapter_bridge.so
    cp /data/local/tmp/bridge.so /system/android/lib/liboh_adapter_bridge.so
    chcon u:object_r:system_lib_file:s0 /system/android/lib/liboh_adapter_bridge.so"
sh "aa force-stop com.github.ashutoshgngwr.noice"   # next cold launch picks it up
```

### B. libhwui — `libhwui.so` (deployed md5 `8b8f84ec`)

Source patch in `bridge-src/hwui_oh_abi_patch.cpp`. Three fixes:
1. **Gated per-frame `glReadPixels`** in the swapBuffers hijack (behind a frame
   condition). *Solves:* an unconditional x4 readback was stalling every app's
   render thread → red flicker during playback. **Status: working (red flicker
   fixed).**
2. **`ASurfaceControl_release` no-op (G3.8).** *Solves:* a use-after-free
   SIGSEGV in the RenderThread during 2-window teardown
   (`ASurfaceControl_release+20` derefs a freed `sc`). Making it a no-op leaks a
   bounded handle but stops the crash; RSSurfaceNode owns the real lifecycle.
   (This was originally a binary patch in the runtime; the consolidated fix
   lives in the hwui/bridge build path — see the topic docs.) **Status:
   working (noice renders stably).**
3. **New-surface EGL fix (NSFIX).** In the `eglCreateWindowSurface` hijack, set
   the OH NativeWindow format (`SET_FORMAT,12`) + usage (`|HW_RENDER|HW_TEXTURE`)
   on the unwrapped native window before the real EGL create, plus a bounded
   retry on `EGL_NO_SURFACE`. *Solves:* the 2nd render surface (e.g. the
   SoundInfo "关于这个声音" page) failed to create → render-thread abort. **Status:
   working (SoundInfo page renders).**

**Build libhwui** (locally):

```bash
cd $HOME/bridge-build
OH_ROOT=$HOME/openharmony \
AOSP_ROOT=$HOME/bridge-build/aosp \
ADAPTER_ROOT=$HOME/bridge-build \
BRIDGE_TMP=/tmp/bridge_build \
bash build/build_aosp_lib.sh --target=libhwui.so
# output: out/aosp_lib/libhwui.so  (deploy to /system/android/lib/libhwui.so, chcon system_lib_file)
```

(The Phase-4 UND gate may flag stdlib/skia/minikin UNDs as "new"; they are
defined in NEEDED libs — sound. See `docs/noice-dpad-consumer-keystub.md`.)

### C. framework.jar smali patches + boot image (patched jar md5 `15396933`)

The adapter's OHOS has no Android system services, so `getSystemService(...)`
returns null and many framework calls NPE/crash. The universal fix patches the
framework BCP jar so service fetchers return non-null managers with inert
methods. Patched smali in `framework-smali-patches/`:

| File | What it solves |
|------|----------------|
| `android_app_SystemServiceRegistry$88.smali` | ShortcutManager fetcher: catch `ServiceNotFoundException` → build `ShortcutManager(ctx, null)` instead of throwing. |
| `android_app_SystemServiceRegistry$7.smali` | AlarmManager fetcher: same pattern → `AlarmManager(null, ctx)`. |
| `android_content_pm_ShortcutManager.smali` | null-guard `getDynamicShortcuts`/`getManifestShortcuts`/`getPinnedShortcuts` (return empty list when service is null). |
| `android_app_AlarmManager.smali` | null-guard `canScheduleExactAlarms` (try/catch + return true when service null). |
| `android_content_ContentResolver.smali` | null-guard `register`/`unregisterContentObserver` (no ContentService on the adapter → NPE). This one originally unblocked the DNS path. |
| `RuntimeInit_KillApplicationHandler.smali` | selective uncaught-exception handler (kill only on the main thread). **Status: INEFFECTIVE** — the adapter runtime intercepts uncaught exceptions before this handler runs, so it does not actually swallow background crashes. Documented for completeness; the working coroutine-crash mitigation is in the noice APK (§D). |

These make noice's per-app APK guards (ShortcutManager/AlarmManager) redundant
and fix the same family for all apps. **Status: working** (except the
RuntimeInit UEH, which is known-ineffective).

**Rebuild framework.jar + boot image** (the boot-regen cycle; full detail in
`docs/reference_boot_regen_cycle_2026-05-30.md`):

1. baksmali the class out of `framework.jar`, apply the patch, reassemble:
   - baksmali: cmdline-tools `smali-baksmali/dexlib2/util-3.0.3.jar` + guava.
   - assemble: `scripts/SmaliAssemble.java` (drives
     `brut.androlib.mod.SmaliMod` from apktool.jar — apktool's own baksmali.Main
     has no `main()`; this wrapper supplies the assembler entry point).
   - `zip framework.jar classes.dex` (preserve the manifest), re-baksmali to
     confirm the edit landed.
2. Pull **all 10** BCP jars from the device (relative recv paths — hdc.exe
   mangles absolute WSL paths), swap in the patched framework.jar.
3. Regenerate the boot image with the host `dex2oat64`
   (`$HOME/tools/dex2oat64` + `lib64/libsigchain.so`,
   `--instruction-set=arm --base=0x70000000 --compiler-filter=speed`),
   producing 30 segments (`boot.{art,oat,vdex}` + 9× `boot-<jar>.{art,oat,vdex}`).
   **Verify BCP order** first: `strings /system/bin/appspawn-x | grep 'framework/.*\.jar'`.
4. Send all 30 boot files + the jar (size-verify + retry; they're 20-50MB) to
   `/system/android/framework/arm/` + `/system/android/framework/`, chcon
   `system_file`, `rm -rf /data/misc/appspawnx/dalvik-cache/*`, **reboot**.
5. HW-gate: HelloWorld reaches `onResume` with zero
   `mark_sweep|Fatal|cppcrash|Class mismatch|InitWithoutImage`. Snapshot the
   current working boot+jar to `/data/local/tmp/<rollback>` BEFORE deploying so a
   bad image rolls back to the latest win.

### D. noice APK smali patches (apktool + debug-sign)

Patched smali shipped in `noice-smali-patches/`:
- `kotlinx-coroutines/a.smali` — **the coroutine fix.** noice's
  `kotlinx.coroutines.a.a(CoroutineContext, Throwable)`
  (`handleCoroutineException`) is patched so a background-coroutine exception is
  **swallowed/logged** and only kills the process on the **main thread**.
  *Solves:* the subscription page (`查看订阅计划` → `loadPlans` → `listPlans`)
  threw an uncaught exception in the i8.s dispatcher when the flaky network
  fetch failed → process death. **Status: PROVEN — subscription/Register flows
  survive instead of crashing.**

(Additional noice APK patches developed during the project — tag rendering, the
FlexboxLayout measure fix, the ShortcutManager/AlarmManager per-app guards — are
described in `docs/noice-network-inet-gid-fix.md` and
`docs/noice-content-population-findings.md`. They are now largely redundant with
the framework.jar fixes in §C; the coroutine patch is the one shipped here as it
has no framework-level equivalent.)

**Rebuild + sign the APK:**

```bash
java -jar $HOME/apktool.jar d base.apk -o noice_dec          # decode
# copy patched smali into noice_dec/smali*/kotlinx/coroutines/a.smali (verify the path/classesN match)
java -jar $HOME/apktool.jar b noice_dec -o noice-unsigned.apk # rebuild
zipalign -p -f 4 noice-unsigned.apk noice-aligned.apk
apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android \
  --out noice-coro-signed.apk noice-aligned.apk
```

(For single-method smali edits you can instead baksmali just the affected
classesN.dex, edit, reassemble with `scripts/SmaliAssemble.java`, and rezip —
faster than a full apktool round-trip.)

**Deploy the APK** (`scripts/deploy_coro.sh` automates this):

```bash
BUNDLE=/data/app/el1/bundle/public/com.github.ashutoshgngwr.noice/android
# send via Windows path, then:
sh "mount -o remount,rw / 2>/dev/null
    cp /data/local/tmp/nc.apk $BUNDLE/base.apk; chmod 644 $BUNDLE/base.apk; chown 0:0 $BUNDLE/base.apk
    chcon u:object_r:app_install_file:s0 $BUNDLE/base.apk
    rm -rf $BUNDLE/oat /data/app/el1/0/base/com.github.ashutoshgngwr.noice/code_cache"
sh "reboot"   # dex boot-scan picks up the new APK
```

### E. Connectivity (LD_PRELOAD chain + BPF grant + CA store) — see §6, §7

---

## 5. Building the native shims

Build with the OHOS clang prebuilt, arm32 target, against the OHOS musl sysroot.
The provided `libv4force.so` (md5 `7c3e5ecec7d1ceff6e1ce66092161467`, 32-bit ARM)
is committed; here is how to rebuild it (adjust paths to your OHOS tree):

```bash
prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang \
  --target=arm-linux-ohos -fPIC -shared -O2 \
  -isystem out/rk3568/obj/third_party/musl/usr/include/arm-linux-ohos \
  -nostdlib \
  -o libv4force.so native-libs/libv4force.c \
  out/rk3568/.../lib/arm-linux-ohos/libc.so
```

The same command pattern builds `libjdnshook.so` (from `native-libs/libjdnshook.c`),
`libsetgidhook.so`, `libdnshook.so`, `libnetlog.so`.

---

## 6. Connectivity — the LD_PRELOAD chain (what each shim solves)

| Shim | Source | Solves |
|------|--------|--------|
| `libsetgidhook.so` | (src in MEMORY) | appspawn-x skips `OH_TLV_INTERNET_INFO` so children lack AID_INET → `socket(AF_INET)` EPERM. Intercepts `setgroups` and appends gids 3003 (inet) + 3004 (net_raw). |
| `libdnshook.so` | (src in MEMORY) | OHOS musl `getaddrinfo` routes through netsys per-netid; sandboxed child has no netid → EAI_NONAME. Does a direct-UDP A-query to 8.8.8.8:53. Resolves trynoice.com. |
| `libjdnshook.so` | `native-libs/libjdnshook.c` | The JVM/libcore DNS path (`android_getaddrinfofornet`) EPERMs separately from plain `getaddrinfo`. Delegates the `*fornet` variant to the hooked `getaddrinfo`. |
| `libnetlog.so` | (src in MEMORY) | Logs socket/connect calls to `/data/local/tmp/netlog.log` for diagnosis. |
| `libv4force.so` | `native-libs/libv4force.c` | The JVM creates dual-stack `AF_INET6` TCP sockets and connects to IPv4-mapped `::ffff:a.b.c.d`; this device has **no IPv6 route** so those connects fail. Forces `AF_INET6` STREAM sockets to `AF_INET` and rewrites IPv4-mapped connect targets to `sockaddr_in`; swallows IPv6 sockopts. |
| `libw14supp.so` | (substrate, REQUIRED) | adapter substrate; must be in the chain or children fail. |

All shims go in `/system/android/lib/` (chcon `system_lib_file`) and are listed
in `start_asx.sh`'s LD_PRELOAD (§2). LD_PRELOAD **must** come via
`start_asx.sh` (shell domain), never the init cfg (AT_SECURE strips it).

---

## 7. Connectivity — BPF socket grant + CA trust store

**BPF grant.** Even with inet gids, OHOS netsys runs a cgroup-eBPF program
(`/system/etc/bpf/netsys.o`, `cgroup_sock/inet_create_socket`) that reads map
`oh_sock_permission_map` keyed by uid: val==0 → DENY, val!=0/absent → ALLOW.
BMS had added noice's uid 13731 with val=0. Grant it:

```bash
sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map"   # sets [13731]=1
```

Re-run during launch (netsys may reset it). After the grant, `socket(AF_INET)`
succeeds and TCP connects to the real CDN work.

**CA trust store.** The device's `/system/etc/security/cacerts/` was **empty**,
so every HTTPS handshake failed trust validation. Install the bundled roots
(`scripts/deploy_ca.sh`, source `ca-store/cacerts.tgz`,
md5 `888d018ddebbd183d65745faa0972c1c`):

```bash
# send cacerts.tgz via Windows path, then:
sh "mount -o remount,rw / 2>/dev/null; mkdir -p /system/etc/security/cacerts
    cd /system/etc/security/cacerts && tar xzf /data/local/tmp/ca.tgz
    chmod 644 *.0; chown 0:0 *.0; chcon u:object_r:system_file:s0 *.0"
```

> **HONEST connectivity status:** DNS (libdnshook/libjdnshook), TCP, the socket
> family (libv4force), the BPF grant and the CA store are all done and deployed.
> The low-level path is proven (a raw probe reaches the real cdn.trynoice.com
> nginx). **But the app's live HTTPS data still does not load** — the
> subscription page shows "网络无法访问". A deeper diagnosis (libv4force
> request-logging through okhttp's non-blocking connect / TLS) is **pending**.
> Connectivity is **partially working / in progress**. Do not assume live HTTPS
> works end-to-end.

---

## 8. Deploy scripts (in `scripts/`)

These were written for the specific device paths above; adapt `HDC`/`WINDIR` to
your host. Each does send-via-Windows-path + the device-side install + a verify.

- `deploy_v4.sh` — installs `libv4force.so`, adds it to the start_asx LD_PRELOAD,
  restarts appspawn-x, re-enables IPv6 (to prove libv4force is the fix, not the
  disable), cold-launches noice with empty cache to force a live fetch, captures
  a screenshot, and dumps netlog connect results.
- `deploy_coro.sh` — installs the coroutine-fixed APK, reboots, brings up
  appspawn-x, then runs the coroutine-fix regression: taps subscription ×3 and
  Register ×3 and checks the pid SURVIVED each time (PASS = no crash).
- `deploy_ca.sh` — installs the CA bundle and re-tests subscription live load.
- `livetest.sh` — cold-launches with empty cache+db to force a live library
  fetch; reports screenshot size (>45k populated, <25k blank) + okhttp/network
  log lines.
- `navtour.sh` — full nav tour via the tap control channel: captures each page
  (library, SoundInfo, volume, saved, sleep-timer, alarms, add-alarm, account).

---

## 9. Verification

- **Renders / populated:** `snapshot_display -f /data/local/tmp/i.jpeg` then
  check the file size — **>45 KB = populated** library; <25 KB = blank/race.
  Recv via a *relative* path: `$HDC file recv /data/local/tmp/i.jpeg shot.jpeg`.
- **noice alive:** the uid-13731 pid finder in §3; same pid before/after an
  action = survived.
- **Touch/D-pad reaching the view:** the bridge logs (`dispatchKeyViaViewRoot
  ... -> ViewRootImpl OK`, `tapControlChannel: in-process tap`, `lastHandled=1`)
  and the OHTouchInjector diag fields (`invokeCount/runCount/lastHandled/lastEx`).
- **Connectivity (low level):** `/data/local/tmp/netlog.log` socket/connect rc;
  the `conntest`/`bpfgrant` tools.
- A **uxfixture-style** harness (the `*.sh` scripts here) automates: reboot →
  bring-up → cold-launch-until-populated (reroll on shot<45K) → drive via the
  tap control channel → capture per page. The control channel is
  focus-independent, so tests don't depend on flaky WMS focus.

---

## 10. Honest limitations & flakiness

- **Runtime un-rebuildable.** `liboh_android_runtime.so` `16e08711` is a fixed
  blob; its build source no longer matches anything compilable (local rebuilds
  regress noice at render-thread init). Every runtime-level gap (KEY-consumer
  stub, missing VelocityTracker/AudioTrack JNI) is worked around from the
  **bridge**, not by rebuilding the runtime.
- **RuntimeInit UEH ineffective** (§4C) — the runtime intercepts uncaught
  exceptions first. The coroutine crash is mitigated in the noice APK instead.
- **Live HTTPS still failing** (§7) — DNS/TCP/socket-family/CA done; the app's
  live data fetch does not complete; diagnosis pending. UI is populated from a
  cached library in the meantime.
- **Audio output unsolved** — the play *click* works (binds
  SoundPlaybackService + ExoPlayer), but the runtime never registers
  `android_media_AudioTrack` JNI, so actual PCM output → AudioTrack → OH HAL is
  not wired. A real fix is an AudioTrack→OH_AudioRenderer bridge (separate
  project).
- **Focus reliability** — only a **cold** launch reliably lands WMS input focus
  on noice; warm re-`aa start` and `moveMissionToFront` do not (the latter's
  missionId lookup returns -1 on this AMS). Idle screen-lock and stray unlock
  swipes steal focus → "not clickable". Mitigations: keep the screen awake
  (`power-shell timeout -o 86400000`), use the focus-independent tap control
  channel for tests, cold-launch to re-focus.
- **Spawn/render flakiness** — `aa force-stop` degrades AMS after ~4 (reboot to
  clear); ~50% of cold launches hit a blank-list flow race (reroll until
  shot>45K); an intermittent bad-boot spin in `installSettingsContentProviderStub`
  (IContentProvider Proxy class-linking) burns a boot (reboot to reroll); the
  displayId compositing race sometimes renders the launcher over noice in
  screenshots.

---

## 11. Component md5 reference (for provenance; binaries NOT all committed)

| Component | Deployed md5 | Committed here? |
|-----------|--------------|-----------------|
| `liboh_android_runtime.so` | `16e08711` | No — un-rebuildable base blob; record only. |
| `liboh_adapter_bridge.so` | `2967c30c` | No — buildable from `bridge-src/`. |
| `libhwui.so` | `8b8f84ec` | No — buildable from `bridge-src/hwui_oh_abi_patch.cpp`. |
| `framework.jar` (patched) | `15396933` | No — regenerated from smali in `framework-smali-patches/` + boot regen. |
| boot image (30 segments) | — | No — large; regenerate per §4C. |
| noice `base.apk` (coro-signed) | `384134d8...` (see ARTIFACT-INVENTORY.txt) | No — rebuild from `noice-smali-patches/` + a noice base APK. |
| `libv4force.so` | `7c3e5ece...` | **Yes** (3 KB). |
| `cacerts.tgz` | `888d018d...` | **Yes** (133 KB). |

Large/un-rebuildable binaries are intentionally **not** committed; their md5s +
provenance are recorded here and in `ARTIFACT-INVENTORY.txt` so they can be
reproduced or matched. Sources, scripts, smali patches and the two small data
blobs (libv4force.so, cacerts.tgz) are all in this repo.
