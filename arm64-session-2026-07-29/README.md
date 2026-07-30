# arm64 session — 2026-07-29

Work from one long bring-up session on board `5cdbf6af…` (aarch64, OHOS 6.1.0.31).

> **Picking this up cold?** Read [REPRODUCE.md](REPRODUCE.md) — ordered build/deploy/verify steps, how
> to drive the UI, and what is still broken. [ARTIFACT-MAP.md](ARTIFACT-MAP.md) says which built file
> goes where on the device and which rollback backups already exist there.
> This file explains **what** each fix does and **why** — the reasoning, not the procedure.

### Which bridge tree to build
`bridge-full-src/` is the **complete** native source the working `liboh_adapter_bridge.so` is built
from — all 96 `.cpp` plus headers, copied verbatim from the live tree. `build_bridge_arm64.sh` compiles
it by default (`BRIDGE_SRC=` overrides), and it is **verified to build**: 94/96 compile and the .so
links, the two expected failures being unused files.

⚠️It has **diverged from this repo's older `bridge-src/`** (e.g. `oh_input_bridge.cpp` is 148,518 B
here vs 74,901 B there). Nothing tracked was overwritten; `bridge-src/` is left as-is. Reconcile
deliberately — do not assume they are interchangeable.

`bridge/` keeps just the **seven files changed in this session**, as a readable diff surface. It is a
subset of `bridge-full-src/`, not a separate version.

## What got fixed (all verified on device unless marked)

### Sound library now loads and renders (31 sounds)
* **Proxy dispatch** — app-dex surgery rewrote 3 `invoke-interface`-on-Proxy sites to `invoke-static`
  into a merged `westlake/WlProxy` helper. Confirms the §436 libart validator defect end-to-end.
* **TLS** — the JSSE "provider" was a passthrough stub returning the *plain* socket, so the app spoke
  cleartext to :443. Replaced with a real `SSLSocket` over OHOS's own `libssl_openssl.z.so`,
  verifying against `/etc/ssl/certs/cacert.pem`:
  `HANDSHAKE OK proto=TLSv1.3 cipher=TLS_AES_256_GCM_SHA384 verify=OK`.
  Also added the missing `android.net.ssl.SSLSockets` (OkHttp's Android10Platform needs it) and a
  KeyStore + TrustManagerFactory, since this BCP had neither.
* **java.util.regex was a LITERAL MATCHER** — libart ships a bring-up stub whose `compileImpl` just
  stores the pattern string. Real ICU `uregex_*` natives are now registered over it (13/15 methods,
  symbols carry the `_66` suffix). 13/13 test cases pass.
* **Charset was a Latin-1 stub** writing ABSOLUTE offsets where AOSP writes DELTAS → 565KB of UTF-8
  JSON blew up as `CoderMalfunctionError: newPosition > limit`. Real `ucnv_*` natives registered.
* **CursorWindow could never inflate** (no ashmem on OHOS), so it was stuck at its 16KB inline buffer
  and 114KB `sound_metadata` rows failed with `SQLiteBlobTooBigException`. Added a heap fallback that
  covers **every** ashmem failure path, not just a failed `create_region`.

### Font crash fixed properly, no LD_PRELOAD
`Font::prepareFont` handed HarfBuzz a raw pointer into a mapping owned elsewhere; the owner freed it
and the next text measure faulted in `hb_face_reference_table`. HarfBuzz now gets its **own copy** of
the font bytes. 35+ tab taps, 0 crashes (was ~10-15). A `shared_ptr<MinikinFont>` keep-alive was
tried first and **still crashed** — the unmap is not tied to MinikinFont's lifetime.
See `minikin/Font.cpp.hb-owns-font-bytes.diff` (upstream remote is Google's, so it lives here).

### Input
* **Dialog touch dispatch** — the injector picked the **first** focused-or-shown root, but
  `mRoots` APPENDS, so that is the OLDEST window (or a stale leftover). A stale window still reports
  `isShown()` and returns `handled=1`, which is indistinguishable from success. Now keeps the **LAST**
  matching root in both tap and drag paths, plus `chose root[N] of M` logging — start any future
  input debugging there.
* Physical touch needs `touchfwd` running (added to `run406.sh`); the Material `Slider` works
  (`VALUE=1.000 → 0.340`) — earlier "it ignores input" was a bad measurement of the row label.

### Audio — NOT working yet, but much further
`bindService` **and** `startService` are now routed to an in-process service (`WlAmsBind`), so
`SoundPlaybackService` is created, connected and receives `onStartCommand`. `startService` was the
real missing gate — a media service is told to play through it, not through bind.
Framework.jar dex surgery rewrote 6 `invoke-interface/range` sites on `IActivityManager` to
`invoke-static/range` into `android/app/WlAmsBridge` (boots fine; both `NoSuchMethodError`s gone).
Binding the two unbound audio-policy list natives fixed the real cause of
`SoundPlayerManager`'s null `AudioAttributesCompat` statics — they now self-initialise.
**Remaining:** `MediaSession.<init>` on a null `MediaSessionManager`. A `SystemServiceRegistry`
fetcher for it is written and deployed but **UNVERIFIED** — the board dropped off hdc before the test.

## Layout
```
REPRODUCE.md          start here: prerequisites -> build -> deploy -> verify -> drive UI -> known-broken
ARTIFACT-MAP.md       artifact -> device path, rollback backups, where the non-rebuildable libart lives
bridge-full-src/      COMPLETE native bridge source (96 .cpp, 391 files) -- what actually gets built
bridge-build-inputs/  overlay/ + stubinc/ + bridge_incs_all.txt + musl_compat.* (the rest of the build)
bridge/               just the 7 files changed this session (a readable subset of bridge-full-src/)
java/                 new BCP helpers (adapter.compat.*, android.net.ssl.SSLSockets) + android.app.WlAmsBridge
tools/                dexlib2 utilities: call-site finders, dex rewriters, disassemblers, catch injector
recipes/              env.sh + the four build/patch/sign-in recipes
scripts/host|build|device/   13 host drivers, 6 aarch64 build scripts, run406.sh
native-tools/         touchfwd.c + the DNS/TCP/IPv6 probes
minikin/              the HarfBuzz font-lifetime fix as a diff
```
Paths are parameterised by **`$WLROOT`** (default `$HOME`) so nothing here hardcodes one machine's
home directory; `recipes/env.sh` sets it and auto-detects the Windows-side tools dir.

## Board note
`hdc` lists nothing while Windows still enumerates the device as `APP Mode` ⇒ hdcd/USB-debugging is
not serving. WiFi debug is **not** a fallback unless `hdc tmode port 8710` was armed earlier over USB.
Do that next time USB is healthy.
