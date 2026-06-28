---
name: Pipe + SurfaceView rendering pipeline
description: Full Android-on-OHOS rendering pipeline — dalvikvm subprocess writes DLST display lists to stdout pipe, host renders on SurfaceView via Skia
type: project
---

## Architecture
```
dalvikvm (ART11, ARM64, musl, static) — launched from adb shell (SELinux shell context)
  → Java app → shim Android framework → OHBridge canvas ops
    → [DLST 4B][size 4B][ops] → stdout → tcp_pipe (ARM64 binary)
      → TCP socket localhost:19876
        → Host app ServerSocket → SurfaceView.lockCanvas() → Skia replay → screen
Touch: host → file IPC → VM → dispatch → re-render
Text: long-press → AlertDialog → file IPC → VM EditText.setText()
```

## IPC mechanism (2026-03-30)
- **Why TCP?**: SELinux blocks mkfifo on shell_data_file. Boot image needs shell SELinux context for mmap(PROT_EXEC).
- **tcp_pipe**: Tiny static ARM64 binary (38KB) at `/data/local/tmp/westlake/tcp_pipe`. Reads stdin, forwards to TCP socket. Retries connection for 10s.
- **Host app**: `ServerSocket(19876)` in background thread. `WestlakeVMApkScreen` polls `pipeStream` every 200ms until connected.
- **Launch**: `adb shell sh /data/local/tmp/westlake/launch_mcd.sh`

## Working apps (2026-03-29)
- **MockDonalds**: scrollable menu, tap navigation — WORKS
- **TODO List**: multi-Activity, startActivityForResult, text input, add/remove — WORKS
- **Tip Calculator**: 5 buttons, live recalc — WORKS
- **Counter (Play Store APK)**: full lifecycle, tap +/- changes count — WORKS
- **McDonald's (Play Store APK)**: 177MB, 119K classes, 33 DEX — **RENDERS on phone** via TCP pipe. Menu shows Big Mac, Quarter Pounder, McChicken, etc. with prices and ADD buttons. Boot image loaded from shell context.

## Shim components
- RecyclerView, ConstraintLayout, RelativeLayout (real AOSP impl)
- CardView, AppBarLayout, CollapsingToolbarLayout, FAB, BottomNavigationView
- ManifestParser: parses binary AXML for Application class, activities, providers
- GMS stubs: GoogleApiAvailability, Task/Tasks, ConnectionResult
- Firebase stubs: FirebaseApp, Analytics, Messaging, Auth, Crashlytics
- Dagger/Hilt annotations, javax.inject.Inject
- androidx.startup.InitializationProvider

## Native stubs (openjdk_stub.c)
- Inflater/Deflater: full zlib wrappers
- JarFile.getMetaInfEntryNames
- ICU PatternNative/MatcherNative: regex via POSIX
- Character: 20 methods (isLetterOrDigit, isAlphabetic, etc.)
- Typeface: 10 methods
- ZipFile: full suite

## Key gotchas
- Boot image must include shim DEX — regenerate with dex2oat when shim changes
- Use `--dex-location=` to embed on-device paths in boot image
- Must delete OAT cache (`oat/` and `dalvik-cache/`) when pushing new shim
- Application.onCreate() has 15s timeout — Dagger DI hangs in interpreter mode
- Activity.onCreate() has 15s timeout (thread + join)
- Looper.getMainLooper().getThread() returns Thread.currentThread() — bypasses lifecycle checks
- startActivity wraps performCreate/Start/Resume in catch(Throwable) individually

## McDonald's real app progress (2026-03-30)
- **Security providers FIXED**: Patched core-oj.jar's security.properties (Sun stub instead of Conscrypt/BouncyCastle)
- **Application.onCreate RUNS**: Gets past UUID/SecureRandom, EntryPoints returns dynamic proxies
- **Hilt DI STUBBED**: ApplicationComponentManager, ActivityComponentManager, FragmentComponentManager, EntryPoints — all return stub proxies via java.lang.reflect.Proxy
- **SplashActivity instantiated**: Hilt_SplashActivity created, injector proxy satisfies GeneratedInjector cast
- **Real splash layout RENDERS**: activity_splash_screen.xml inflated, splash_screen_view.xml content injected into fragment container
- **Golden arches visible**: McDonald's red (#DA291C) + yellow "m" (#FFCC00) + "i'm lovin' it" tagline
- **VDEX generated**: All 33 DEX files verified/quickened (148MB vdex in oat/arm64/)
- **dex2oat rebuilt**: Multiple variants (OHOS musl, Android bionic via NDK r25)
- **Bionic dex2oat**: Full ART recompile for Android NDK (Makefile.bionic-arm64, 31MB binary)
  - Boot image gen: ✅ (668K .art, 129K .oat on phone)
  - Quicken compilation: ✅ (2.3MB OAT, 121MB VDEX, 27s)
  - Speed (AOT) compilation: ❌ SEGV_ACCERR in ARM64 code generator for classpath DEX
  - Boot image speed compilation works (129K vs 81K verify) but produces minimal native code
- **Key fix found**: `-Xverify:none` bypasses the Object.getClass() VerifyError for boot image generation
- **Key fix found**: `globals_unix.cc` dlopen check must be disabled for static binaries
- **Key fix found**: `ART_ENABLE_CODEGEN_arm64` must be set for all compiler/dex2oat/runtime objects
- **FULL AOT WORKS**: ALL 33/33 DEX files AOT compiled (71MB native ARM64 code!), loaded by dalvikvm (`kOatUpToDate`, `executable=1`)
- **ROOT CAUSE FIX**: `mprotect(PROT_NONE)` in dlmalloc_space.cc, malloc_space.cc, rosalloc_space.cc, region_space.cc — ART protects unused heap pages which causes SEGV_ACCERR in static-pie binaries. Disabled ALL mprotect(PROT_NONE) in gc/space/*.cc
- **SharedPreferences fix**: Changed from class to interface + SharedPreferencesImpl backing class
- **Key fix**: `method_verifier.cc:2222` VERIFY_ERROR_BAD_CLASS_HARD→SOFT for "returning register with conflict"
- **Key fix**: BCP paths in OAT must EXACTLY match runtime BCP
- **Current state**: Application.onCreate runs (no errors, no crashes) but blocks on futex/threading after 30s. SplashActivity.performCreate also blocks on futex after reading SharedPreferences. The blocking is NOT performance (AOT is fast) — it's a threading/synchronization issue in McDonald's code waiting for background threads.

## Key files changed (2026-03-30)
- `core-oj.jar`: security.properties patched (sun.security.provider.Sun)
- `tcp_pipe.c`: ARM64 binary (38KB) for TCP pipe IPC
- `launch_mcd.sh`: pipes dalvikvm stdout through tcp_pipe to localhost:19876
- `Looper.java`: myLooper() always returns main looper; mThread volatile for re-assignment
- `ViewGroup.java`: resetResolved* methods no-oped (prevent StackOverflow)
- `WestlakeLauncher.java`: tries real Hilt activity, falls back to branded splash from APK resources
- Hilt stubs: ApplicationComponentManager, ActivityComponentManager, FragmentComponentManager, ComponentSupplier, EntryPoints, GeneratedComponent, GeneratedComponentManager, GeneratedComponentManagerHolder
