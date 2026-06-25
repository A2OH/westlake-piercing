# STATUS — Android apps on OpenHarmony (noice + Material Catalog)

Honest snapshot of what works end-to-end today, for **both** apps this repo
covers. See `REPRODUCE.md` (noice) and `CATALOG-REPRODUCE.md` (catalog) for the
how, `BUILD-FROM-SOURCE.md` to build either stock APK, and `docs/` for the full
root-cause history. Both apps are **stock** Android APKs running on the OHOS
`appspawn-x` AOSP-app adapter (no source changes; the only APK edits are the
cosmetic smali patches in `*-smali-patches/`).

Device: OpenHarmony DAYU200 / RK3568, 32-bit ARM. noice uid 13731; catalog uid
16371. Deployed component md5s for each app are at the bottom of its section.

---

## Material Components Catalog (`io.material.catalog`) — NEW

### ✅ Works end-to-end
- **Launches from the OHOS launcher icon** — correct Material Catalog logo +
  "Material Catalog" label (`entry.hap`), and from a **cold power-on** hands-off
  (Permissive SELinux + the `ondemand` appspawn-x auto-spawns on the icon-tap; no
  laptop, no manual bring-up).
- **All 32 grid categories navigable** L1→L3, every Material widget TYPE driven
  with a visible result: buttons (snackbar), switches/checkboxes/radios (+
  dependent enable), sliders (drag → value change + snackbar), tabs (switch),
  chips, badges (increment), FABs (show/hide), long-press multi-select,
  container-transform morph (animates frame-by-frame), progress (indeterminate +
  determinate), text fields (focus + IME keyboard). **0 functional failures / 0
  tombstones** across a full multi-hour sweep.
- **Date Picker** renders its calendar and is modal-interactive to **L5** (pick a
  date → header updates → OK dismisses) — used to hard-crash before the libart W9
  vtable fix. **Time Picker** clock loads; **dialogs / nav-drawer / side-sheets /
  bottom-sheet** composite and are drivable.
- **Soft keyboard appears + persists** on a plain Text Field (the Android-IMM →
  OHOS-`InputMethodController` bridge); **Search focus no longer crashes**
  (ContentResolver null-guard).
- **Crashes fixed**: the metaData NPE (`CatalogApplication.onCreate`), the
  ConnectivityManager NPE (`handleBindApplication`), the Date Picker W9 vtable
  hard-crash, the proxy-LinkMethods O(n²) hang, the Search-focus ContentResolver
  NPE, and the `createHardwareBitmap` SIGBUS (demo Activities open).
- **Cold-boot reliable**: the 6.3 MB fontconfig O(n²) parse that froze ~25 % of
  cold boots is fixed (minimal config) → **0 % (0/16)** bad-boot.

### ⚠️ Partial / known walls (NOT per-app bugs — adapter-level)
- **IME text-ENTRY via synthetic input** — the keyboard APPEARS and the field
  FOCUSES (cursor + placeholder), but typed characters don't commit through any
  injection path tried (`uinput -K`, `uinput -t`, on-screen key taps). OHOS
  synthetic input isn't bridged to the Android `InputConnection.commitText`. A
  **physical** keyboard is the untested real test (the IME window + focus path it
  would use is proven). Same family as the ignored BACK key.
- **WMS focus / compositing intermittency** — adapter app windows render on top
  but don't hold durable OHOS WMS focus (`EntryView`/SceneBoard keeps it). Some
  modals/popups composite intermittently per boot, and the SearchView keyboard is
  torn down ~65 ms after it shows. The plain Text Field keyboard persists; Date
  Picker / AlertDialog / nav-drawer / side-sheet do composite and are drivable —
  per-instance flaky, not a hard failure. **This is the single highest-leverage
  remaining item.**

### Cosmetic
- OHOS recents shows zombie catalog entries (names, no thumbnails) — multiton
  launch mode + stubbed mission persistence + the compositing wall. Clear All or
  reboot clears them; `launchMode:singleton` in `entry.hap` prevents the
  duplicates.

### Performance
- The catalog's own dex runs interpreted + JIT (no app-AOT; framework is AOT).
  Naive app-AOT is a dead end (stock dex2oat vs the custom vtable-rewriting libart
  → SIGBUS). The per-launch libart logging was trimmed 43,000 → ~800 lines
  (`kLogVtableFixup=false`). Warm relaunch ≈ 3.7 s to first draw; cold first-boot
  launch ~25–30 s (prefork + W-series vtable fixups, which AOT wouldn't remove).

### Catalog deployed component md5s (verified on device 2026-06-25)
libart `ba40f173` · framework.jar `e6f9e1a3` · adapter-runtime-bcp `c026e80c` ·
oh-adapter-framework `4690cae1` · bridge `9b2a9727` · liboh_ime_helper `e4880759` ·
libappexecfwk_common `4d2c6399` · appspawn-x `3abe3bde` · boot-framework.oat
`290e4499` · hm_symbol_config_next.json `425290bd` · catalog base.apk `a9df5518`.

---

## noice (`com.github.ashutoshgngwr.noice`)

### ✅ Works end-to-end
- **noice renders stably**: AppIntro welcome slide + MainActivity (声音库 library
  + 5-tab bottom nav + shuffle FAB), dark theme, live clock. (libhwui G3.8 +
  ASurfaceControl no-op + new-surface EGL fix + bridge.)
- **Full populated sound library UI**: LIFE group + Birds/Crickets/Heartbeat/
  Purring-Cat, each with name · tags · favorite star + the 4-button control row
  (info / download / play / volume) + correctly-sized SVG illustration. Data is
  the real `cdn.trynoice.com` `library.json`, served from a **cached** copy
  (the live re-fetch is the open connectivity item below).
- **D-pad interaction**: DPAD_DOWN navigates the list; DPAD_CENTER activates a
  sound (binds SoundPlaybackService + ExoPlayer). (bridge in-process dispatch.)
- **Touch click + touch navigation**: tapping the info button opens the SoundInfo
  page; tapping play engages the audio pipeline; the volume bottom-sheet renders
  over the live library. (bridge dispatchTouchViaViewRoot + VelocityTracker JNI
  stub + the `/data/local/tmp/noice_tap` control channel.)
- **Multi-page navigation, 9/10 pages crash-free** (2 sweep rounds): library,
  SoundInfo, volume dialog, play, Saved/Presets, sleep-timer, alarms, add-alarm
  TimePicker, account — all survive. Only the live-API subscription page is
  network-flaky (below).
- **Universal framework crash-fixes (PROVEN universal)**: the ShortcutManager
  (Saved tab) and AlarmManager (add-alarm) crashes are fixed *framework-wide* in
  `framework.jar` (SystemServiceRegistry fetchers return a non-null manager +
  manager-method null-guards). An **unguarded** noice APK survives both crashes
  with only the framework fix. Plus tagsoup (play), ContentResolver NPE
  (subscription path), and the coroutine crash (noice APK `a.smali`).
- **Connectivity, lower layers PROVEN**: inet gids, direct-UDP DNS (all 3 JNI
  resolver variants), AF_INET socket family (libv4force IPv4-force), the netsys
  cgroup-eBPF socket grant, CA trust store, and **native TLS createSocket** all
  work — a raw probe + the in-app native TlsJniSocket reach the real
  `cdn.trynoice.com` nginx; DNS resolves `api.trynoice.com -> 35.94.160.101`.

### ⚠️ Partial / in progress
- **Live HTTPS data fetch** — the chain gets all the way to native TLS
  `createSocket` (TlsJniSocket instantiates, libtlsjni loads), but `startHandshake`
  is **never reached**: okhttp (both the BCP `com.android.okhttp` and noice's
  bundled `okhttp3`) casts/`instanceof`-checks
  `com.android.org.conscrypt.OpenSSLSocketImpl`, which is **erroneous in the boot
  image** (conscrypt is incomplete on this adapter — the same reason a stub
  `TlsShimProvider` exists) → `NoClassDefFoundError`. **This is the current
  connectivity wall.** UI is populated from a cached library meanwhile.
- **cgroup-bpf socket grant flakiness** — `bpfgrant <uid> oh_sock_permission_map`
  reliably grants on a *warm* appspawn-x, but on a *cold reboot* noice's
  appspawn-cgroup socket sometimes still gets `EPERM` at DNS (6/6 cold cycles
  EPERM'd in one test). Operational, not a code bug; re-grant + warm restart.
- **Focus reliability** — only a COLD launch reliably focuses noice; focus drifts
  to launcher/screen-lock over a session. Mitigated (screen-awake + cold-launch +
  focus-independent tap control channel) but not solved (WMS/displayId
  arbitration, deeper than the bridge).

### ❌ Not done
- **Audio output** — play *click* works and ExoPlayer/SoundPlaybackService binds,
  but the SoundPlaybackService ability never runs its player, and the runtime
  exposes no AudioTrack path; actual PCM → AudioTrack → OH HAL output is not
  wired. `android.media.AudioTrack` isn't even in the device framework.jar.
  OH_AudioRenderer NDK exists, so an AudioTrack→OH bridge is buildable *in
  principle*, but it is a separate multi-component project.
- **Bottom-nav tabs via raw MMI touch** — y≥1208 is consumed by the OHOS systemui
  nav window; tabs are driven via the tap control channel (`echo N >
  /data/local/tmp/noice_tap`) instead.
- **Universal UEH in framework.jar** — implemented but **ineffective**: the
  un-rebuildable runtime intercepts uncaught coroutine exceptions before any Java
  handler runs. The coroutine crash is mitigated in the noice APK instead.

### noice deployed component md5s
runtime `16e08711` (**un-rebuildable** base) · bridge `2967c30c` · libhwui
`8b8f84ec` · framework.jar `15396933` · libv4force.so `7c3e5ece…` · libtlsjni.so
`e248cc47…` · tlsjni-extra.dex `01ade5c4…` · cacerts.tgz `888d018d…`

---

## Shared known flakiness (operational, not bugs)
- `aa force-stop` degrades AMS after ~4 → reboot to clear.
- ~50 % of cold launches hit a flow/compositing race → reroll
  (noice: screenshot >45 KB = populated, mitigated by restoring `noice-room.db.bak`
  + `noice-cdn-cache.bak`; catalog: fresh composite ≈ 40–130 KB).
- Intermittent bad-boot spin in `installSettingsContentProviderStub` → reboot.
- displayId compositing race can render the launcher over the app in screenshots.
- direct-UDP DNS ignores `/etc/hosts` + `resolv.conf`, so the noice subscription
  network failure can't be force-reproduced on the bench (intermittent ~10-20%).
- The board is DC-powered with a MOCK battery — "low battery 11%" is fake;
  **reboot is always safe** (see `notes/device-safety.md`).

## The two foundational walls (shared, adapter-level, NOT per-app)
1. **Synthetic input → `InputConnection` not bridged** (catalog IME text-commit +
   BACK key; noice's VelocityTracker was the same family, worked around in the
   bridge).
2. **Adapter app windows don't hold durable OHOS WMS focus** (compositing
   intermittency; catalog SearchView keyboard teardown; noice
   focus-on-cold-launch-only). Fixing #2 is the highest-leverage remaining item
   for both apps.
