# Westlake — running the Android app *noice* on OpenHarmony (OHOS)

This repo captures the **Westlake** project's work to run the stock Android app
**noice** (`com.github.ashutoshgngwr.noice`) on an **OpenHarmony (OHOS)
DAYU200 / RK3568** device (32-bit ARM, app uid **13731**) using the
**appspawn-x** AOSP-app adapter. It contains the sources, native libs, smali
patches, deploy scripts, test fixtures and a full reproduce procedure so another
engineer/agent can redo the entire effort from scratch.

> **Read order:** `STATUS.md` (honest end-to-end status) → this file → `docs/`
> (the full root-cause history; `docs/MEMORY.md` is the index, the
> `docs/noice-*.md` topic files have the per-component detail). The `docs/` files
> are the primary source of truth; this document distills the *repeatable steps*.

This is a thorough, copy-pasteable procedure. Adjust the host paths (clang, SDK,
`HDC`, the Windows tools dir) to your machine; the device-side paths are exact.

---

## Table of contents

0. Architecture & device access
1. Device prerequisites
2. Host toolchain setup (exact paths)
3. Bring-up procedure (after every reboot)
4. Launching noice (populated)
5. Per-component BUILD instructions
   - A. Bridge `liboh_adapter_bridge.so`
   - B. `libhwui.so`
   - C. `framework.jar` smali patches + **boot image regen**
   - D. noice APK smali patches (apktool + debug-sign)
   - E. Native LD_PRELOAD shims (libv4force / libjdnshook / …)
   - F. **Native-TLS chain** (libtlsjni + TlsJni/TlsJniSocket + DexClassLoader $Sf)
6. DEPLOY instructions (the hdc.exe quirk + per-target deploy)
7. Connectivity — the 5-layer stack (DNS / IPv4-force / cgroup-BPF / CA / TLS)
8. TEST instructions (the UX fixtures + NetTest + the per-page/per-submenu table)
9. Honest limitations & flakiness
10. Artifacts: component md5s & provenance

---

## 0. Architecture & device access

noice is a stock Android APK. OHOS has no Android app runtime; the project runs
it via an **AOSP-app adapter** layered on OHOS. Components:

- **`liboh_android_runtime.so`** — the adapter's Android runtime (ART + JNI +
  framework natives), loaded into each `appspawn-x` child. **Deployed md5
  `16e08711`.** This is the **un-rebuildable base** (§9): its build source no
  longer matches anything we can compile (local rebuilds regress noice at
  render-thread init), so it is treated as a fixed blob. **Do NOT rebuild it** —
  every runtime-level gap is worked around from the *bridge* instead.
- **`liboh_adapter_bridge.so`** — the C++ JNI bridge between the AOSP framework
  and OHOS (windowing, input, surfaces, ability lifecycle). Locally buildable.
  Deployed md5 `2967c30c`.
- **`libhwui.so`** — AOSP hwui (Skia/EGL render pipeline) ported to OHOS. Locally
  buildable. Deployed md5 `8b8f84ec`.
- **`framework.jar`** + the boot image — AOSP framework, patched via smali and
  recompiled into a dex2oat boot image. Patched framework.jar md5 `15396933`.
- **`adapter-runtime-bcp.jar`** + **`tlsjni-extra.dex`** — the adapter runtime
  BCP and the out-of-BCP native-TLS dex (§5F).
- **noice `base.apk`** — patched via apktool + smali, debug-signed.
- **LD_PRELOAD native shims** + a **BPF socket grant** + a **CA trust store** +
  **native TLS** — the connectivity layer (§7).

### Device access (hdc from WSL via Windows hdc.exe)

The device is reached over `hdc` **from WSL, invoking the Windows hdc.exe**:

```bash
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
sh() { $HDC shell "$1" 2>&1 | tr -d '\r'; }   # strip the CRLF hdc.exe adds
```

**File-send quirk (critical).** `hdc.exe file send` with a *relative* or WSL
(`/home/...`) **source** path silently mangles the destination (often makes it a
directory) or drops large files. **Always copy the file into the Windows tools
dir first and send the Windows path:**

```bash
WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools     # same dir, WSL view
WINDIR='C:\Users\dspfa\Dev\ohos-tools'       # same dir, Windows view
cp myfile.so "$WSLWIN/x.so"
$HDC file send "$WINDIR\\x.so" /data/local/tmp/x.so 2>&1 | tr -d '\r' | grep -iE 'finish|fail'
# verify remote size == local size; retry up to 5x for big files (boot-*.* are 20-50 MB)
```

**File-recv quirk:** `hdc.exe file recv` to an **absolute** WSL path mangles too;
recv to a **relative** path from the cwd:
`$HDC file recv /data/local/tmp/i.jpeg shot.jpeg`.

---

## 1. Device prerequisites

- OHOS DAYU200 / RK3568 board, rooted hdc shell, SELinux settable to permissive.
- The appspawn-x AOSP adapter already installed under `/system/android/`
  (runtime, framework, boot image). This repo **patches an existing adapter
  install**; it does not bootstrap one from scratch.
- noice installed as a bundle at
  `/data/app/el1/bundle/public/com.github.ashutoshgngwr.noice/android/base.apk`
  (app data base: `/data/app/el1/0/base/com.github.ashutoshgngwr.noice`).
- These device-side tools staged in `/data/local/tmp/` (built previously with
  OHOS clang; sources for the ones we ship are in `native-libs/` / `native-tls/`):
  - `start_asx.sh` — launches a shell-domain appspawn-x with the LD_PRELOAD chain.
  - `bpfgrant` — grants per-uid internet in the netsys eBPF map (§7).
  - In `/system/android/lib/`: `libsetgidhook.so`, `libdnshook.so`,
    `libjdnshook.so`, `libnetlog.so`, `libw14supp.so`, `libv4force.so`,
    `libtlsjni.so`.
  - `noice-room.db.bak` + `noice-cdn-cache.bak/` — a cached populated sound
    library so the list renders without depending on a live fetch.
  - (`conntest` — a small uid-13731 socket probe used to isolate the cgroup-BPF
    layer from the app's stack.)

**USB convenience params** (set once; keeps hdc on and kills the USB-mode dialog
that backgrounds noice on every reconnect):

```bash
$HDC shell "param set persist.sys.usb.config hdc_debug"
$HDC shell "param set persist.usb.setting.gadget_conn_prompt false"
```

---

## 2. Host toolchain setup (exact paths)

All builds run in WSL. Exact tool locations used by this project:

| Tool | Path / version | Used for |
|------|----------------|----------|
| OHOS clang | `$HOME/openharmony/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang` | native shims + libtlsjni (`--target=arm-linux-ohos`) |
| OHOS musl sysroot | `out/rk3568/obj/third_party/musl/...` (in the OHOS tree) | `-isystem` + `libc.so` for the shims |
| Android SDK | `$HOME/android-sdk` | d8, apksigner, zipalign, android-34 `android.jar` |
| build-tools | `$SDK/build-tools/<ver>/{d8,apksigner,zipalign}` | dex compile, sign, align |
| android.jar | `$SDK/platforms/android-34/android.jar` | compile TlsJni/TlsJniSocket/NetTest |
| cmdline-tools smali jars | `$SDK/cmdline-tools/latest/lib/external/.../smali-{baksmali,dexlib2,util}-3.0.3.jar` | baksmali (disassemble) |
| guava jar | (alongside the smali jars in cmdline-tools) | smali/baksmali dependency |
| jcommander jar | (alongside) | smali/baksmali CLI dependency |
| apktool.jar | `$HOME/apktool.jar` (2.9.3) | APK decode/build **and** `brut.androlib.mod.SmaliMod` (the assembler) |
| `scripts/SmaliAssemble.java` | this repo | wraps `brut.androlib.mod.SmaliMod` — apktool's own `baksmali.Main` has **no `main()`**, so this supplies the assembler entry point |
| dex2oat64 | `$HOME/tools/dex2oat64` (+ `lib64/libsigchain.so`) | boot image regen |
| JDK | host `javac`/`java` (8+; compile with `-source 8 -target 8` for dex) | compile Java → class → d8 |

**SmaliAssemble usage** (assemble a smali tree → `classes.dex`):

```bash
CP="$HOME/apktool.jar"   # SmaliMod lives in apktool.jar
javac -cp "$CP" scripts/SmaliAssemble.java -d /tmp/sa
java  -cp "$CP:/tmp/sa" SmaliAssemble <smali_dir> <out_classes.dex>
```

**baksmali usage** (disassemble a dex → smali tree):

```bash
SM=$SDK/cmdline-tools/latest/lib/external
java -cp "$SM/.../smali-baksmali-3.0.3.jar:$SM/.../smali-dexlib2-3.0.3.jar:\
$SM/.../smali-util-3.0.3.jar:$SM/.../guava-*.jar:$SM/.../jcommander-*.jar" \
  com.android.tools.smali.baksmali.Main d classes.dex -o out_smali
```

---

## 3. Bring-up procedure (run after every reboot)

appspawn-x is on-demand/transient and, when launched by init, runs in an
AT_SECURE SELinux domain that **strips LD_PRELOAD** — so the preload shims never
load and apps fork-fail. The fix is to launch appspawn-x **from the hdc shell
domain** (not AT_SECURE) via `start_asx.sh`, then fix up the socket label.

`start_asx.sh`'s LD_PRELOAD chain (order matters; `libw14supp.so` is the required
substrate; `libjdnshook.so` must come **after** `libdnshook.so` so its
`getaddrinfo` resolves to the direct-UDP one):

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
sh "/data/local/tmp/bpfgrant 13731 broker_sock_permission_map"   # belt+suspenders (§7)

# 6. (optional) quiet hilog for readable child stderr
sh "hilog -p off; param set hilog.private.on false"
```

Verify exactly **one** appspawn-x has the preloads loaded:

```bash
sh "grep -l setgidhook /proc/*/maps"   # exactly one match (comm=main, ppid=1)
```

Two appspawn-x = socket conflict = all apps fail. Kill stragglers and re-run.

---

## 4. Launching noice (populated)

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

A **COLD launch** (after force-stop) is the only reliable way to land WMS input
focus on noice (warm re-`aa start` and `moveMissionToFront` do NOT — §9). Budget
~4 force-stops per reboot before AMS degrades and stops spawning; then reboot.

---

## 5. Per-component BUILD instructions

### A. Bridge — `liboh_adapter_bridge.so` (deployed md5 `2967c30c`)

Source in `bridge-src/`: `oh_window_manager_client.cpp` (windowing/session/
surface), `oh_input_bridge.cpp`/`.h` (input dispatch + the tap control channel).

Fixes:
1. **Sub-window → foreground-activity parent tracking** (`g_fgMainSession`, set on
   main-window create/show). *Solves:* multi-activity dialogs (volume
   bottom-sheet, time picker) were parented to the highest-sessionId window
   instead of the actual foreground activity → dialog detached / launcher bled
   through. Now sub-windows parent to the real foreground main + the parent
   session is re-shown. **Status: working** (volume sheet renders over the live
   library).
2. **Removed the dead focus heartbeat** — `moveMissionToFront` never fired
   (`getMissionIdForBundle` returns -1 on this AMS). Removed churn. **Status:
   done** (focus reliability itself is still unsolved — §9).
3. **In-process D-pad + touch dispatch + tap control channel**
   (`dispatchKeyViaViewRoot`, `dispatchTouchViaViewRoot`,
   `startTapControlChannel`). The deployed runtime's InputChannel *consumer* drops
   KEY events (stub) and MMI pointer delivery to noice is intermittent
   (displayId/WMS arbitration). The bridge sidesteps both: it builds a Java
   `KeyEvent`/`MotionEvent` and posts it straight to the focused `ViewRootImpl`'s
   `mInputEventReceiver` (found via `WindowManagerGlobal.mRoots` reflection) on
   the main looper. It also polls **`/data/local/tmp/noice_tap`**:
   `echo "X Y" > /data/local/tmp/noice_tap` = in-process tap at raw coords;
   `echo "N"` (1–5) = bottom-nav tab N. **Focus-independent** — the reliable way
   to drive the UI in automated tests. **Status: D-pad nav+activate proven; tap
   control channel proven; raw MMI touch still intermittent.**
   - The touch path also needs the **VelocityTracker JNI stub** the bridge
     registers via `env->RegisterNatives` (the runtime never registers
     `android_view_VelocityTracker`, so `nativeInitialize` threw
     `UnsatisfiedLinkError` and aborted touch dispatch). Velocity always reads 0
     (no fling momentum), but down/move/up + click + scroll work.

**Build** (locally; no runtime rebuild):

```bash
cd $HOME/bridge-build
OH_ROOT=$HOME/openharmony \
AOSP_ROOT=$HOME/bridge-build/aosp \
ADAPTER_ROOT=$HOME/bridge-build \
BRIDGE_TMP=/tmp/bridge_build \
bash build/build_adapter.sh --target=liboh_adapter_bridge.so
# output: out/adapter/liboh_adapter_bridge.so
```

### B. libhwui — `libhwui.so` (deployed md5 `8b8f84ec`)

Source patch in `bridge-src/hwui_oh_abi_patch.cpp`. Three fixes:
1. **Gated per-frame `glReadPixels` x4** in the swapBuffers hijack (behind a frame
   condition). *Solves:* an unconditional readback stalled every app's render
   thread → red flicker during playback. **Status: working.**
2. **`ASurfaceControl_release` no-op (G3.8).** *Solves:* a use-after-free SIGSEGV
   in the RenderThread during 2-window teardown (`ASurfaceControl_release+20`
   derefs a freed `sc`). Making it a no-op leaks a bounded handle but stops the
   crash; RSSurfaceNode owns the real lifecycle. **Status: working** (noice
   renders stably).
3. **New-surface EGL fix (NSFIX).** In the `eglCreateWindowSurface` hijack: set
   the OH NativeWindow format (`SET_FORMAT,12`) + usage (`|HW_RENDER|HW_TEXTURE`)
   on the unwrapped native window before the real EGL create, plus a bounded
   retry on `EGL_NO_SURFACE`. *Solves:* the 2nd render surface (e.g. the SoundInfo
   "关于这个声音" page) failed to create → render-thread abort. **Status: working**
   (SoundInfo page renders; verified `eglCreateWindowSurface[NSFIX] ... rc=0`).

**Build** (locally):

```bash
cd $HOME/bridge-build
OH_ROOT=$HOME/openharmony \
AOSP_ROOT=$HOME/bridge-build/aosp \
ADAPTER_ROOT=$HOME/bridge-build \
BRIDGE_TMP=/tmp/bridge_build \
bash build/build_aosp_lib.sh --target=libhwui.so
# output: out/aosp_lib/libhwui.so
```

(The Phase-4 UND gate may flag stdlib/skia/minikin UNDs as "new"; they're defined
in NEEDED libs — sound. See `docs/noice-dpad-consumer-keystub.md`.)

### C. framework.jar smali patches + boot image (patched jar md5 `15396933`)

The adapter's OHOS has no Android system services, so `getSystemService(...)`
returns null and many framework calls NPE/crash. The **universal fix** patches the
framework BCP jar so service fetchers return non-null managers with inert methods.
Patched smali in `framework-smali-patches/`:

| File | What it solves |
|------|----------------|
| `android_app_SystemServiceRegistry$88.smali` | ShortcutManager fetcher: `.catch ServiceManager$ServiceNotFoundException` → `const v0,0x0; goto :after` → `new ShortcutManager(ctx, null)` instead of throwing. |
| `android_app_SystemServiceRegistry$7.smali` | AlarmManager fetcher: same pattern → `new AlarmManager(null, ctx)`. |
| `android_content_pm_ShortcutManager.smali` | null-guard `getDynamicShortcuts`/`getManifestShortcuts`/`getPinnedShortcuts` (`if-nez mService` else empty `ArrayList`). |
| `android_app_AlarmManager.smali` | null-guard `canScheduleExactAlarms` (try/catch Throwable + `if-eqz mService` → return true). |
| `android_content_ContentResolver.smali` | null-guard `register`/`unregisterContentObserver` (no ContentService → NPE). This one unblocked the subscription/DNS path. |
| `RuntimeInit_KillApplicationHandler.smali` | selective uncaught-exception handler (swallow non-main-thread non-Error). **Status: INEFFECTIVE** — the runtime intercepts uncaught coroutine exceptions before any Java handler runs (`AdapterUEH` fires 0×). Documented for completeness; the working coroutine mitigation is the noice APK `a.smali` (§D). |

**PROVEN universal:** an *unguarded* noice (all 3 app-specific service guards
reverted) survives the ShortcutManager (Saved) and AlarmManager (add-alarm)
crashes with only the framework fix. This is the core Westlake win — the escape
from death-by-a-thousand-app-patches.

**Rebuild framework.jar + boot image** (the boot-regen cycle; full detail in
`docs/reference_boot_regen_cycle_2026-05-30.md`):

1. baksmali the class out of `framework.jar` (§2), apply the patch, reassemble
   with `scripts/SmaliAssemble.java` (§2), `zip framework.jar classes.dex`
   (preserve the manifest), re-baksmali to confirm the edit landed.
2. Pull **all 10** BCP jars from the device (use **relative** recv paths — hdc.exe
   mangles absolute WSL paths), swap in the patched framework.jar. Verify BCP
   order first: `strings /system/bin/appspawn-x | grep 'framework/.*\.jar'`.
3. Regenerate the boot image with the boot-regen script (which drives the host
   `dex2oat64`, `--instruction-set=arm --base=0x70000000 --compiler-filter=speed`,
   producing 30 segments: `boot.{art,oat,vdex}` + 9× `boot-<jar>.{art,oat,vdex}`):

   ```bash
   cd $HOME/openharmony
   WORK=/tmp/tagsoup-boot bash docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh
   # boot image -> /tmp/tagsoup-boot/out/boot-image/  (also out/adapter/*.jar)
   ```
4. Send all 30 boot files + the jar (size-verify + retry; 20-50 MB each) to
   `/system/android/framework/arm/` + `/system/android/framework/`, chcon
   `system_file`, `rm -rf /data/misc/appspawnx/dalvik-cache/*`, **reboot**.
5. HW-gate before relying on it: HelloWorld reaches `onResume` with zero
   `mark_sweep|Fatal|cppcrash|Class mismatch|InitWithoutImage`. Snapshot the
   current working boot+jar to `/data/local/tmp/<rollback>` BEFORE deploying.

### D. noice APK smali patches (apktool + debug-sign)

Patched smali shipped in `noice-smali-patches/`:
- `kotlinx-coroutines/a.smali` — **the coroutine fix.**
  `kotlinx.coroutines.a.a(CoroutineContext, Throwable)`
  (`handleCoroutineException`) is patched so a background-coroutine exception is
  swallowed/logged and only kills on the **main thread**. *Solves:* the
  subscription page (`查看订阅计划` → `loadPlans` → `listPlans`) threw an uncaught
  exception in the `i8.s` dispatcher when the flaky network fetch failed → process
  death. **Status: PROVEN** — subscription/Register flows survive instead of
  crashing. (This is the residual still needed in the APK because the framework
  RuntimeInit UEH is intercepted by the runtime — §C.)

(Earlier noice APK patches — ShortcutManager `d0/b.e`, AlarmManager
`repository/b.j`, tag rendering, the FlexboxLayout measure fix — are now redundant
with the framework fixes in §C and are documented in
`docs/noice-network-inet-gid-fix.md` / `docs/noice-content-population-findings.md`.
The coroutine patch is the one shipped here as it has no framework equivalent.)

**Rebuild + sign:**

```bash
java -jar $HOME/apktool.jar d base.apk -o noice_dec
# copy patched smali into noice_dec/smali*/kotlinx/coroutines/a.smali (match classesN)
java -jar $HOME/apktool.jar b noice_dec -o noice-unsigned.apk
$SDK/build-tools/<ver>/zipalign -p -f 4 noice-unsigned.apk noice-aligned.apk
$SDK/build-tools/<ver>/apksigner sign --ks ~/.android/debug.keystore \
  --ks-pass pass:android --out noice-coro-signed.apk noice-aligned.apk
```

(`apk_install` on this device accepts a debug-signed APK. For single-method edits
you can baksmali just the affected `classesN.dex`, edit, reassemble with
`SmaliAssemble`, and rezip — faster than a full apktool round-trip.)

### E. Native LD_PRELOAD shims

Build with the OHOS clang prebuilt, arm32 target, against the OHOS musl sysroot.
`libv4force.so` (md5 `7c3e5ece…`, 3 KB) is committed; rebuild it (adjust the
sysroot/`libc.so` paths to your OHOS tree):

```bash
$HOME/openharmony/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang \
  --target=arm-linux-ohos -fPIC -shared -O2 -nostdlib \
  -isystem out/rk3568/obj/third_party/musl/usr/include/arm-linux-ohos \
  -o libv4force.so native-libs/libv4force.c \
  out/rk3568/.../lib/arm-linux-ohos/libc.so
```

The same command pattern builds `libjdnshook.so` (from
`native-libs/libjdnshook_v2.c` — see §7; v1 `libjdnshook.c` is kept for history),
`libsetgidhook.so`, `libdnshook.so`, `libnetlog.so`.

### F. Native-TLS chain (the connectivity frontier)

The adapter ships a **stub** `com.android.internal.os.TlsShimProvider` registered
as the default SSL provider; every `$Sf.createSocket` throws *"TlsShim: no real
TLS connect"*, so ALL app HTTPS is non-functional by design. But the device has
**real native TLS** (`/system/lib/platformsdk/libssl_openssl.z.so` +
`libcrypto_openssl.z.so`; `openssl s_client` does a full handshake to trynoice).
The fix wires native OpenSSL into a Java `SSLSocket` and patches `$Sf.createSocket`
to return it.

Sources in `native-tls/`:
- `libtlsjni.c` / `libtlsjni.so` (committed, 6 KB) — JNI over the device's
  OpenSSL. `dlopen` `libssl_openssl.z.so`+`libcrypto`; `dlsym`
  `SSL_CTX_new/SSL_new/SSL_set_fd/SSL_ctrl(SNI=55)/SSL_connect/SSL_read/SSL_write/
  SSL_shutdown/SSL_get1_peer_certificate/i2d_X509`. Exports
  `Java_com_android_internal_os_TlsJni_{sslConnect,sslRead,sslWrite,sslClose,
  sslPeerCertDer}`. Default verify = none (trust-all, **test-grade**).
- `TlsJni.java` — the Java native-method holder.
- `TlsJniSocket.java` (+ `$TlsIn/$TlsOut/$TlsSession` nested classes) — an
  `SSLSocket` subclass wrapping okhttp's connected plain socket; fd via reflection
  (`Socket.impl → SocketImpl.fd → FileDescriptor`); `startHandshake → sslConnect`;
  streams → `SSL_read/write`; `getSession` → `TlsSession` with peer cert from
  `sslPeerCertDer`.
- `TlsShimProvider_Sf_DexClassLoader.java` (`SfGen`) — the `$Sf.createSocket`
  body that loads `TlsJniSocket` via a **DexClassLoader** from
  `/system/android/framework/tlsjni-extra.dex` (parent =
  `Object.class.getClassLoader()` = boot, so `SSLSocket` resolves; the class is
  runtime-verified, not boot-verified).
- `TlsShimProvider_Sf_patched.smali` — the patched `$Sf` smali.
- `tlsjni-extra.dex` (committed, 10 KB) — the compiled TlsJni + TlsJniSocket.
- `okhttp-bcp-Platform-PATCHED.smali` — an attempted patch to skip okhttp's
  conscrypt path (see the wall below).
- `NetTest.java` — the headless HTTPS test harness (§8).

**Two hard-won constraints (else it fails boot-image verification):**
1. Compile against **android-34** (the device matches it) with `-source 8
   -target 8`, and do **NOT** implement `getPeerCertificateChain` (the device boot
   LACKS `javax.security.cert.X509Certificate` → that method makes the class fail
   boot-image verification → `NoClassDefFoundError`).
2. Use **named** nested classes only (d8 NPEs on anonymous classes).
3. Keep `TlsJni`+`TlsJniSocket` **OUT of the BCP** — put them in the standalone
   `tlsjni-extra.dex` loaded via DexClassLoader. (Inside the BCP the class is
   marked erroneous at boot for the same conscrypt reason; out-of-BCP it is
   runtime-verified.)

**Build the native-TLS chain:**

```bash
# 1. libtlsjni.so (needs the NDK jni.h)
$HOME/openharmony/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang \
  --target=arm-linux-ohos -fPIC -shared -O2 -nostdlib \
  -I$HOME/openharmony/interface/sdk_c/.../<jni.h dir> \
  -isystem out/rk3568/obj/third_party/musl/usr/include/arm-linux-ohos \
  -o libtlsjni.so native-tls/libtlsjni.c \
  out/rk3568/.../lib/arm-linux-ohos/libc.so

# 2. compile TlsJni + TlsJniSocket -> classes -> d8 -> tlsjni-extra.dex
mkdir -p /tmp/tlsjava/classes /tmp/tlsjava/dexout
javac -source 8 -target 8 -bootclasspath $SDK/platforms/android-34/android.jar \
  -d /tmp/tlsjava/classes native-tls/TlsJni.java native-tls/TlsJniSocket.java
$SDK/build-tools/<ver>/d8 --min-api 28 --output /tmp/tlsjava/dexout \
  $(find /tmp/tlsjava/classes -name '*.class')
cp /tmp/tlsjava/dexout/classes.dex native-tls/tlsjni-extra.dex   # this is tlsjni-extra.dex

# 3. build the DexClassLoader $Sf and splice it into TlsShimProvider$Sf:
#    - compile SfGen (TlsShimProvider_Sf_DexClassLoader.java), d8, baksmali it,
#    - transplant its createSocket body into the real $Sf smali
#      (TlsShimProvider_Sf_patched.smali) inside adapter-runtime-bcp.jar's classes.dex,
#    - reassemble (SmaliAssemble), rezip the jar, then BOOT REGEN (§C step 3) so the
#      patched adapter-runtime-bcp.jar is baked into the boot image.
```

(`scripts/tls_dcl.sh` automates steps 3 + boot regen + deploy; `scripts/deploy_tls.sh`
is the BCP-resident variant.)

> **HONEST native-TLS status (the current connectivity wall):** native TLS
> `createSocket` **WORKS** — the DexClassLoader loads `TlsJniSocket`,
> `libtlsjni.so` loads, the socket instantiates (`tls.log`:
> `TlsJni: libtlsjni loaded OK` + `TlsJniSocket: ctor host=api.trynoice.com`).
> **But `startHandshake` is never reached:** between createSocket and
> startHandshake, okhttp (both the BCP `com.android.okhttp` and noice's bundled
> `okhttp3` AndroidPlatform) casts/`instanceof`-checks
> `com.android.org.conscrypt.OpenSSLSocketImpl` (for `setHostname`/ALPN), which is
> **erroneous in the boot image** (conscrypt is incomplete — the same reason the
> stub provider exists) → `NoClassDefFoundError: Class not found using the boot
> class loader`. To finish: either make `OpenSSLSocketImpl` loadable (fix/stub
> conscrypt `NativeCrypto` — deep) **or** patch okhttp's `configureTlsExtensions`
> to skip the conscrypt path (`okhttp-bcp-Platform-PATCHED.smali` is the WIP).

---

## 6. DEPLOY instructions

All transfers use the **send-via-Windows-path** quirk (§0). Scripts in `scripts/`
encapsulate this.

| Target | Deploy method | Reboot? |
|--------|---------------|---------|
| `liboh_adapter_bridge.so` | `cp` to `/system/lib/` + `/system/android/lib/`, chcon `system_lib_file`; loads **per-child** → `aa force-stop noice` + clear `$BUNDLE/oat` + relaunch | No |
| `libhwui.so` | `cp` to `/system/android/lib/libhwui.so`, chcon `system_lib_file`; loads per-child → force-stop + relaunch | No |
| noice `base.apk` | `cp` to `$BUNDLE/base.apk`, chmod 644, chown 0:0, chcon `app_install_file`; `rm -rf $BUNDLE/oat` + `code_cache`; loads per-child → force-stop + relaunch | No (a reboot also works via dex boot-scan) |
| `framework.jar` + boot image (30 segs) | to `/system/android/framework/` + `/system/android/framework/arm/`, chcon `system_file`; `rm -rf /data/misc/appspawnx/dalvik-cache/*` | **Yes** |
| `adapter-runtime-bcp.jar` + boot image | same as framework.jar | **Yes** |
| `tlsjni-extra.dex` | `cp` to `/system/android/framework/tlsjni-extra.dex`, chmod 644, chcon `system_file` | (with the boot regen) |
| native shims (`lib*.so`) | `cp` to `/system/android/lib/`, chcon `system_lib_file`; **add to `start_asx.sh`'s LD_PRELOAD** (§3) + restart appspawn-x | restart appspawn-x |
| `cacerts.tgz` | extract into `/system/etc/security/cacerts/`, chmod 644, chcon `system_file` | No |

**Key efficiency win:** bridge / libhwui / base.apk all load **per-child**, so
`force-stop noice + clear $BUNDLE/oat + relaunch` picks up new `.so`/`.dex` with
**no appspawn-x restart and no reboot**. Only framework.jar / adapter-runtime-bcp
/ boot-image / native-shim changes need a reboot or appspawn-x restart.

**Scripts (adapt `HDC`/`WINDIR` to your host):**
- `scripts/deploy_v4.sh` — installs `libv4force.so`, adds it to LD_PRELOAD,
  restarts appspawn-x, re-enables IPv6 (to prove libv4force is the fix), cold-
  launches noice with empty cache to force a live fetch, screenshots, dumps netlog.
- `scripts/deploy_coro.sh` — installs the coroutine-fixed APK, reboots, brings up,
  runs the regression: taps subscription ×3 + Register ×3, asserts pid SURVIVED.
- `scripts/deploy_ca.sh` — installs the CA bundle, re-tests subscription live load.
- `scripts/deploy_tls.sh` — pushes `libtlsjni.so` + `adapter-runtime-bcp.jar` +
  boot image, reboots, brings up, runs NetTest, prints `/data/local/tmp/httptest.log`.
- `scripts/tls_dcl.sh` — the DexClassLoader variant: repackage jar + extra dex,
  boot regen, deploy jar+boot+`tlsjni-extra.dex`, reboot, NetTest.
- `scripts/broker_fix.sh` — confirms noice vs shell netns, grants
  `broker_sock_permission_map[uid]`, relaunches + NetTest (the cgroup-BPF probe).
- `scripts/livetest.sh` — cold-launch with empty cache+db to force a live library
  fetch; reports screenshot size + okhttp/network log lines.
- `scripts/navtour.sh` — full nav tour via the tap control channel.

---

## 7. Connectivity — the 5-layer stack

Live HTTPS needs **all five** layers. Layers 1–4 are done + proven; layer 5 is the
current wall.

### Layer 1 — DNS (LD_PRELOAD `libjdnshook.so` v2 + `libdnshook.so`)

OHOS musl `getaddrinfo` routes through netsys per-netid; the sandboxed child has no
netid → `EAI_NONAME`. `libdnshook.so` does a **direct-UDP A-query to 8.8.8.8:53**.
But the JVM/libcore DNS path uses different JNI entrypoints that EPERM separately:
- v1 `native-libs/libjdnshook.c` hooked only `android_getaddrinfofornet`.
- **v2 `native-libs/libjdnshook_v2.c` is the fix** — the libcore
  `Linux.android_getaddrinfo` JNI on this adapter actually calls
  `android_getaddrinfofornetcontext`. v2 hooks **all three variants**
  (`android_getaddrinfofornet` + `android_getaddrinfofornetcontext` +
  `android_getaddrinfo`), each delegating to `getaddrinfo()` → libdnshook's
  direct-UDP. **Status: ✅ DNS resolves** (`api.trynoice.com -> 35.94.160.101`;
  EPERM gone). Deploy v2 as `/system/android/lib/libjdnshook.so`.

### Layer 2 — inet gids (LD_PRELOAD `libsetgidhook.so`)

appspawn-x skips `OH_TLV_INTERNET_INFO`, so children lack AID_INET →
`socket(AF_INET)` EPERM. The shim intercepts `setgroups` and appends gids
**3003** (inet) + **3004** (net_raw). **Status: ✅.**

### Layer 3 — socket family (LD_PRELOAD `libv4force.so`)

The JVM creates dual-stack `AF_INET6` TCP sockets and connects to IPv4-mapped
`::ffff:a.b.c.d`; this board has **no IPv6 route** so those connects fail
(`errno=115`). `libv4force.so` forces `AF_INET6` STREAM sockets to `AF_INET`,
rewrites IPv4-mapped connect targets to `sockaddr_in`, and swallows IPv6 sockopts.
**Status: ✅** (committed: `native-libs/libv4force.so`, md5 `7c3e5ece…`).

### Layer 4 — cgroup-eBPF socket grant (`bpfgrant` + `bpf-analysis/netsys-ebpf.c`)

Even with inet gids, OHOS netsys runs a **cgroup-eBPF program** that gates
`socket()` at creation. The decompiled/recovered source is in
`bpf-analysis/netsys-ebpf.c` — the relevant program:

```c
SEC("cgroup_sock/inet_create_socket")
int inet_create_socket(struct bpf_sock *sk) {
    void *map_ptr = &oh_sock_permission_map;
    if (bpf_get_netns_cookie(sk) != bpf_get_netns_cookie(NULL))
        map_ptr = &broker_sock_permission_map;          // different netns -> broker map
    __u32 uid = (__u32)bpf_get_current_uid_gid();
    sock_permission_value *value = bpf_map_lookup_elem(map_ptr, &uid);
    if (value == NULL) return 1;   // not an appspawn hap -> allow (native)
    if (*value == 0)   return 0;   // explicit deny
    return 1;                       // allow
}
```

**Key scheme = plain `uid`** (not iface|uid as first guessed). value 0 = DENY,
NULL/!=0 = ALLOW. BMS had added noice's uid **13731** with val 0. Grant it:

```bash
sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map"      # sets [13731]=1
sh "/data/local/tmp/bpfgrant 13731 broker_sock_permission_map"  # in case of netns mismatch
```

noice is the **same netns** as the shell, so `oh_sock_permission_map` is the one
that matters; the broker grant is belt-and-suspenders. **Status: ◑ flaky after a
cold reboot** — `conntest` (shell cgroup, uid 13731) gets `socket rc OK` after a
fresh grant, but noice (appspawn cgroup, same uid) sometimes still `EPERM`s at DNS
on a cold boot (6/6 cold cycles EPERM'd in one multi-reboot test; warm restarts
succeed). Re-grant + a warm appspawn-x restart is the workaround. This intermittent
gate sits *below* TLS, so it can mask the TLS-layer result.

### Layer 5 — TLS (native `libtlsjni` + the okhttp↔conscrypt wall)

The adapter's stub `TlsShimProvider` is replaced by the native-TLS chain (§5F).
**Native TLS `createSocket` ✅ proven; the wall is okhttp's hard dependency on the
erroneous `com.android.org.conscrypt.OpenSSLSocketImpl`** (§5F status box). This is
the **current end-to-end blocker** for live HTTPS.

### CA trust store

The device's `/system/etc/security/cacerts/` was **empty**, so HTTPS trust
validation fails. Install the bundled roots (`scripts/deploy_ca.sh`, source
`ca-store/cacerts.tgz`, md5 `888d018d…`):

```bash
sh "mount -o remount,rw / 2>/dev/null; mkdir -p /system/etc/security/cacerts
    cd /system/etc/security/cacerts && tar xzf /data/local/tmp/ca.tgz
    chmod 644 *.0; chown 0:0 *.0; chcon u:object_r:system_file:s0 *.0"
```

(With native TLS using verify=none this is test-grade redundant, but it is the
correct layer for a real handshake and is kept.)

> **HONEST connectivity summary:** DNS (v2, ✅), inet gids (✅), socket family
> (libv4force, ✅), CA store (✅), and native-TLS createSocket (✅) are all done.
> The **cgroup-BPF grant is flaky after a cold reboot (◑)** and the **okhttp↔
> conscrypt coupling is the TLS-layer wall (❌)**. The low-level path is proven (a
> raw probe + the native socket reach the real `cdn.trynoice.com` nginx), but the
> app's **live HTTPS data still does not load** end-to-end. The UI is populated
> from a cached library meanwhile. Do not assume live HTTPS works.

---

## 8. TEST instructions

Testing does **not** need reliable WMS focus. The key insight: the **control
channel `/data/local/tmp/noice_tap`** (bridge `dispatchTouchViaViewRoot` →
noice's own ViewRootImpl) is **focus-independent** — it drives the UI even when
WMS focus is on the launcher. (`uinput` is focus-*dependent* → flaky; use it only
for drag widgets, which need foreground.) The only requirement is that noice's
window is **foreground/present**, which a cold launch satisfies.

### Control channel

```
echo "N"   > /data/local/tmp/noice_tap   # bottom-nav tab N (1=library 2=saved 3=timer 4=alarms 5=account)
echo "X Y" > /data/local/tmp/noice_tap   # in-process tap at raw window coords (X,Y)
echo "40 60" > /data/local/tmp/noice_tap # toolbar back-arrow (return from a 2nd-level page)
```

Drag widgets (slider / time-dial / scroll) use uinput (foreground only):
`uinput -T -m X1 Y1 X2 Y2 DURATION_MS`.

### Coordinate map (720×1280 portrait, validated)

```
tab row y=1218 : library=72 saved=216 timer=360 alarms=504 account=648
library row1 buttons y=337 : info=84 download=204 play=324 volume=492
add-alarm FAB = (648,1090) ; toolbar back-arrow = (40,60)
account submenu : register=(360,257) login=(360,383) subscription=(360,513)
                  settings=(360,738) support=(360,866) about=(360,994)
settings sliders: fade-in handle ~ (125,490) ; fade-out handle ~ (125,697)
settings toggles: ignore-audio-focus ~ (635,835) ; media-buttons ~ (635,978)
time-picker dial: center ~ (360,615) ; "9" ~ (150,615) ; "3" ~ (560,615)
                  OK 确定 ~ (598,972) ; Cancel 取消 ~ (447,972)
```

### PASS/FAIL model (in-process truth, not pixels)

- **CRASH (hard fail):** the uid-13731 pid changed/died after the action (the
  fixtures re-find the pid before/after each tap; same pid = survived).
- **NAV (soft):** the target fragment/class marker appears in the per-child stderr
  `/data/service/el1/public/appspawnx/adapter_child_<pid>.stderr` (markers fire on
  first class-load; quiet on re-visit, so treated soft).
- **RENDER (soft):** screenshot byte size — `snapshot_display -f
  /data/local/tmp/u.jpeg`, **>45 KB = populated**, <25 KB = blank/race. Recv via a
  relative path.

### Fixtures (in `test-fixtures/`)

- **`ux_full_fixture.sh`** — **the per-page / per-submenu / per-widget fixture**
  (the priority test). `bash test-fixtures/ux_full_fixture.sh`. It embeds the
  coordinate map above and:
  - brings noice up populated (`ensure_up`: force-stop → restore
    db.bak+cdn-cache → bpfgrant → cold launch → require shot>45 KB, up to 4 tries),
  - walks **every** screen + every account submenu via the control channel, and
    drives the interactive widgets with **uinput drags** (time-dial 9→3, fade-in/
    fade-out sliders, settings-list scroll),
  - verifies each step by pid-alive + stderr marker + screenshot size; on a crash
    it logs `[FAIL]` and **self-recovers** (`ensure_up` relaunches) so one failure
    doesn't abort the run; prints a `PASS=/FAIL=/WARN=` summary + `UXFULL_DONE`,
    saves all shots to `uxshots/`, exits non-zero iff any hard FAIL.
- **`uxfixture.sh`** — the **13-step** focus-independent regression harness.
  `bash test-fixtures/uxfixture.sh [ROUNDS]`. Same in-process-truth model; steps:
  tab1 library, sound-info(84,337), volume(492,337), play(324,337), tab2 saved,
  tab3 timer, tab4 alarms, add-alarm FAB(648,1090), tab5 account,
  subscription(360,513), with dim-area dismiss taps. Self-recovery: restores the
  cached library, reboots on no-spawn, relaunches on crash. **Result 2026-06-05:
  26/26 PASS, 0 FAIL over 2 rounds** (all pages crash-free incl subscription, whose
  network-flaky crash is intermittent — run more rounds to measure the flake rate).
- **`navtour.sh`** — a simpler **page tour** that captures each page (library,
  SoundInfo, volume, saved, sleep-timer, alarms, add-alarm, account) to disk.
- **`nettest/NetTest.java`** — the **headless HTTPS harness**. A
  `com.nettest.NetTest` class (`HttpsURLConnection` to api/cdn.trynoice.com on a
  bg thread, full exception → `/data/local/tmp/httptest.log`) **injected into
  noice's `MainActivity.onCreate`** (runs every launch, no UI/nav/cache
  dependency). This is how the connectivity layers were isolated end-to-end —
  inject, rebuild+sign the APK (§5D), launch, then `cat /data/local/tmp/httptest.log`.
  It gives exact errors (DNS EPERM → conscrypt `NoClassDefFoundError`, etc.).

### Per-page / per-submenu reference

Coordinates are control-channel taps unless noted. "Expected" = the crash-free
result; cross-reference the named screenshot in `screenshots/`.

| # | Page / submenu | How to reach | Expected result | Screenshot |
|---|----------------|--------------|-----------------|------------|
| 1 | Library (声音库) | `echo 1` (or cold launch) | populated list: LIFE group, Birds/Crickets/Heartbeat/Purring-Cat, each name·tags·★ + 4-button row | `p1_library.jpeg` |
| 2 | SoundInfo (关于这个声音) | info button `echo "84 337"` | Birds·Animals + 流畅地重复 + 媒体资源 license credits (2nd render surface) | `p2_soundinfo.jpeg` |
| 3 | Volume bottom-sheet | volume button `echo "492 337"` | 音量 sheet (slider) over the live library, no launcher bleed | `p3_volume.jpeg`, `p3_volume_back.jpeg` |
| 4 | (back to library) | `echo "40 60"` / dim-tap | returns to list | `p4_back.jpeg` |
| 5 | Saved / Presets (预设) | `echo 2` | presets page (ShortcutManager NPE fixed framework-wide) | `p5_saved.jpeg` |
| 6 | Sleep-timer | `echo 3` | sleep-timer page | `p6_sleeptimer.jpeg` |
| 7 | Alarms | `echo 4` | alarms list | `p7_alarms.jpeg` |
| 8 | Add-alarm TimePicker | FAB `echo "648 1090"` | 选择时间 clock face + 取消/确定 (AlarmManager exception fixed) | `p8_addalarm.jpeg` |
| 9 | (back from alarm) | `echo "40 60"` | returns | `p9_back.jpeg` |
| 10 | Account | `echo 5` | account page with the 6 submenu rows | `p10_account.jpeg`, `L2_account.jpeg` |
| 11 | Register / Sign-up | `echo "360 257"` | register form (survives; coroutine fix) | `p11_register.jpeg`, `L2_register.jpeg`, `L2_register_back.jpeg` |
| 12 | Login / Sign-in | `echo "360 383"` | login form | `L2_login.jpeg` |
| 13 | Subscription (查看订阅计划) | `echo "360 513"` | page shown; **data-load depends on TLS/conscrypt** — on a network/TLS failure shows "网络无法访问 / 发生未知错误" (crash-proofed by the coroutine fix) | `subscription-error.jpeg` |
| 14 | Settings | `echo "360 738"` | settings (fade sliders, toggles) | `L2_settings.jpeg`, `L2_settings_back.jpeg` |
| 15 | Support | `echo "360 866"` | support / donate page | `L2_support.jpeg`, `L2_support_back.jpeg` |
| 16 | About | `echo "360 994"` | about / open-source libraries | `L2_about.jpeg`, `L2_about_back.jpeg` |

(The `pN_*` shots are the primary page tour; the `L2_*` shots are the account
2nd-level submenus + their back-navigations.)

---

## 9. Honest limitations & flakiness

- **Runtime un-rebuildable.** `liboh_android_runtime.so` `16e08711` is a fixed
  blob; local rebuilds regress noice at render-thread init. Every runtime-level
  gap (KEY-consumer stub, missing VelocityTracker/AudioTrack JNI, the
  uncaught-coroutine handler) is worked around from the **bridge** or by accepting
  the limitation — never by rebuilding the runtime.
- **Live HTTPS not end-to-end** (§7) — DNS/gids/socket-family/CA/native-TLS-
  createSocket done; the **okhttp↔conscrypt coupling** blocks `startHandshake`,
  and the **cgroup-BPF grant is flaky after a cold reboot**. UI uses a cached
  library.
- **RuntimeInit UEH ineffective** (§5C) — the runtime intercepts uncaught
  exceptions first; the coroutine crash is mitigated in the noice APK instead.
- **Audio output unsolved** — the play *click* binds SoundPlaybackService +
  ExoPlayer, but the service never runs its player and there's no AudioTrack path
  (`android.media.AudioTrack` isn't in the device framework.jar). OH_AudioRenderer
  NDK exists, so an AudioTrack→OH bridge is buildable in principle — separate
  multi-component project.
- **Focus reliability** — only a **cold** launch reliably lands WMS focus on
  noice; warm re-`aa start` and `moveMissionToFront` do not (missionId lookup
  returns -1 on this AMS). Idle screen-lock and stray unlock swipes steal focus →
  "not clickable". Mitigations: keep the screen awake (`power-shell timeout -o
  86400000`), use the focus-independent tap control channel for tests,
  cold-launch to re-focus. `RequestFocus(windowId)` returns rc=1 but doesn't grab.
- **Spawn/render flakiness** — `aa force-stop` degrades AMS after ~4 (reboot to
  clear); ~50% of cold launches hit a blank-list flow race (reroll until
  shot>45 KB; mitigated by restoring the cached db/cache); an intermittent
  bad-boot spin in `installSettingsContentProviderStub` burns a boot (reboot to
  reroll); the displayId compositing race sometimes renders the launcher over
  noice in screenshots (noice is actually on DisplayId 0, ZOrd 1 above the
  launcher — the "drift" is triggered by screen-lock/HOME/stray swipes, not
  spontaneous).
- **Subscription network failure can't be force-reproduced** on the bench —
  direct-UDP DNS ignores `/etc/hosts`+`resolv.conf`, so the fetch can't be made to
  fail deterministically (intermittent ~10-20%).

---

## 10. Artifacts: component md5s & provenance

| Component | Deployed md5 | Committed here? |
|-----------|--------------|-----------------|
| `liboh_android_runtime.so` | `16e08711` | No — un-rebuildable base blob; record only. |
| `liboh_adapter_bridge.so` | `2967c30c` | No — buildable from `bridge-src/` (§5A). |
| `libhwui.so` | `8b8f84ec` | No — buildable from `bridge-src/hwui_oh_abi_patch.cpp` (§5B). |
| `framework.jar` (patched) | `15396933` | No — regen from `framework-smali-patches/` + boot regen (§5C). |
| `adapter-runtime-bcp.jar` (patched) | — | No — regen from the native-TLS `$Sf` patch + boot regen (§5F). |
| boot image (30 segments) | — | No — large; regenerate per §5C. |
| noice `base.apk` (coro-signed) | `384134d8…` (see `ARTIFACT-INVENTORY.txt`) | No — rebuild from `noice-smali-patches/` + a noice base APK (§5D). |
| `libv4force.so` | `7c3e5ece…` | **Yes** (3 KB) — `native-libs/libv4force.so`. |
| `libtlsjni.so` | `e248cc47…` | **Yes** (6 KB) — `native-tls/libtlsjni.so`. |
| `tlsjni-extra.dex` | `01ade5c4…` | **Yes** (10 KB) — `native-tls/tlsjni-extra.dex`. |
| `cacerts.tgz` | `888d018d…` | **Yes** (133 KB) — `ca-store/cacerts.tgz`. |
| screenshots (`p*`, `L2_*`) | — | **Yes** (~1.4 MB) — `screenshots/`. |

Large/un-rebuildable binaries (runtime `.so`, the patched JARs, the boot image,
`base.apk`) are intentionally **not** committed; their md5s + provenance are here
and in `ARTIFACT-INVENTORY.txt` so they can be reproduced or matched. All
sources, scripts, smali patches, the eBPF reference, the test fixtures and the
small data blobs are in this repo.
