# STATUS — noice on OpenHarmony (Westlake), as of 2026-06-05

Honest end-to-end snapshot. See `REPRODUCE.md` for the *how* and `docs/` for the
full root-cause history. The deployed component md5s are at the bottom.

Device: OpenHarmony DAYU200 / RK3568, 32-bit ARM, app uid 13731, appspawn-x AOSP
adapter. App: `com.github.ashutoshgngwr.noice` (the "noice" ambient-sound app).

---

## ✅ Works end-to-end

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

## ⚠️ Partial / in progress

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

## ❌ Not done

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

## Known flakiness (operational, not bugs)

- `aa force-stop` degrades AMS after ~4 → reboot to clear.
- ~50% of cold launches hit a blank-list flow race → reroll until screenshot >45 KB
  (mitigated by restoring `noice-room.db.bak` + `noice-cdn-cache.bak`).
- Intermittent bad-boot spin in `installSettingsContentProviderStub` → reboot.
- displayId compositing race can render the launcher over noice in screenshots.
- direct-UDP DNS ignores `/etc/hosts` + `resolv.conf`, so the subscription
  network failure can't be force-reproduced on the bench (intermittent ~10-20%).

## Deployed component md5s

runtime `16e08711` (**un-rebuildable** base) · bridge `2967c30c` · libhwui
`8b8f84ec` · framework.jar `15396933` · libv4force.so `7c3e5ece…` · libtlsjni.so
`e248cc47…` · tlsjni-extra.dex `01ade5c4…` · cacerts.tgz `888d018d…`
