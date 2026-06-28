---
name: noice in-process launch breakthrough (option 3)
description: 2026-05-14 — NoiceInProcessActivity executes noice's real MainActivity.onCreate end-to-end inside WestlakeHost APK process via real Android scaffolding; Hilt-injected dependencies resolve, MainActivity reaches startActivity(AppIntroActivity); remaining blockers are storage permissions + cross-activity navigation, both tractable
type: project
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---
## What works (validated on real Android, OnePlus 6 cfb7c9e3, 2026-05-14)

`am start -n com.westlake.host/.NoiceInProcessActivity` runs the following path inside the host process — NO dalvikvm, NO binder pivot, NO V2 substrate:

1. ✅ `createPackageContext("com.github.ashutoshgngwr.noice", CONTEXT_INCLUDE_CODE)` loads noice's classes
2. ✅ `noiceCl.loadClass("com.github.ashutoshgngwr.noice.NoiceApplication").newInstance()` — instantiates noice's real Hilt-instrumented Application
3. ✅ `Application.attachBaseContext(noiceCtx)` + reflective wire `LoadedApk.mApplication = app` so `noiceCtx.getApplicationContext()` returns it
4. ✅ `Application.onCreate()` runs without crashing
5. ✅ `MainActivity.newInstance()` + reflective `Activity.attach(..., noiceApp, intent, ai, ...)` with real `ActivityThread.sCurrentActivityThread`
6. ✅ Reflective `MainActivity.onCreate(null)` invoked
7. ✅ Hilt lazy `settingsRepository$2.b()` resolves via `f3.y.l` → `e0.g.q` — **the exact line V2 substrate could never get past**
8. ✅ MainActivity runs to natural conclusion at line 209: `startActivity(intent for AppIntroActivity)` (first-run intro screen, same behavior as standalone `am start`)
9. ❌ Cross-activity nav fails: `ActivityNotFoundException` — AppIntroActivity not registered under `com.westlake.host`
10. ❌ Background SQLite init crashes: `EACCES` writing `/data/user/0/com.github.ashutoshgngwr.noice/databases` (host UID can't write noice's app-data dir)

## Why this matters
- **Resolves the V2 substrate ceiling.** The Hilt-internal context wrapping that V2's macro shim couldn't unwrap (CR58 close-out) is just a consequence of incomplete real-Android scaffolding. With real `LoadedApk + ContextImpl + PhoneWindow + ActivityThread`, Hilt's `EntryPoints.get(Application, Class)` walks the chain to a non-null Application and resolves cleanly.
- **No per-app hacks.** The NoiceInProcessActivity is ~150 LOC of generic plumbing: load classes via classloader, attach Application + Activity, run lifecycle. No noice-specific Hilt knowledge.

## Update 2026-05-14 final — McD ALSO RUNS, framework is generic

McdInProcessActivity (sed-clone of NoiceInProcessActivity with McD constants) successfully:
- Loads `com.mcdonalds.app.application.McDMarketApplication` via createPackageContext
- Wires LoadedApk + dataDir patches + LocaleManager hook
- Runs `com.mcdonalds.mcdcoreapp.common.activity.SplashActivity.onCreate()` cleanly
- Steals content view → drives lifecycle → renders **McD's "Check your Wi-Fry" offline screen** (golden arches, McD fries graphic, "It looks like you're offline. Check your internet connection and try again.", Try again button) inside the host process
- Screenshot at `/tmp/mcd_v4.png` shows the McD-branded offline screen with system crash dialog on top (background McD UI is the actual Westlake render)

The same five-pillar pattern (hidden-api bypass + LoadedApk dir redirect + safe-context bind stub + LocaleManager binder hook + lifecycle drive to Resumed) works generically. Difference between apps lives in 4 constants (PKG, MAIN_CLS, APP_CLS, alias list).

### McD-specific issues found this session
1. **`getApplicationLocales` SecurityException** — McD's AppCompatDelegate.Api33Impl directly calls LocaleManager which requires READ_APP_SPECIFIC_LOCALES (not grantable to non-system apps). Resolved by **`stubLocaleManager()`** — uses `java.lang.reflect.Proxy` on `ILocaleManager` interface, replaces `LocaleManager.mService` field to return `LocaleList.getEmptyLocaleList()` for `getApplicationLocales` calls. Applied to BOTH NoiceInProcessActivity + McdInProcessActivity for defensiveness.
2. **Cross-activity nav uses hardcoded `com.mcdonalds.app` package** — McD's SplashActivity builds intents with `setClassName("com.mcdonalds.app", "...HomeDashboardActivity")` literally, bypassing our patched ApplicationInfo.packageName. Manifest `<activity-alias>` only resolves intents targeting `com.westlake.host/`. **Open item**: needs an Instrumentation-level intent rewriter to redirect mcd-package intents → host-package proxy. Java-based subclass should compile (Kotlin can't override execStartActivity due to hidden-API restrictions in SDK stubs).
3. **`installSwallowingUncaughtHandler()`** added — catches background coroutine FATALs (~11 swallowed in nav-stress) without killing process. Main-thread crashes still chain to system handler.

### Verified working in Westlake (com.westlake.host process)
- **noice**: Welcome → Library (offline state) → Favorites (3 cached presets: Beach, Camping, Thunderstorm) → Profile (Account / Sign up / Sign in / View plans). Full fragment nav, ViewModels resolve, Compose UI renders.
- **McD**: SplashActivity → Wi-Fry offline screen with McD branding. Crashes on HomeDashboardActivity cross-pkg intent (open item #2 above).

### Files touched this final iteration
- `westlake-host-gradle/app/src/main/java/com/westlake/host/NoiceInProcessActivity.kt` (~330 LOC)
- `westlake-host-gradle/app/src/main/java/com/westlake/host/McdInProcessActivity.kt` (~330 LOC, sed-clone)
- `westlake-host-gradle/app/src/main/AndroidManifest.xml` (12 permissions + 13 activity aliases for both apps)

### Reproducer
```
cd westlake-host-gradle && ./gradlew :app:assembleDebug
adb push app/build/outputs/apk/debug/app-debug.apk /data/local/tmp/host.apk
adb shell "pm install -r -t /data/local/tmp/host.apk"
adb shell "am force-stop com.westlake.host"
adb shell "input keyevent KEYCODE_WAKEUP"
adb shell "am start -n com.westlake.host/.NoiceInProcessActivity"   # noice
adb shell "am start -n com.westlake.host/.McdInProcessActivity"     # McD
```

## Update 2026-05-14 late — VISUAL CONFIRMATION

noice's actual Welcome / AppIntro UI **renders inside com.westlake.host process** (noice logo, "Focus, meditate and relax with natural calming noise.", SKIP/next controls, 6-page intro dots). Screenshot at `/tmp/noice_inprocess5.png`.

Three further pieces landed for visible rendering:
1. **Lifecycle drive** — `driveLifecycleToResumed()` reflectively calls `performStart` + `performResume` + `performTopResumedActivityChanged` on noice's Activity, since the Compose/Fragment/ViewModel layer only inflates the visible tree during start/resume, not onCreate.
2. **onNewIntent + currentNoiceActivity tracking** — when noice does `startActivity(AppIntroActivity)` and Android delivers as `START_DELIVERED_TO_TOP` (result code 3), our `onNewIntent` swaps target and reloads.
3. **NoiceSafeContext** — `ContextWrapper` that stubs `bindService` / `startService` / `startForegroundService` for cross-package noice service intents (e.g., SoundPlaybackService), returning false / pretending success. Without this, `LibraryViewModel.<init>` SecurityException-killed the process. Wraps both `noiceApp.mBase` AND the Activity's `attachCtx`-inner-base.

## Update 2026-05-14 evening — BOTH BLOCKERS RESOLVED

Cross-activity nav + storage redirect both landed; full launch now completes cleanly:

```
✅ Hidden API restrictions bypassed (VMRuntime.setHiddenApiExemptions double-reflection trick)
✅ Patched LoadedApk.mDataDirFile / mDeviceProtectedDataDirFile / mCredentialProtectedDataDirFile
✅ Post-patch noiceCtx.dataDir = /data/user/0/com.westlake.host/noice_data (MATCH)
✅ noice Application.onCreate() OK (no EACCES)
✅ MainActivity.onCreate() returned (Hilt resolved, all paths satisfied)
✅ Content view transferred to host
✅ ActivityTaskManager: Displayed com.westlake.host/.NoiceInProcessActivity (+1s213ms)
✅ Process stays resident, no crash, no FATAL
```

Fixes:
1. **Storage redirect** — `redirectDataDir()` patches `LoadedApk.{mDataDirFile, mDeviceProtectedDataDirFile, mCredentialProtectedDataDirFile}` to a host-writable scratch dir (`getFilesDir().parentFile + "/noice_data"`). Requires hidden API bypass because two of those three fields are `max-target-o` (denied for target_sdk > 26).
2. **Hidden API bypass** — `bypassHiddenApiRestrictions()` calls `VMRuntime.setHiddenApiExemptions(["L"])` via double-reflection: `Class.class.getDeclaredMethod("getDeclaredMethod", ...)` → grab `setHiddenApiExemptions` Method object → invoke. Bypasses the blocklist on the meta-call itself.
3. **Cross-activity nav** — manifest `<activity-alias>` entries map each known noice activity class (MainActivity, AppIntroActivity, SignInLinkHandlerActivity, SetAlarmHandlerActivity, StripeCheckoutSessionCallbackActivity) → NoiceInProcessActivity. When noice's MainActivity calls `startActivity(AppIntroActivity)`, Android resolves the alias to NoiceInProcessActivity, and `getIntent().component.className` carries the alias FQCN so NoiceInProcessActivity can load that target class via noice's classloader.

## Original blockers (HISTORICAL; both resolved above)

### (a) Cross-activity navigation
noice's MainActivity calls `startActivity(intent for AppIntroActivity)`. Intent is built with `setClassName(applicationInfo.packageName, ...)` — and we patched packageName to "com.westlake.host", so it looks up AppIntroActivity under host's manifest where it isn't registered.

Options:
- Register noice's manifest activities (AppIntroActivity, SignInLinkHandlerActivity, etc.) as `<activity-alias>` in host manifest pointing to a generic `NoiceProxyActivity` that dispatches by class name
- Hook `startActivity` via reflective Instrumentation replacement that rewrites the package back to noice's and resolves through noice's PackageManager
- Override `Activity.startActivityForResult` chain (more invasive)

### (b) App-data dir permissions
noice writes shared_prefs / databases / cache to `/data/user/0/com.github.ashutoshgngwr.noice/` but we run as host UID (10218 vs noice's 10206).

Options:
- Add `android:sharedUserId="com.westlake.host.noice.shared"` to BOTH host and noice manifests (requires re-signing noice APK — invasive but clean)
- Override `ContextImpl.mDataDir` reflectively to point at host's data dir + symlink subdirs
- Use a stub Context that overrides `getFilesDir/getCacheDir/getDir/getDatabasePath/getSharedPreferences` to host-writable paths

## Key code
`westlake-host-gradle/app/src/main/java/com/westlake/host/NoiceInProcessActivity.kt` (~210 LOC)

Manifest entry: `<activity android:name=".NoiceInProcessActivity" android:exported="true" android:theme="@android:style/Theme.DeviceDefault.NoActionBar" />`

## Test command
```
cd westlake-host-gradle && ./gradlew :app:assembleDebug
adb push app/build/outputs/apk/debug/app-debug.apk /data/local/tmp/host.apk
adb shell "pm install -r -t /data/local/tmp/host.apk"
adb shell "am start -n com.westlake.host/.NoiceInProcessActivity"
adb logcat | grep -E "NoiceInProcess|noice"
```

## Strategic implication
Option 3 (real Android in-process) is a viable PARALLEL strategy to the binder pivot (V2 substrate + dalvikvm). It's lower architectural risk because real Android is doing the heavy lifting. Phase 2 OHOS port (CR41 roadmap) could leverage this pattern if OHOS exposes a compatible Activity/ActivityThread surface.
