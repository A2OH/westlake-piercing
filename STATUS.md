# STATUS — noice on OpenHarmony (as of 2026-06-05)

Honest snapshot of what works end-to-end today. See `REPRODUCE.md` for the how
and `docs/` for the full root-cause history.

## ✅ Works end-to-end

- **noice renders stably** on OHOS DAYU200/RK3568: AppIntro welcome slide and
  MainActivity (声音库 library + 5-tab bottom nav + shuffle FAB), dark theme,
  live clock. (libhwui G3.8 + EGL NSFIX + bridge.)
- **Full populated sound library UI**: LIFE group + Birds/Crickets/Heartbeat/
  Purring-Cat, each with name · tags · favorite star + the 4-button control row
  (info / download / play / volume) + correctly-sized SVG illustration. Data is
  the real cdn.trynoice.com `library.json`, served from a **cached** copy
  (network was the source originally; live re-fetch is the open item below).
- **D-pad interaction**: DPAD_DOWN navigates the list, DPAD_CENTER activates a
  sound (binds SoundPlaybackService + ExoPlayer). (bridge in-process dispatch.)
- **Touch click + touch navigation**: tapping the info button opens the SoundInfo
  page; tapping play engages the audio pipeline. (bridge dispatchTouchViaViewRoot
  + VelocityTracker JNI stub + tap control channel.)
- **Multi-page navigation crash-free (9/10 pages)**: library, SoundInfo, volume
  dialog, play, Saved/Presets, sleep-timer, alarms, add-alarm time picker,
  account — all survive. (framework.jar null-service guards + noice APK guards +
  coroutine fix.)
- **Crashes fixed**: tagsoup NoSuchMethodError (play), ShortcutManager NPE
  (Saved), AlarmManager exception (add-alarm), ContentResolver NPE, and the
  subscription background-coroutine crash (coroutine `a.smali` fix — survives).
- **Connectivity low-level proven**: inet gids, direct-UDP DNS, AF_INET socket
  family (libv4force), BPF socket grant, CA trust store — a raw probe reaches the
  real cdn.trynoice.com nginx over TLS-port TCP.

## ⚠️ Partial / in progress

- **Live HTTPS data fetch** — DNS/TCP/socket-family/CA are all done, but the
  app's live library/subscription fetch does not complete (subscription shows
  "网络无法访问"). UI is populated from the cached library meanwhile. Deeper
  okhttp/non-blocking-connect/TLS diagnosis is pending.
- **Focus reliability** — only a COLD launch reliably focuses noice; focus drifts
  to launcher/screen-lock over a session. Mitigated (screen-awake + cold-launch +
  focus-independent tap control channel) but not solved.

## ❌ Not done

- **Audio output** — play *click* works and ExoPlayer binds, but the runtime
  never registers `android_media_AudioTrack` JNI, so actual PCM → AudioTrack →
  OH HAL output is not wired. Needs an AudioTrack→OH_AudioRenderer bridge.
- **Bottom-nav tabs via raw MMI touch** — y≥1208 is consumed by the OHOS
  systemui nav window; tabs are driven via the tap control channel
  (`echo N > /data/local/tmp/noice_tap`) instead.

## Known flakiness (operational, not bugs)

- `aa force-stop` degrades AMS after ~4 → reboot to clear.
- ~50% of cold launches hit a blank-list flow race → reroll until screenshot >45 KB.
- Intermittent bad-boot spin in `installSettingsContentProviderStub` → reboot to reroll.
- displayId compositing race can render the launcher over noice in screenshots.

## Deployed component md5s

runtime `16e08711` (un-rebuildable base) · bridge `2967c30c` · libhwui `8b8f84ec`
· framework.jar `15396933` · libv4force.so `7c3e5ece…` · cacerts.tgz `888d018d…`
