---
name: McDonald's Golden Arches on Pixel 7 Pro
description: Full rendering pipeline proven — splash screen with golden arches visible on phone. Dashboard layout inflates but needs drawable decoding.
type: project
---

## Milestone: Golden Arches Rendered on Pixel 7 Pro (2026-04-04)

### What Works
- **Full pipeline**: dalvikvm → WestlakeLauncher → Activity → View tree → Canvas → display list → pipe → host Bitmap → ImageView → screen
- **Splash screen**: McDonald's golden arches WebP logo perfectly rendered
- **Real layout inflation**: `activity_splash_screen.xml` inflated from installed APK's resources.arsc (14,103 entries)
- **WebP image decoding**: splash_screen.webp decoded via OP_IMAGE display list op
- **Both activities**: SplashActivity + HomeDashboardActivity onCreate complete
- **Hilt DI**: stub proxy injection working
- **Touch input**: working via file IPC
- **Resource table**: correctly maps resource IDs to file paths from installed APK

### Working Configuration
- **dalvikvm**: build-bionic-arm64 (27MB, v085, Android bionic libc)
- **Core JARs**: AOSP 11 (core-oj 5MB, core-libart 660KB, core-icu4j 2.6MB)
- **Boot images**: compiled on-device by art-universal-build dex2oat2 (v085)
- **BCP**: core-oj:core-libart:core-icu4j:aosp-shim.dex
- **resources.arsc**: from installed McD APK base.apk (3.3MB, 14103 entries)
- **res/ files**: extracted from installed APK (3842 files including 2358 PNGs)
- **Host app**: ImageView + Bitmap rendering (no SurfaceView — destroyed by ComponentActivity)

### Remaining for Full UI
1. **BitmapFactory**: returns empty bitmaps — needs real PNG/JPEG pixel decoding
   - Current: parses headers only, creates blank bitmaps
   - Fix: implement Java PNG decoder OR use app_process64 with real framework BitmapFactory
2. **Dashboard content**: toolbar/nav icons are blank because ImageView drawables are empty bitmaps
3. **Fragment navigation**: content fragments not loaded (need FragmentManager)
4. **Network**: Retrofit/OkHttp in DEX files — should work on real phone but untested

### Key Files
- Host activity: `WestlakeActivity.kt` line 108+ (McD raw ImageView mode)
- Pipe reader: `WestlakeVM.kt` readPipeAndRenderLocked (Bitmap rendering path)
- Display list replay: `WestlakeVM.kt` replayDisplayList
- Context.getDrawable: `Context.java` line 353 (loads from resource table)
- BitmapFactory.doDecode: `BitmapFactory.java` line 154 (STUB — blank bitmaps)
- Resource table: `ResourceTable.java` (14103 entries from installed APK)

### Two Paths Forward
1. **app_process64**: Use phone's real ART → real BitmapFactory, real Resources, real LayoutInflater. Instant boot. Needs shim refactoring to not override framework classes.
2. **Custom dalvikvm + real image decoder**: Add stb_image or Java PNG decoder to BitmapFactory. Keep full control but slower.
