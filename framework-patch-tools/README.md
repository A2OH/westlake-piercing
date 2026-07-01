# framework-patch-tools

dexlib2-based patchers that produce the `framework-smali-patches/` classes by
surgically editing `framework.jar`'s dex (no full re-pool of unrelated classes).
Each reads a dex and writes a patched dex; the boot image is then regenerated
with `dex2oat` over the current boot classpath jars.

Build: `javac -cp "$(cat classpath.txt)" Patch*.java`
(smali-dexlib2 / baksmali / guava / jcommander on the classpath; see classpath.txt)

| tool | what it does |
|------|--------------|
| `PatchPI.java`        | `PendingIntent.get{Activity,Broadcast,Service}AsUser`: when `IActivityManager.getIntentSenderWithFeature` returns null, synthesize a non-null `IIntentSender$Stub.asInterface(new Binder())` proxy so the factory never returns null. |
| `PatchMS.java`        | `MediaSession.<init>`: when `getSystemService(media_session)` is null, take a fabricated branch that builds a proxy `ISession` + `Token(dummy ISessionController)` and returns, instead of NPE-ing. |
| `PatchEmptyList.java` | Replace a method body with `return new ArrayList()` (used for `AudioProductStrategy.initializeAudioProductStrategies`, `AudioVolumeGroup.initializeAudioVolumeGroups`, `ShortcutManager.getDynamicShortcuts/getManifestShortcuts`). |
| `PatchReturnNew.java` | Replace a method body with `return new <Type>()` (used for `DisplayManagerGlobal.getWifiDisplayStatus` -> `new WifiDisplayStatus()`). |
| `PatchProxy.java`     | After a `<Stub>.asInterface(...)` result, insert a null->proxy substitution (used for `MediaRouter$Static.<init>` `IMediaRouterService`). |
| `PatchS88.java`       | Rewrite `SystemServiceRegistry$88.createService` (ShortcutManager fetcher) to use non-throwing `ServiceManager.getService` so the fetcher yields a non-null manager. |
