# noice: audible MP3 playback end-to-end (tap play → speaker)

This documents the full fix that makes the noice app
(`com.github.ashutoshgngwr.noice`) **play audible sound** on the OpenHarmony
AOSP-adapter (appspawn-x) board: tap a free sound's ▶ → the app streams the MP3
from the CDN, decodes it through a **`android.media.MediaCodec` → OHOS
`OH_AudioCodec` bridge**, and renders PCM through the AudioTrack→`OH_AudioRenderer`
shim to the speaker. It stays alive and stable during playback.

Prior docs (`noice-mediasession-npe-chain.md`) covered making *tap play not crash*.
This covers the remaining chain to *actual sound*, which turned out to be **six
independent gates**, each of which silently blocked playback.

## Signal path

```
tap ▶ → SoundPlaybackService.onStartCommand(playSound, soundId)
      → SoundPlayerManager.g()  [needs audio focus]        ── gate 2
      → AudioFocusManager.b() → AudioManager.requestAudioFocus() == GRANTED
      → SoundPlayer.e() → LocalSoundPlayer.o()  (queueNextSegment)
          [filters to isFree segments when signed-out — free sounds pass]
      → build "noice://cdn/library/…/128k.mp3" → ExoPlayer.setMediaItem + prepare
      → d3/a HTTP DataSource → https://cdn.trynoice.com/library/…/128k.mp3  (streams)
      → ExoPlayer MediaCodecAudioRenderer → MediaCodecList/MediaCodec  ── gates 3,4
      → OH_AudioCodec decodes MP3 → PCM
      → DefaultAudioSink → android.media.AudioTrack → OH_AudioRenderer shim
      → OHOS AudioPolicyService → SPEAKER  (must be unmuted)            ── gate 6
```

The subscription-status service bind (gate 1) and audio focus (gate 2) both sit
*before* `queueNextSegment`, so the app never even opened a socket until they were
fixed. Then the codec (gates 3–5) and mute (gate 6) had to fall in order.

## The six gates

### 1. In-app service bind (the recurring adapter wall)
`SoundPlaybackService.onCreate` binds `SubscriptionStatusPollService` to resolve the
subscription state, and playback is gated on that state. The adapter failed **all**
in-app service binds (`nativeConnectAbility` returned −1), so the play blocked with
no error, no socket.

**Fix (native, no boot regen):** synchronous in-process bind implemented in
`oh_inproc_service.cpp` → `inproc_bindServiceSync2()`:
- creates the Service (`attach` + `onCreate`, cached), **or** reuses an
  already-running instance found in `ActivityThread.mServices`
  (`findExistingService()`),
- calls `Service.onBind(intent)`,
- delivers the resulting `IBinder` to the app's `ServiceConnection` via
  `adapter.activity.ServiceConnectionRegistry.onServiceConnected(connId,…)`
  (which posts the real callback to the app main looper).

Wired in `activity_manager_adapter.cpp` `nativeConnectAbility`:
- light services (e.g. `SubscriptionStatusPollService`) → `createIfMissing=1`
  (create on demand),
- `SoundPlaybackService` → `createIfMissing=0` (**reuse-only**, never create the
  heavy ExoPlayer `onCreate` at a launch-time bind — that is what regressed the app
  in an earlier attempt; it is started via `startService(playSound)` anyway).

This is the first working in-app service bind for adapter apps: the app now receives
real `onServiceConnected` callbacks.

### 2. Audio focus never granted
`SoundPlayerManager` requests `AudioManager.requestAudioFocus()` and only proceeds to
`queueNextSegment` on `AUDIOFOCUS_REQUEST_GRANTED (1)`. The adapter's stub audio
service never granted it, so the player parked in "pending focus" with no log/throw.

**Fix (framework.jar smali patch + boot regen):** `PatchReturnOne.java` rewrites all
five `android.media.AudioManager.requestAudioFocus(...)` overloads to
`const/4 v0,1 ; return v0` (return `AUDIOFOCUS_REQUEST_GRANTED`). This is an
ABI-boundary fix that also unblocks any other focus-gated app.

### 3. `libmedia_jni.so` missing → `UnsatisfiedLinkError`
`android.media.MediaCodecList.<clinit>` / `MediaCodec.<clinit>` call
`System.loadLibrary("media_jni")` and then, in the **same** `<clinit>`, a native
method (`native_init`). ART re-resolves that class's natives against the just-loaded
library, which drops the `RegisterNatives` bindings the bridge set in the zygote
parent. The device has no `libmedia_jni.so` at all → `UnsatisfiedLinkError`, uncaught
→ `System.exit`.

**Fix:** a **stub `libmedia_jni.so`** (`libmedia_jni_stub.c`, deployed to
`/system/lib` + `/system/android/lib`) whose `JNI_OnLoad` `dlopen`s the bridge and
calls `register_MediaCodec_shim` (exported `extern "C"`) — i.e. it does exactly what
a real `libmedia_jni` would: register the media natives, at the moment
`loadLibrary` runs, in the child. Also added `MediaCodecList.getCodecCapabilities`
(via `CodecCapabilities.createFromProfileLevel`) and `native_getGlobalSettings` to
the shim.

### 4. The MediaCodec → OH_AudioCodec bridge (async)
`oh_mediacodec_shim.cpp` bridges `android.media.MediaCodec` + `MediaCodecList` to
OHOS `OH_AudioCodec` (`libnative_media_acodec.so`). MP3 mime → `CreateByMime`,
`Configure`/`Prepare`/`Start`, and callback-driven buffer flow.

Two things were essential:
- **noice's ExoPlayer uses the ASYNCHRONOUS MediaCodec adapter** (`setCallback`),
  not sync `dequeue`. So `native_setCallback` must fire
  `MediaCodec$Callback.onInputBufferAvailable` / `onOutputBufferAvailable` /
  `onOutputFormatChanged` from the OH `onNeedInputBuffer` / `onNewOutputBuffer`
  callbacks. (A sync-mode dequeue path is also implemented as a fallback.)
- **The OH async API delivers the `OH_AVBuffer` *inside* the callback** —
  `OH_AudioCodec_GetInputBuffer(index)` returns NULL. So the shim **captures and
  stores the `OH_AVBuffer` per index** (`Codec::inBufs` / `outBufs`) and returns it
  from `getBuffer` / uses it in `queueInputBuffer`. Without this, ExoPlayer gets a
  null `ByteBuffer` and NPEs on `Buffer.position()`.

Verified: `queueInput size=384` (MP3 frames in) → `cbOutput size=4608` (PCM out) →
ExoPlayer reads the output buffer → AudioTrack.

### 5. OH callback threads must detach from the JVM
The `OH_AudioCodec` callbacks run on native threads the shim attaches with
`AttachCurrentThread`. ART aborts a process if an attached thread exits without
`DetachCurrentThread` ("Native thread exited without calling DetachCurrentThread").

**Fix:** a `pthread_key` with a destructor (`detachThread`) registered in
`attachEnv()`, so each OH callback thread detaches when it exits.

### 6. The device MUSIC stream was muted
After all of the above the full pipeline ran — `AudioPolicyService` showed an active
MUSIC render stream for the app's uid on the SPEAKER — but produced no audible sound
because the device MUSIC stream had `mute = 1`.

**Fix:** unmute / raise MUSIC volume via volume-up key events
(`uinput -K -d 16 -u 16`, keycode 16 = `VOLUME_UP`). `mute` → 0.

## Also required (established earlier this pass)

- **Connectivity:** the app's online check is
  `getNetworkCapabilities(getActiveNetwork()).hasCapability(x)` OR
  `getActiveNetworkInfo().isConnectedOrConnecting()`. In `adapter-mainline-stubs.jar`,
  `hasCapability`/`isConnectedOrConnecting` already return true and
  `getNetworkCapabilities` is non-null; the only gap was `getActiveNetwork()`
  returning null → patched to `new Network()` (via `PatchReturnNew`). See
  `framework-smali-patches/unified-connectivity/`.
- **Free vs premium:** `LocalSoundPlayer.o()` filters segments to `isFree` when
  signed-out; fully-free sounds (all `sound_segment.is_free = 1`, e.g. campfire /
  white noise) stream and play. Premium segments are the app's own paywall, not an
  adapter limitation. Streaming a free segment needs no auth.
- **Network reachability:** the test board's firewall is a **whitelist** that allows
  `trynoice.com` (both `cdn.` and `api.` reachable on :443) but blocks the general
  internet. `diagnostics/netcheck2.c` is a small native DNS + TCP-connect probe used
  to confirm this from the device.

## Final configuration (validated end-to-end)

| Artifact | Role |
|---|---|
| `bridge-src/oh_mediacodec_shim.cpp` | MediaCodec/MediaCodecList → OH_AudioCodec (async + sync) |
| `bridge-src/oh_inproc_service.cpp` | synchronous in-process service bind |
| `bridge-src/activity_manager_adapter.cpp` | `nativeConnectAbility` bind wiring (light vs heavy) |
| `native-libs/libmedia_jni_stub.c` | stub `libmedia_jni.so`, re-registers shim natives |
| `framework-patch-tools/PatchReturnOne.java` | `requestAudioFocus → GRANTED` (framework classes2.dex) |
| `framework-smali-patches/unified-connectivity/` | `getActiveNetwork → new Network` |
| `diagnostics/netcheck2.c` | device DNS + TCP reachability probe |

Result: the app launches, navigates, and **plays free sounds audibly and stably**.
The play button UI still shows ▶ rather than pause — the `SoundPlaybackService`
UI-side bind is display-only and races the service `onCreate`; it does not affect
audio.
