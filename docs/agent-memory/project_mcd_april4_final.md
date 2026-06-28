---
name: McDonald's April 4 Session — Full Summary
description: Golden arches on screen, stb_image+libwebp decoding, split resource loading, app_process64 tested. Next steps clear.
type: project
---

## Achieved April 4, 2026

### Rendering Pipeline (PROVEN ON PHONE)
- Golden arches splash screen displayed on Pixel 7 Pro ✅
- Demo screen with decoded WebP icons + text + status ✅
- Full pipeline: dalvikvm → Activity → Canvas → display list → pipe → Bitmap → ImageView → screen
- Touch input via file IPC ✅

### Image Decoding
- **stb_image** compiled into bionic dalvikvm (PNG/JPEG)
- **libwebp** (85 objects from AOSP) compiled and linked — WebP decoding ✅
- `OHBridge.imageDecodeToPixels([B)[I` — native JNI: image bytes → ARGB pixel array
- `OP_ARGB_BITMAP` (opcode 12) — new display list op for decoded pixel data
- `BitmapFactory.doDecode` calls native decode → fills Bitmap with real pixels
- `DLIST_MAX` increased to 2MB for large ARGB data

### Resource Loading
- Installed APK's `resources.arsc` (base: 3.3MB, 14103 entries) ✅
- Split APK resources (xxxhdpi: 300KB, 881 entries) merged ✅
- `ResourceTable.mStrings/mNames/mEntryValues` made static (shared across all instances)
- `ResourceTableParser.parse()` merges into existing table instead of replacing
- `Context.getDrawable()` resolves file paths from resource table → loads PNG/WebP via BitmapFactory
- `LayoutInflater.applyXmlAttributes` handles `android:src` (ATTR_SRC=0x01010119) for ImageViews
- `applyByName` handles `srcCompat` attribute

### app_process64 Investigation
- Real Android context (`ContextImpl`) created via `ActivityThread.systemMain()` ✅
- Real `Resources` available ✅
- 4 McD drawables decoded by REAL framework `getDrawable()` → 21KB PNG ✅
- `canvasDrawImage` via pipe works from app_process64 ✅
- **BLOCKER**: `app_process64` can't run as subprocess of untrusted app (SELinux)
- Works from adb shell but not from host APK's ProcessBuilder
- `ohbridge_pipe.so` compiled as shared library (516KB with stb_image+libwebp)

### Host App Changes
- Raw `ImageView` + `Bitmap` rendering (no Compose SurfaceView — lifecycle issues on Android 15)
- `surfaceDestroyed` NOT cleared (prevents holder loss during window setup)
- `startWithConfig` runs on `Dispatchers.IO` (prevents main thread blocking)
- `System.out.println` → `System.err.println` (prevents text on binary pipe)
- Pipe reader polls for both pipeStream AND surfaceHolder

### Key Files Modified
- `ohbridge_stub.c`: stb_image, libwebp decode, imageDecodeToPixels, canvasDrawArgbBitmap, OP_ARGB_BITMAP
- `Makefile.bionic-arm64`: WebP objects in link, WebP include for ohbridge
- `BitmapFactory.java`: native decode via OHBridge, Bitmap.setPixels
- `Bitmap.java`: mPixels array, setPixels/getPixels/getPixel, CompressFormat enum
- `Context.java`: getDrawable with real context fallback + file-based loading
- `ResourceTable.java`: static shared maps, split resource merging, mPrevStringPool
- `ResourceTableParser.java`: merge into existing table
- `LayoutInflater.java`: ATTR_SRC handling, Strategy 3.5 file loading, real inflater attempt
- `WestlakeInstrumentation.java`: resolveImageDrawables walk, Hilt _initHiltInternal
- `WestlakeActivityThread.java`: DataSourceHelper stub proxy init, real AssetManager inject attempt
- `WestlakeVM.kt`: ImageView rendering, pipe reader fixes, app_process64 launch
- `WestlakeActivity.kt`: raw ImageView mode, FrameLayout container
- `Application.java`: componentManager/b() for Hilt
- `ApplicationComponentManager.java`: singletonComponent set on stub proxy path
- `OHBridge.java`: imageDecodeToPixels, canvasDrawArgbBitmap declarations

### Next Steps
1. **For OHOS**: stb_image+libwebp in OHOS dalvikvm gives PNG/WebP decode — same as bionic
2. **For better Android demo**: run app_process64 from adb shell, pipe to host app via TCP/named pipe
3. **For real app screens**: need Fragment navigation + network (Retrofit API calls)
4. **Dashboard content**: needs login → auth token → API calls → menu data → RecyclerView
