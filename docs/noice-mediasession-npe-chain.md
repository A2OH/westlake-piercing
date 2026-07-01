# noice "clean death" = an uncaught-NPE chain, and the audio path

## Root cause (the big one)

For a long time noice would die with **no tombstone, no Java exception in the
obvious logs, no AMS kill** — a "clean death". An LD_PRELOAD death-catcher
(`diagnostics/deathcatch.c`) that interposes the process-teardown paths proved
what actually happens:

```
uncaught Java NPE
  -> AppSpawnXInit uncaught-exception handler
  -> System.exit(1)
  -> JVM_Halt(1)                 (libopenjdkjvm)
  -> art::Runtime::CallExitHook  (libart)
  -> _exit(1)
```

So every "clean death" is an **uncaught NullPointerException on an unimplemented
adapter system-service**, swallowed into `System.exit`. It is *not* the
signal-recovery / implicit-null-check wall. Read the exception with:

```
hilog -G 64M; hilog -r; <repro>; hilog -x | grep AppSpawnXInit
```

## The chain (noice's eager MediaSession init, in SoundPlaybackService.onCreate)

noice's `SoundPlayerManagerMediaSession` is built lazily and touched during
`onCreate`. Each Android system service it uses is a null gap in the adapter, so
it NPE-crashed one step at a time. Fixed each at the framework (BCP) layer via
dexlib2 patchers (`framework-patch-tools/`) + boot-image regen:

| # | gap | fix (see `framework-smali-patches/`) |
|---|-----|--------------------------------------|
| 1 | `PendingIntent.get{Activity,Broadcast,Service}` return null (`getIntentSenderWithFeature` stub) -> `mainActivityPi!!` NPE | insert non-null `IIntentSender$Stub.asInterface(new Binder())` proxy after each getIntentSender* |
| 2 | `MediaSession.<init>` NPE on null `MediaSessionManager` | null-manager branch builds proxy `ISession` + `Token(dummy ISessionController)`, `mController=null`, returns |
| 3 | `AudioProductStrategy.initializeAudioProductStrategies` -> unimplemented `native_list_*` (via `AudioAttributes.setLegacyStreamType`) | return empty `ArrayList` |
| 4 | `AudioVolumeGroup.initializeAudioVolumeGroups` -> same | return empty `ArrayList` |
| 5 | `DisplayManagerGlobal.getWifiDisplayStatus` null -> `MediaRouter.getFeatureState` NPE | return `new WifiDisplayStatus()` (defaults = feature unavailable) |
| 6 | `MediaRouter$Static.<init>` NPE on null `IMediaRouterService.registerClientAsUser` | null->proxy `IMediaRouterService$Stub.asInterface(new Binder())` |
| 7 | Favorites/Presets nav: `ShortcutManager` null -> `getDynamicShortcuts()` NPE | `SystemServiceRegistry$88` uses non-throwing `getService`; `getDynamicShortcuts/getManifestShortcuts` return empty |

Result: **noice cold-launches, renders the full Sound Library, survives the play
tap, and navigates every bottom-nav page (Library / Favorites / Timer / Alarm /
Account) with no crash**; ExoPlayer initializes (drm/upstream classes,
`exoplayer_internal.db`) and `startService(playSound)` fires
`onCreate`/`onStartCommand`.

## The audio output path (validated)

`toneplayer.c` drives `OH_AudioRenderer` (the target of the AudioTrack->OHOS
shim) with a 440 Hz sine: Create / SetRendererInfo / GenerateRenderer / Start
all rc=0, callback pulls PCM for the full duration. So the OHOS audio *output*
backend works; any AudioTrack client bridged to it will produce sound.

## Why you still don't hear noice

noice ships **no bundled/downloaded sounds** — its `files/offline-sounds/` is an
empty ExoPlayer `SimpleCache`; it **streams** from the CDN. The board has no WAN.
The CDN *is* public though: `https://cdn.trynoice.com/library/library-manifest.json`
(only the account API needs auth), and noice uses `DefaultHttpDataSource`
(HttpURLConnection, honours `https.proxyHost`). So a device->host->CDN tunnel
(`diagnostics/proxy.py` + `hdc rport` + `bridge-src/proxy_inject.cpp`, gated on
`/data/local/tmp/oh_proxy`) makes the CDN reachable from the device (verified end
to end with `netcheck.c`). Remaining to make it audible while tethered:

1. the adapter `ConnectivityManager` reports *offline*
   (`getActiveNetwork`/`NetworkCapabilities`/`registerDefaultNetworkCallback`),
   so noice won't attempt to stream until it reports a validated INTERNET network;
2. it only works while the board is tethered to the host running the proxy.
