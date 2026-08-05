# Reference traces — stock Android, for differential debugging

`oneplus6t-android11-play-birds.log` is what noice 2.5.1 does when it plays the **Birds** sound on a
stock, rooted **OnePlus 6T (Android 11, SDK 30, arm64-v8a)** — the same ABI the board runs, and the
same APK (`noice.apk.pre440`, md5 `ec174f82b20db9fdcce79f4f56b2a54e`). Captured with
`adb logcat -v threadtime` filtered to the app pid.

Use it as the **golden path**. When playback stalls on the board, diff against this rather than
reasoning forward from first principles: it names, in order, every stage that has to happen.

```
ExoPlayerImpl: Init [ExoPlayerLib/2.18.5]
LocalSoundPlayer: loadSoundMetadata: loading sound metadata
AudioFocusManager: requestFocus: audio focus request granted
LocalSoundPlayer: loadSoundMetadata: starting playback        <- metadata resolved
LocalSoundPlayer: queueNextSegment: queuing birds_3
okhttp: --> GET https://cdn.trynoice.com/library/segments/birds/birds_3/128k.mp3
okhttp: <-- 200 (90ms, 1392045-byte body)
CCodec: allocate(c2.android.mp3.decoder)   max-input-size 8192, 48000 Hz, 2 ch
AudioTrack: set(): sampleRate 48000, format 0x1, channelMask 0x3, frameCount 15376
AudioTrack::start  ->  LocalSoundPlayer: onMediaPlayerStateChanged: state=PLAYING
AudioTrack::setVolume 0.010000 ... 1.0                        <- noice's fade-in
```

## ★The app narrates itself — read that, not just our markers

noice calls plain `android.util.Log.d(LOG_TAG, …)` with **no `BuildConfig.DEBUG` gate**, and R8 keeps
the tags (`LocalSoundPlayer`, `SoundPlayerManager`, `SoundPlaybackService`, `AudioFocusManager`),
so the release APK reports its own state machine on both platforms. On the board those lines exist
too — but they go to a **different sink** than every westlake marker:

| sink | carries |
|---|---|
| `/data/service/el1/public/appspawnx/adapter_child_<pid>.stderr` | westlake markers, libart probes, JNIMISS |
| `hilog`, domain `0xD000F00`, rendered `C00f00/<TAG>` | **everything the app logs via `android.util.Log`** |

`android_util_Log.cpp` routes `println_native` straight to `HiLogPrint(…, 0xD000F00, …)`. Reading
only stderr — which is what happened for most of this port — makes the app look silent while it is
in fact describing exactly where it stopped. `scripts/host/wl_applog.sh <pid>` dumps the app side.

Two practical notes on hilog here: it rejects multiple "command" flags at once (`-x -P <pid>` is
fine, adding `-z` is not), and the app-type buffer caps at 16 MB, so clear it (`hilog -r`) right
before the action you want to observe.

## Reproducing the reference

```bash
adb install -r -g noice-base.apk          # the pristine APK; no sign-in needed to play a free sound
adb shell monkey -p com.github.ashutoshgngwr.noice -c android.intent.category.LAUNCHER 1
adb shell uiautomator dump /sdcard/ui.xml # navigate by resource-id, not pixels
adb logcat -c && adb shell input tap <play> && sleep 20 && adb logcat -d -v threadtime
```

`Birds` plays without an account — its `birds_3` segment is `isFree=1`.
