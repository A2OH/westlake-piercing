# Validate the unchanged Toutiao APK on the D600 board

This runbook validates Toutiao 13.9.0 (`com.ss.android.article.news`) on the
aarch64 D600 OpenHarmony board. It is an acceptance procedure, not a deployment
recipe. It assumes the Westlake runtime, APK, WebView package, native libraries,
and launcher are already staged under `/data/local/tmp/asx`.

The known board used for this bring-up is:

- serial: `5cdbf6af00000000000000000923012c`
- architecture: `aarch64`
- OS: `OpenHarmony 6.1.0.31`
- display used by the coordinates below: `1200x1920`

The primary harness is `scripts/toutiao/ttwalk.sh` in the `A2OH/westlake`
checkout. The detailed root-cause and rollback history is in
`/home/dspfac/bridge-build-arm64/TOUTIAO-HANDOFF.md` on the bring-up host.

## What counts as a pass

Do not reduce “Toutiao works” to one screenshot. A release-validation pass has
independent gates:

| Gate | Required evidence |
| --- | --- |
| Unchanged APK | Device APK MD5 is `cc99caaaa3baffbb100118753fa1d130` |
| Launch | Harness reaches `side-channels started`; recorded child PID remains alive |
| Android UI | View tree contains the search bar, category strip, and feed recycler |
| Live feed | Recommended tab contains multiple real headlines from the current run |
| Pixels | Current-run capture is `LIT`; Toutiao RenderService nodes are `SpecialLayer=0` |
| Input/scroll | Feed visibly moves down and back up; process stays alive |
| Search | Exact query `AI` produces real result content, not only an empty shell |
| Article HTML | Current-run article shows a title and body; WebView/KTX reports data ready |
| Article images | At least one remote article image renders; no image-failure placeholder for the checked image |
| Video | Player reports rendering/playing and two current-run captures show different decoded frames |
| JIT | Child log contains the live-JIT marker and no fatal/SEGV/StackOverflow/XZ failure |
| Controls | Unchanged Noice and Material Catalog pass on the same runtime bytes |

Record article HTML and article images separately. A readable article containing
`图片加载失败` is an HTML pass and an image failure, not a full WebView pass.

## Rules that prevent false results

1. Use the PID written to `/data/local/tmp/asx/walkpid`; do not use the newest
   log filename as a substitute for process identity.
2. Gate launch on `side-channels started`, not a fixed sleep. A forced-JIT cold
   launch on this board can take several minutes.
3. Save a view tree and framebuffer capture at every major state. Neither oracle
   is sufficient alone.
4. Select the Recommended feed before declaring the feed empty. The Following
   tab can legitimately be empty for a logged-out account.
5. Verify hashes on the device. A successful host build or file-send message
   does not prove the device contains those bytes.
6. Never use `kill -3`; this port has no normal Android SignalCatcher and SIGQUIT
   can create a false crash or materially perturb article timing.
7. Do not reuse an old screenshot, cached proof reel, or a frame from another
   PID. Every artifact must carry the current run name and PID.
8. A submitted tap is not a successful navigation. Require the destination view
   tree and pixels to change.
9. Run three launch/feed trials before making a stability claim. Run Noice and
   Material Catalog after every ART, framework, bridge, surface, or launcher
   change.
10. `ttwalk.sh launch` kills the existing `appspawn-x` and its child. Run this
    only in a dedicated validation window.

## 1. Host setup

Use WSL/bash on the bring-up host:

```bash
export D600_HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
export D600_SERIAL=5cdbf6af00000000000000000923012c
export WESTLAKE_CHECKOUT=/home/dspfac/openharmony/.codex-worktrees/westlake-fixes
export TT_HARNESS=$WESTLAKE_CHECKOUT/scripts/toutiao/ttwalk.sh
export TT_SHOTLIT=$WESTLAKE_CHECKOUT/scripts/toutiao/shotlit.py
export TT_EVIDENCE_ROOT=/home/dspfac/openharmony/toutiao-d600-validation
export TT_RUN_NAME=jit-$(date +%Y%m%d-%H%M%S)
export TT_RUN_DIR=$TT_EVIDENCE_ROOT/$TT_RUN_NAME
mkdir -p "$TT_RUN_DIR"
```

The current harness has the D600 `hdc.exe` path embedded. If the checkout is on
another host, update its `H=` line or place `hdc.exe` at the path above before
running it.

Confirm that exactly the intended board is connected:

```bash
$D600_HDC list targets | tr -d '\r' | tee "$TT_RUN_DIR/targets.txt"
$D600_HDC shell 'uname -m; param get const.product.software.version; getenforce' \
  | tr -d '\r' | tee "$TT_RUN_DIR/board.txt"
```

Expected: the serial above, `aarch64`, `OpenHarmony 6.1.0.31`, and
`Permissive`. Stop if a different target is selected.

## 2. Fingerprint the deployed stack

Capture device hashes before launching:

```bash
$D600_HDC shell 'md5sum \
  /data/local/tmp/asx/toutiao.apk \
  /data/local/tmp/asx/libart.so \
  /data/local/tmp/asx/liboh_adapter_bridge.so \
  /data/local/tmp/asx/fw/oh-adapter-framework.jar \
  /data/local/tmp/asx/fw/adapter-mainline-stubs.jar \
  /data/local/tmp/asx/focused-text-helper.jar \
  /data/local/tmp/asx/libandroid_native_network_compat.so \
  /data/local/tmp/asx/run_tt.sh 2>/dev/null' \
  | tr -d '\r' | tee "$TT_RUN_DIR/device-md5.txt"
```

The APK hash is invariant and mandatory:

```text
cc99caaaa3baffbb100118753fa1d130  /data/local/tmp/asx/toutiao.apk
```

The runtime hashes can change between candidate builds. Compare them with the
release manifest or PR under test. Never silently bless an unknown combination.
For reference only, the D600 contained these bytes when this document was
written on 2026-08-27:

| Artifact | Observed MD5 |
| --- | --- |
| `libart.so` | `29f458a8d649beb72927108c26e202ba` |
| `liboh_adapter_bridge.so` | `486704782e895d0ed89596b69c8ec622` |
| `fw/oh-adapter-framework.jar` | `180010c554b036e92c56b11b33ee301b` |
| `focused-text-helper.jar` | `89866d46f8b16f8326a9420718dbd968` |
| `libandroid_native_network_compat.so` | `c87945acdaa78e3cb51a3c4a7250ee28` |
| `run_tt.sh` | `af01c232b9262a6e5c1d26e6ec7fd262` |

Those observed hashes identify a snapshot; they are not a substitute for the
candidate's own manifest and acceptance report.

## 3. Reusable evidence helpers

The enhanced harness supports `status`, `vt`, `tap`, `swipe`, `back`, `text`, and
`shot`. These raw helpers also work when validating from an older harness:

```bash
tt_pid() {
  $D600_HDC shell 'cat /data/local/tmp/asx/walkpid 2>/dev/null' | tr -d '\r'
}

tt_tap_channel() {
  printf '/data/local/tmp/noice_tap.%s' "$(tt_pid)"
}

tt_swipe() {
  $D600_HDC shell "echo '$1 $2 $3 $4' > $(tt_tap_channel)" >/dev/null
}

tt_back() {
  $D600_HDC shell "echo back > $(tt_tap_channel)" >/dev/null
}

tt_text() {
  $D600_HDC shell "printf '%s' '$1' > /data/local/tmp/noice_text" >/dev/null
}

tt_capture() {
  local capture_name=$1
  $D600_HDC shell "power-shell wakeup >/dev/null 2>&1; \
    snapshot_display -f /data/local/tmp/$capture_name.jpeg >/dev/null 2>&1"
  (cd "$TT_RUN_DIR" && \
    $D600_HDC file recv "/data/local/tmp/$capture_name.jpeg" \
      "./$capture_name.jpeg" >/dev/null)
  python3 "$TT_SHOTLIT" "$TT_RUN_DIR/$capture_name.jpeg"
}
```

Do not put `tr` or `awk` inside a D600 shell command; use host tools after the
`hdc` output returns.

## 4. Launch with forced JIT

Wake and unlock the panel first:

```bash
$D600_HDC shell 'power-shell timeout -o 3600000; power-shell wakeup; \
  uinput -T -m 600 1500 600 400 300' >/dev/null
```

Launch the unchanged APK:

```bash
WL_EVIDENCE_ROOT="$TT_EVIDENCE_ROOT" \
WL_RUN_ID="$TT_RUN_NAME" \
APPSPAWNX_FORCE_JIT=1 \
WL_TOUCH_ENQUEUE=1 \
"$TT_HARNESS" launch | tee "$TT_RUN_DIR/launch.txt"
```

Do not interrupt merely because the cold launch is slow. The harness waits for
the bridge side-channel and writes the exact child PID only after readiness.

Record initial state:

```bash
"$TT_HARNESS" status | tee "$TT_RUN_DIR/status-launch.txt"
"$TT_HARNESS" vt > "$TT_RUN_DIR/launch.vt"
tt_capture launch
```

Minimum launch gate:

- `walkpid` is non-empty and `/proc/<walkpid>` exists;
- `side-channels started` is present in that PID's child log;
- at least one Toutiao RenderService node is visible;
- the view tree contains real Android widgets rather than only the OHOS shell.

If the first-run privacy dialog is visible, dismiss it and capture the result:

```bash
"$TT_HARNESS" tap 600 1207
sleep 5
"$TT_HARNESS" vt > "$TT_RUN_DIR/after-consent.vt"
tt_capture after-consent
```

## 5. Validate the live Recommended feed

Select Recommended. On the 1200x1920 layout the established fallback coordinate
is approximately `(74,212)`:

```bash
"$TT_HARNESS" tap 74 212
```

Poll the structural oracle rather than assuming a fixed network delay:

```bash
for feed_try in $(seq 1 18); do
  "$TT_HARNESS" vt > "$TT_RUN_DIR/feed-$feed_try.vt"
  if rg -q 'FeedItemRootLinerLayout|FeedTitleTextView' \
      "$TT_RUN_DIR/feed-$feed_try.vt"; then
    break
  fi
  sleep 10
done
```

Capture and inspect the settled feed:

```bash
"$TT_HARNESS" vt > "$TT_RUN_DIR/feed-final.vt"
tt_capture feed-final
rg -n 'FeedItemRootLinerLayout|FeedTitleTextView|FeedCommonRecyclerView' \
  "$TT_RUN_DIR/feed-final.vt" | tee "$TT_RUN_DIR/feed-lines.txt"
```

Feed pass criteria:

- multiple current news titles are readable in the view tree and capture;
- cards are children of `FeedCommonRecyclerView`;
- the process is still alive;
- the capture is `LIT`;
- at least one media thumbnail should be recorded separately as the feed-image
  gate. Gray placeholders do not fail headline networking, but they do fail the
  feed-image gate.

## 6. Validate scrolling and input

Capture before, after a downward content scroll, and after scrolling back:

```bash
tt_capture feed-scroll-before
tt_swipe 600 1650 600 450
sleep 5
"$TT_HARNESS" vt > "$TT_RUN_DIR/feed-scroll-down.vt"
tt_capture feed-scroll-down

tt_swipe 600 450 600 1650
sleep 5
"$TT_HARNESS" vt > "$TT_RUN_DIR/feed-scroll-up.vt"
tt_capture feed-scroll-up

md5sum "$TT_RUN_DIR"/feed-scroll-*.jpeg \
  | tee "$TT_RUN_DIR/feed-scroll-md5.txt"
"$TT_HARNESS" status | tee "$TT_RUN_DIR/status-after-feed-scroll.txt"
```

Different image hashes alone are not a pass. Visually confirm that different
headlines/cards occupy the viewport after the first swipe and that the list
moves back after the reverse swipe.

## 7. Validate search

Open the search UI using its visible search field or icon. On the established
layout the field is near `(500,105)`:

```bash
"$TT_HARNESS" tap 500 105
sleep 5
"$TT_HARNESS" vt > "$TT_RUN_DIR/search-open.vt"
tt_capture search-open
```

If prior text is present, tap the clear `x` near `(1040,106)`. Then commit the
exact query once:

```bash
"$TT_HARNESS" tap 1040 106
sleep 2
"$TT_HARNESS" tap 500 105
tt_text AI
sleep 10
"$TT_HARNESS" vt > "$TT_RUN_DIR/search-ai-entered.vt"
rg -n 'SearchAutoCompleteTextView' "$TT_RUN_DIR/search-ai-entered.vt"
```

Require the focused/top search field to contain exactly `AI`, not `AIAI`, before
submitting:

```bash
"$TT_HARNESS" tap 1136 106
sleep 30
"$TT_HARNESS" vt > "$TT_RUN_DIR/search-ai-result.vt"
tt_capture search-ai-result
```

If the first request shows `网络异常，请稍后重试`, press Retry once near
`(600,945)`, wait, and record that the run needed fallback:

```bash
"$TT_HARNESS" tap 600 945
sleep 30
"$TT_HARNESS" vt > "$TT_RUN_DIR/search-ai-retry.vt"
tt_capture search-ai-retry
```

Search passes only when the current-run capture shows actual AI summary/results,
tabs, titles, or media. A shell with tabs plus a network-error panel is a failure.
A successful single retry is a degraded pass and must be reported as such.

## 8. Validate an article and WebView content

Scroll until a normal result card or `全文` link is visible. Save the result title
and tap coordinate in `notes.txt`; do not claim success from the tap-delivered log.

After tapping, poll every 20 seconds for up to seven minutes. Cold/expired-cache
article runs on this stack have previously varied from roughly 125 to 375 seconds.
Do not use SIGQUIT thread dumps during this wait.

```bash
for article_try in $(seq 1 21); do
  "$TT_HARNESS" vt > "$TT_RUN_DIR/article-$article_try.vt"
  if rg -q 'MyWebViewV9.*dataLen=[1-9][0-9]*.*phase=4.*started=1.*ready=1.*dataReady=1' \
      "$TT_RUN_DIR/article-$article_try.vt"; then
    break
  fi
  sleep 20
done

"$TT_HARNESS" vt > "$TT_RUN_DIR/article-final.vt"
tt_capture article-final
```

Article HTML pass criteria:

- the capture contains a readable title and at least two body paragraphs;
- the current view tree reports non-zero `dataLen`, `phase=4`, `started=1`,
  `ready=1`, and `dataReady=1` for `MyWebViewV9` or its current equivalent;
- the screen is no longer `正努力加载中，请耐心等待`;
- the process remains alive.

Article-image pass criteria:

- at least one remote inline image has real pixels in the current article;
- the checked image is not a gray placeholder;
- `图片加载失败` or `图片加载失败，点击重试` is absent for that image.

Validate WebView scrolling in both directions:

```bash
tt_swipe 600 1650 600 350
sleep 5
"$TT_HARNESS" vt > "$TT_RUN_DIR/article-down.vt"
tt_capture article-down

tt_swipe 600 350 600 1650
sleep 5
"$TT_HARNESS" vt > "$TT_RUN_DIR/article-up.vt"
tt_capture article-up
```

Require visible body movement and a live process after both gestures.

## 9. Validate video playback

Return to the main UI, then select the bottom Video tab. The established bottom
tab coordinate is approximately `(360,1880)`:

```bash
tt_back
sleep 5
"$TT_HARNESS" tap 360 1880
sleep 20
"$TT_HARNESS" vt > "$TT_RUN_DIR/video-tab.vt"
tt_capture video-tab
```

Tap a visible video poster/play control chosen from the current capture. After
playback begins, collect structural state and two frames at least ten seconds
apart:

```bash
"$TT_HARNESS" vt > "$TT_RUN_DIR/video-playing.vt"
tt_capture video-frame-a
sleep 10
tt_capture video-frame-b
md5sum "$TT_RUN_DIR"/video-frame-*.jpeg \
  | tee "$TT_RUN_DIR/video-frame-md5.txt"
rg -n 'TTReusePlayer|renderStarted|playing|paused|preparing' \
  "$TT_RUN_DIR/video-playing.vt"
```

Video passes when:

- the player oracle reports `renderStarted=1`, `playing=1`, `paused=0`, and
  `preparing=0`, or equivalent current fields;
- both captures show decoded video rather than a static poster/black rectangle;
- visual inspection confirms different content frames;
- the process remains alive.

Changing hashes caused only by clock text or controls do not prove video motion.

## 10. Verify JIT, liveness, RenderService, and fatal markers

Copy the exact child log named by `walkpid`:

```bash
export TT_PID=$(tt_pid)
export TT_CHILD_LOG_DEVICE=/data/service/el1/public/appspawnx/adapter_child_${TT_PID}.stderr
(cd "$TT_RUN_DIR" && \
  $D600_HDC file recv "$TT_CHILD_LOG_DEVICE" ./adapter-child.stderr >/dev/null)

$D600_HDC shell 'hidumper -s RenderService -a RSTree' \
  | tr -d '\r' > "$TT_RUN_DIR/rs-tree.txt"

"$TT_HARNESS" status | tee "$TT_RUN_DIR/status-final.txt"
rg -n 'JIT IS LIVE|\[JIT-528\]' "$TT_RUN_DIR/adapter-child.stderr" \
  | tee "$TT_RUN_DIR/jit-markers.txt"
```

The JIT run fails if any unapproved fatal marker is present:

```bash
if rg -n 'FATAL EXCEPTION|Check failed|WESTLAKE-CHILDSEGV|SIGSEGV|StackOverflowError|xz_utils.*Check failed' \
    "$TT_RUN_DIR/adapter-child.stderr"; then
  echo 'FAIL: fatal/runtime marker found'
else
  echo 'PASS: no fatal/runtime marker found'
fi
```

Inspect `rs-tree.txt` for the visible Toutiao surfaces. They must be
`SpecialLayer=0`. `PROTECTED`, `SpecialLayer=4`, or `SpecialLayer=4100` is a
surface-boundary regression. If a protected layer ever reappears, the view tree
remains the structural oracle and a black capture must not be used to claim that
the app itself rendered black.

## 11. Interpreter rollback control

After the JIT evidence is archived, repeat launch/feed with JIT disabled:

```bash
export TT_INTERP_RUN=interp-$(date +%Y%m%d-%H%M%S)
mkdir -p "$TT_EVIDENCE_ROOT/$TT_INTERP_RUN"

WL_EVIDENCE_ROOT="$TT_EVIDENCE_ROOT" \
WL_RUN_ID="$TT_INTERP_RUN" \
APPSPAWNX_FORCE_JIT=0 \
WL_TOUCH_ENQUEUE=1 \
"$TT_HARNESS" launch \
  | tee "$TT_EVIDENCE_ROOT/$TT_INTERP_RUN/launch.txt"
```

Dismiss consent if needed, select Recommended, and repeat the launch/feed gates.
The feed should work without a live-JIT marker. This proves the compatibility
path has an immediate runtime rollback; it does not replace the forced-JIT pass.

## 12. Mandatory independent controls

Run the unchanged Noice and Material Catalog APKs against the same ART, bridge,
framework jars, launcher contract, and surface stack.

Noice pass criteria:

- populated sound list (historically about 95 view-tree widgets on this stack);
- capture is `LIT` and surface is `SpecialLayer=0`;
- live JIT marker is present in forced-JIT mode;
- zero fatal, SEGV, StackOverflow, and XZ markers;
- process remains alive after input/scroll.

Material Catalog pass criteria:

- launch with `ASX_KEEP_THEME=1` and direct APK/activity settings;
- navigate to a known Slider demo;
- populated Slider UI (historically about 52 widgets on the accepted stack);
- capture is `LIT`, surface is `SpecialLayer=0`, and live JIT is present;
- zero fatal/SEGV markers.

Use the existing companion procedures:

- Noice: `REPRODUCE.md` and
  `arm64-session-2026-07-29/scripts/host/wl_ui.sh`
- Material Catalog: `CATALOG-REPRODUCE.md` and
  `arm64-session-2026-08-12-catalog611/recipes/walkcat5.sh`

If either control fails on the same bytes, do not label a Toutiao symptom
app-specific until the shared-stack regression is excluded.

## 13. Stability matrix

Use this minimum matrix for a release claim:

| Trial | Mode | Required scope |
| --- | --- | --- |
| JIT-1 | forced JIT | launch, feed, scroll, search, article HTML/image, video |
| JIT-2 | forced JIT | launch, feed, scroll, liveness |
| JIT-3 | forced JIT | launch, feed, scroll, liveness |
| INT-1 | interpreter | launch, feed, scroll, liveness |
| Noice | forced JIT | independent control |
| Material Catalog | forced JIT | independent control |

All three forced-JIT launch/feed trials must pass. Report search, article-image,
or video failures even if the other two smoke trials pass.

## 14. Result template

Write `RESULT.md` inside the evidence root:

```markdown
# Toutiao D600 validation result

- date/time:
- board serial and OS:
- Westlake/ART/bridge commits:
- device MD5 manifest: device-md5.txt
- unchanged APK MD5:
- JIT trials passed:
- interpreter trial passed:
- Noice control:
- Material Catalog control:

| Gate | PASS/FAIL/DEGRADED | Evidence file | Notes |
| --- | --- | --- | --- |
| Launch/liveness | | | |
| Recommended feed text | | | |
| Feed images | | | |
| Feed scroll down/up | | | |
| Search exact AI query | | | |
| Article HTML | | | |
| Article remote images | | | |
| Article scroll down/up | | | |
| Video decoded frames | | | |
| JIT marker/no fatals | | | |
| SpecialLayer=0 | | | |

## Remaining defects

-
```

The final claim should be no broader than the gates that passed. For example:

- “Launch/feed/JIT passed; article image failed” is valid.
- “Toutiao fully works” is invalid if search required an unreported retry,
  article content came from another PID, an inline image failed, or video motion
  was not visually confirmed.

