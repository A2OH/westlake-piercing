# Reproduce noice on the OHOS arm64 dev board

Written so another agent can pick this up cold. Board `5cdbf6af…`, aarch64, OHOS 6.1.0.31.
Read `ARTIFACT-MAP.md` first for what lives where, and `README.md` for what each fix does and why.

## 0. Prerequisites

**Read this whole section before starting.** Most of what the build needs is NOT in this repo — it is
large third-party source, licensed SDKs, and one binary that cannot be rebuilt. The repo gives you the
*deltas and the procedure*; you must supply the inputs.

### 0.1 Host setup
- **hdc runs on WINDOWS** (`$WIN_DEV_ROOT/ohos-tools/hdc.exe`) and **cannot read WSL paths** — every
  transfer stages through a Windows dir. `recipes/env.sh` provides `push()`/`recv()` that do this and
  **verify the transfer**, because hdc exits 0 on several real failures.
- `. recipes/env.sh` before anything. It sets one knob, **`WLROOT`** (default `$HOME`), the directory
  holding all source trees below, and auto-detects `WIN_DEV_ROOT`.
- **`ADAPTER_ROOT` must be exported** or the bridge build silently misresolves every path (it tries to
  regenerate `IInputConstants.h` and hits an absolute aidl path from the original author's machine).
  `env.sh` exports it; do not bypass env.sh.

### 0.2 Source trees / SDKs required at `$WLROOT` (none are committed)
| Tree | Needed by | Note |
|---|---|---|
| `android-sdk` | every dex recipe | build-tools **34.0.0** (`d8`, `zipalign`), and dexlib2 **3.0.3** + smali-util 3.0.3 + guava 31.1-jre under `cmdline-tools/latest/lib/external` |
| `openharmony` | **all** native builds | supplies the **OHOS prebuilts clang** at `prebuilts/clang/ohos/linux-x86_64/llvm`. ★Must be this clang, not the NDK's — the board's libs use the `__h` libc++ ABI namespace, the NDK's is `__n1`, and a `__n1` bridge cannot resolve any `std::string`-passing OHOS API |
| `ohos-sdk-6.1` | all native builds | `linux/native` musl sysroot |
| `ohos-6.1-src` | bridge | OHOS inner-API headers (window_manager, graphic_2d, bundle_framework) |
| `aosp-15` | bridge | `androidfw`, `libcutils` headers + a few .cpp |
| `third_party/sqlite` | bridge | amalgamation, compiled as **C99** (it will not compile as C++) |
| `aosp-android-11` | libhwui | hwui sources |
| `aosp-14-minikin` | minikin | note **14**, not 15 |
| `aosp-14-base` | minikin stack | |
| `art-latest`, `art-universal-build` | appspawn-x / libart headers | ⛔ do **not** rebuild libart — see 0.4 |
| `bridge-build` | only if you build from a live tree | The bridge sources are now committed here as `bridge-full-src/` (96 `.cpp`, 391 files); `build_bridge_arm64.sh` uses that by default. Set `BRIDGE_SRC=` to override |
| `bridge-build-arm64` | bridge link step | needs `out/board_libs/` = the ~60 OHOS `.so` files **pulled off the board** to link against, plus the built `libbridge_compat.so`. Object/output dirs are created under it |

### 0.3 The board must already carry the base install
⚠️**This kit patches a working westlake install; it does not create one.** `run406.sh` launches
`./appspawn-x` + `./spawn_client` from `/data/local/tmp/asx` and `aa start`s an installed bundle. On a
stock or freshly reflashed board none of that exists and every step below fails at step 1.

Required on-device before anything here applies:
```
/data/local/tmp/asx/       appspawn-x, spawn_client, libart.so, liboh_adapter_bridge.so,
                           liblog_shim.so, board_libs/libbridge_compat.so, noice.apk,
                           libicuuc.so + libicui18n.so + icudt66l.dat (ICU 66), libminikin.so,
                           appspawn_x_sandbox.json, framework-res.apk
/data/local/tmp/asx/fw/    the 10 BCP jars + the boot image (boot*.art/.oat/.vdex)
                           ★adapter-runtime-bcp.jar must be the build with directLaunchNoBms /
                             ASX_DIRECT_LAUNCH, or direct launch never happens
/data/local/tmp/           run406.sh, touchfwd
/system/android/lib/       libhwui.so   (does NOT exist for arm64 on a stock board — must be built)
```
Plus: the noice bundle **registered** (`.app`→`.apk` one-byte patch, then `bm install`), `setenforce 0`,
and the netsys eBPF grant if you need cold-boot networking. `appspawn-x` hardcodes this layout
(`main.cpp seedRequiredEnvs()`), so the paths are not negotiable.
★A whole directory can be staged in one shot: `hdc file send <dir> /data/local/tmp/` (≈200 MB in ~9 s).

### 0.4 libart is a binary prerequisite — treat it as irreplaceable
The only known-good `libart.so` is md5 **`e1af9bb570b6ff1cae1419b9ea86c6cf`**, 22,552,176 bytes.
⛔**Do not rebuild it.** A full rebuild reaches exact marker and undefined-symbol parity and still
SIGSEGVs the child; the drift is outside the art tree's git history.
Known copies: `$WIN_DEV_ROOT/wlstage/libart_good.so` on the host, and
`/data/local/tmp/asx/libart.so.bak-e1af9bb5` on the board.
★Neither is in version control. **Before touching the board, copy it somewhere durable** — the other
`libart*.so` files lying around the host are all different builds and none of them work.

### 0.5 Put the host drivers on your PATH
`scripts/host/*.sh` are written to be run as `~/wl_*.sh`. Install them once:
```bash
cp scripts/host/*.sh ~/ && chmod +x ~/wl_*.sh
```

## 1. Bring the board up

```bash
. recipes/env.sh
board_online                           # or: $HDC list targets — must show 5cdbf6af…
```
**If it lists nothing:** check Windows still enumerates the device
(`Get-PnpDevice | ? InstanceId -like 'USB*'`). If you see `APP Mode`, the board is powered and
connected but **hdcd/USB-debugging is not serving** → power-cycle and re-enable USB debugging.
WiFi debug is **not** a fallback unless `hdc tmode port 8710` was armed earlier over USB — arm it
now, while USB works, so a future drop is recoverable.

A first launch, only to confirm the base install is alive:
```bash
~/wl_restart.sh                        # kills stale children, launches, waits for a frame
# expect: UP: t=16s swaps=N alive=1 children=1
```
★A stale child silently steals taps — always use `wl_restart.sh`, never a bare `aa start`.
★Startup is flaky: a `TIMEOUT` on the first attempt is normal, retry 2-3×.

## 2. Build and deploy (in this order)

⚠️**Deploy everything BEFORE verifying.** The child process loads its jars and `.so`s at startup, so a
child launched in step 1 is still running the *old* artifacts. Verifying against it reads stale markers
and reports a false green. Nothing below restarts the app for you — `wl_deploy.sh` only pushes the
bridge — so the explicit restart at the end of this section is required, not optional.

```bash
# 1. native bridge
scripts/build/build_bridge_arm64.sh          # -> liboh_adapter_bridge.so
~/wl_deploy.sh mytag                         # pushes it

# 2. minikin (the font fix). ★The build compiles whatever is in $WLROOT/aosp-14-minikin --
#    a clean tree builds the CRASHING version. Apply the committed patch first:
( cd $WLROOT/aosp-14-minikin && git apply --check ../westlake-arm64/arm64-session-2026-07-29/minikin/Font.cpp.hb-owns-font-bytes.diff \
  && git apply ../westlake-arm64/arm64-session-2026-07-29/minikin/Font.cpp.hb-owns-font-bytes.diff )
grep -q 'hb_blob_create' $WLROOT/aosp-14-minikin/libs/minikin/Font.cpp || echo "PATCH NOT APPLIED"
scripts/build/build_minikin_stack_arm64.sh   # -> libminikin.so
push $WLROOT/bridge-build-arm64/out/libminikin.so $ASX/libminikin.so && dsh "chmod 755 $ASX/libminikin.so"

# 3. boot-classpath + dex surgery
recipes/build-ohaf-jar.sh                    # BCP helpers: TLS, service binds, audio focus, stubs
recipes/patch-noice-apk.sh                   # proxy dex surgery -> sound library loads
recipes/patch-framework-jar.sh               # 6 IActivityManager sites

# 4. NOW restart, so the child actually loads all of the above
~/wl_restart.sh                              # confirm it boots before trusting any verification
python3 recipes/signin.py                    # optional: real account (subscriptions 401 -> 200)
```
★The build script **exits 0 even when a file fails to compile**. It now prints its failure set and
compares it to the expected one; treat any deviation as "you broke something and the linked .so is
missing your change". Do not just count lines — a *different* file failing keeps the count the same.

⚠️**The patch recipes are not idempotent and have no host-side baseline.** Each one pulls the artifact
currently on the device, patches it, and pushes it back. Run one twice and the second run operates on
already-rewritten bytecode. The only pristine copies are the on-device `*.bak-*`/`*.pre*` files listed
in ARTIFACT-MAP.md — a board wipe destroys them. **Pull those baselines to the host before you start**,
and restore from the host copy rather than re-running a recipe if something goes wrong.

## 3. Verify

★**Verify against a log written AFTER the restart in step 2.4.** `ls -t | head -1` gives the newest
child log, but if the app died and an older child is still the newest file you will read a stale run —
so check liveness first.

```bash
. recipes/env.sh
[ "$(alive)" -ge 1 ] || echo "app is NOT running — everything below is meaningless"
L=$(childlog)

# ★These are ASSERTIONS, not greps. A bare `grep marker` succeeds on a line that says the marker
#   FAILED, which is how a broken run reads as green.
$HDC shell "grep -a 'HANDSHAKE OK' $L"                    # want: proto=TLSv1.3 ... verify=OK
$HDC shell "grep -ac 'WRONG' $L"                          # regex self-test failures: want 0
$HDC shell "grep -a 'WESTLAKE-442' $L | tail -1"          # want the 13/13 summary line, not just a hit
$HDC shell "grep -a 'WESTLAKE-453' $L"                    # CursorWindow inflated via malloc
$HDC shell "grep -a 'WESTLAKE-468' $L | grep -v 'not found'"   # audio-policy natives bound, rc=0
$HDC shell "grep -ac 'hb_face_reference_table' $L"        # font crashes: want 0
```
**What each check cannot tell you:**
- TLS / CursorWindow markers are **absent on a cached run** — they only appear when the app actually
  goes to the network or reads a large row. Clear `.../cache/*` first, or absence proves nothing.
- The font check is only meaningful **after stress**: the crash was intermittent (~1 per 10-15 taps).
  Cycle the five tabs 20+ times, then re-check.
- `~/wl_shot.sh` sizing (~128 KB rendered vs ~58 KB error page) is a **content-dependent heuristic**,
  not a test. Look at the screenshot.

## 4. Driving the UI

The tap channel is a file **on the device** — these must run through `$HDC shell`, or you will silently
create `/data/local/tmp/noice_tap` on your WSL host and nothing will happen:
```bash
$HDC shell 'echo "324 385"            > /data/local/tmp/noice_tap'  # tap (absolute coords)
$HDC shell 'echo "1140 1668 480 1668" > /data/local/tmp/noice_tap'  # drag
$HDC shell 'echo back > /data/local/tmp/noice_tap'   # dismiss a dialog  ← the ONLY reliable way
$HDC shell 'echo v    > /data/local/tmp/noice_tap'   # dump widget tree with absolute rects
$HDC shell 'echo w    > /data/local/tmp/noice_tap'   # list window roots
```
`scripts/host/wl_cmd.sh <cmd>` wraps this for you. Tabs at y=1829:
Library 119 · Presets 359 · Sleep Timer 599 · Alarms 839 · Account 1079.
Helpers: `~/wl_page_widgets.sh <tabX> <LABEL>`, `~/wl_click_all.sh <tabX> <LABEL>`.

**Traps that will waste your time:**
- **Physical touch needs `touchfwd`** (now started by `run406.sh`). Without it the panel looks dead
  while the app is fine. Diagnose by injecting a tap first.
- **Read rects from the `v` dump, never off a screenshot.** And the bottom sheet **animates** — wait
  ~14 s after opening before reading its geometry.
- **A modal sheet swallows every later tap** and `touch_outside` does **not** dismiss it. Use `back`,
  then re-dump to confirm it is gone, or unrelated widgets will look broken.
- **Buttons at y≥1740 overlap the bottom nav** — tapping one can navigate instead.
- **Measure the widget you drive** (`Slider.getValue()` is in the `v` dump), not a proxy view.
- Re-check `ps -A | grep -ac ashu` between steps: a stale dump from a dead child looks exactly like a
  page that did not change.

## 5. Known-broken / next steps

- **Audio does not work.** `bindService`+`startService` are routed and `onStartCommand` runs; the
  blocker is `MediaSession.<init>` on a null `MediaSessionManager`. A `SystemServiceRegistry` fetcher
  for it is in `java/adapter/compat/WlMediaSession.java`, **deployed but never verified** — the board
  dropped off hdc first. Check `grep WESTLAKE-469` for `built MediaSessionManager`.
  Four gates after that remain unported: `libmedia_jni.so` stub, async `setCallback` +
  per-index `OH_AVBuffer`, pthread detach, unmute MUSIC (`uinput -K -d 16 -u 16`).
  ★Do not port them blind — they fix a pipeline that is not entered yet.
- **Presets list renders 0 items** though the 4 rows are in the DB and the query + its `"%%"` binding
  are provably correct; the SELECT is only intermittently reached.
- **`8h` sleep-timer button sits at x=1339**, off-screen on a 1200 px panel.
- **`java.util.regex`/charset/CursorWindow fixes are bridge-side.** If you rebuild the bridge and lose
  them, everything downstream (library, TLS, DB reads) regresses together.

## 5b. Networking — which path is live

The board has **real WiFi** (`wlan0`) and that is the path everything above was verified on. Two traps:
- **OHOS policy routing does not cover the app's uid**, so a route must be added to the MAIN table.
  There is no `ip`/`route` binary on the device — use `native-tools/wlroute.c`.
- The bridge still contains an **older proxy-tunnel path** gated on `WL_PROXY`
  (`hdc rport tcp:8888 tcp:8888` + a host proxy). `run406.sh` does **not** set `WL_PROXY`, so that code
  is dormant. Do not follow the comments in `AndroidRuntime.cpp` around the proxy setup — they describe
  the superseded offline-board fallback. Keep it only for a board with no WiFi.

Preflight before blaming the app: `native-tools/wldns.c` (resolve) and `wlnettest.c` (TCP connect).

## 6. Diagnostic technique notes

- **Content-filter your logging, never a call-count cap.** libart's own throw probe caps at 40 and was
  saturated by benign exceptions, hiding the real one for hours. Same mistake bit a bind-parameter log.
- **A missing native is better bound than patched.** Two unbound audio-policy natives were the real
  cause of a partial `<clinit>`; binding them beat patching the Java caller.
- **`handled=1` proves only that *some* view consumed the event**, not that the right window got it —
  hence the `chose root[N] of M` logging.
- Java's `System.err` goes to the log framework, **not** fd 2. To see app-side traces, call the native
  sink (`WlProbe.logThrowable`) — `tools/PatchLog.java` injects it after every `MOVE_EXCEPTION`.
