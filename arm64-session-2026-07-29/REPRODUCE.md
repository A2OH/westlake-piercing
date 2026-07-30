# Reproduce noice on the OHOS arm64 dev board

Written so another agent can pick this up cold. Board `5cdbf6af…`, aarch64, OHOS 6.1.0.31.
Read `ARTIFACT-MAP.md` first for what lives where, and `README.md` for what each fix does and why.

## 0. Prerequisites

- **hdc runs on WINDOWS** (`/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe`) and **cannot read WSL paths** —
  every push must go through a Windows staging dir. `recipes/env.sh` has a `push()` that does this.
- `. recipes/env.sh` before anything. **`ADAPTER_ROOT` must be exported** or the bridge build silently
  breaks (see the comment in that file).
- Android SDK at `/home/dspfac/android-sdk` (build-tools 34.0.0 for `d8`/`zipalign`, dexlib2 3.0.3
  under `cmdline-tools/latest/lib/external`).
- Sources: `/home/dspfac/bridge-build` (bridge, **not a git repo** — the `bridge/` dir here is a
  snapshot), `/home/dspfac/aosp-14-minikin` (minikin — note **14**, not 15).

## 1. Bring the board up

```bash
. recipes/env.sh
$HDC list targets                      # must show 5cdbf6af…
```
**If it lists nothing:** check Windows still enumerates the device
(`Get-PnpDevice | ? InstanceId -like 'USB*'`). If you see `APP Mode`, the board is powered and
connected but **hdcd/USB-debugging is not serving** → power-cycle and re-enable USB debugging.
WiFi debug is **not** a fallback unless `hdc tmode port 8710` was armed earlier over USB — arm it
now, while USB works, so a future drop is recoverable.

Then launch:
```bash
~/wl_restart.sh                        # kills stale children, launches, waits for a frame
# expect: UP: t=16s swaps=N alive=1 children=1
```
★A stale child silently steals taps — always use `wl_restart.sh`, never a bare `aa start`.
★Startup is flaky: a `TIMEOUT` on the first attempt is normal, retry 2-3×.

## 2. Deploy the pieces (in this order)

```bash
scripts/build/build_bridge_arm64.sh          # -> liboh_adapter_bridge.so   (FAIL baseline is 2)
~/wl_deploy.sh mytag                         # pushes it and restarts

scripts/build/build_minikin_stack_arm64.sh   # -> libminikin.so (font fix)
# push to $ASX/libminikin.so, chmod 755

recipes/build-ohaf-jar.sh                    # BCP helpers: TLS, service binds, audio focus, stubs
recipes/patch-noice-apk.sh                   # proxy dex surgery -> sound library loads
recipes/patch-framework-jar.sh               # 6 IActivityManager sites; RESTART AND CONFIRM BOOT
python3 recipes/signin.py                    # optional: real account (subscriptions 401 -> 200)
```
★The build script **exits 0 even when a file fails to compile** — always grep `^FAIL` and compare
against the baseline of **2** (`apk_bundle_parser`, `oh_skia_ahb_shim`). A count of 3 means you broke
something, and the linked .so will be missing your change.

## 3. Verify (each of these is a real, checkable signal)

```bash
L=$($HDC shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" | tr -d '\r')

$HDC shell "grep -a 'HANDSHAKE OK' $L"        # TLS 1.3 + verify=OK
$HDC shell "grep -a 'WESTLAKE-442. /' $L"     # regex: 13/13 expected
$HDC shell "grep -a 'WESTLAKE-453' $L"        # CursorWindow inflated via malloc
$HDC shell "grep -a 'WESTLAKE-468' $L"        # audio-policy natives bound (rc=0)
$HDC shell "grep -ac 'hb_face_reference_table' $L"   # font crashes: expect 0
~/wl_shot.sh                                  # ~128 KB = library rendered; ~58 KB = error page
```

## 4. Driving the UI

```bash
echo "324 385"          > /data/local/tmp/noice_tap   # tap (absolute coords)
echo "1140 1668 480 1668" > /data/local/tmp/noice_tap # drag
echo back               > /data/local/tmp/noice_tap   # dismiss a dialog  ← the ONLY reliable way
echo v                  > /data/local/tmp/noice_tap   # dump widget tree with absolute rects
echo w                  > /data/local/tmp/noice_tap   # list window roots
```
Tabs at y=1829: Library 119 · Presets 359 · Sleep Timer 599 · Alarms 839 · Account 1079.
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

## 6. Diagnostic technique notes

- **Content-filter your logging, never a call-count cap.** libart's own throw probe caps at 40 and was
  saturated by benign exceptions, hiding the real one for hours. Same mistake bit a bind-parameter log.
- **A missing native is better bound than patched.** Two unbound audio-policy natives were the real
  cause of a partial `<clinit>`; binding them beat patching the Java caller.
- **`handled=1` proves only that *some* view consumed the event**, not that the right window got it —
  hence the `chose root[N] of M` logging.
- Java's `System.err` goes to the log framework, **not** fd 2. To see app-side traces, call the native
  sink (`WlProbe.logThrowable`) — `tools/PatchLog.java` injects it after every `MOVE_EXCEPTION`.
