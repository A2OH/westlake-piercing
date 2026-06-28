---
name: McDonald's Next Steps — April 4 2026
description: Clear next steps after golden arches milestone. Two parallel tracks for image decoding and network.
type: project
---

## Achieved
- Golden arches splash screen rendering on Pixel 7 Pro ✅
- Full pipeline proven: dalvikvm → Activity → Canvas → pipe → Bitmap → screen ✅
- Real layout inflation from installed APK (14103 resources) ✅
- Both activities onCreate complete (Splash + HomeDashboard) ✅
- Touch input working ✅
- Boot images compiled on-device (v085) ✅

## Blocking: BitmapFactory Returns Blank Bitmaps
- `BitmapFactory.doDecode()` parses PNG/JPEG headers (gets dimensions) but creates EMPTY bitmaps
- All ImageView drawables are transparent → toolbar icons, nav bar, menu images invisible
- Dashboard renders as solid navy background (views exist but no visual content)
- Location: `shim/java/android/graphics/BitmapFactory.java` line 154

## Next Step Options (pick one)

### Option A: Add stb_image to OHBridge native (RECOMMENDED)
- Add `imageDecodeToPixels(byte[] data) → int[] rgba` native method to OHBridge
- Use stb_image (already in art-universal-build/stubs/) to decode PNG/JPEG/WebP
- BitmapFactory.doDecode calls this → gets real pixel data → fills Bitmap
- Works on BOTH bionic dalvikvm AND OHOS dalvikvm
- Effort: ~2 hours (add JNI method, compile into ohbridge_stub.c, rebuild)

### Option B: app_process64 with shim refactoring
- Split aosp-shim.dex into engine.dex (WestlakeLauncher, OHBridge) + shim.dex (framework overrides)
- On app_process64: only load engine.dex, use phone's real framework classes
- Real BitmapFactory, real Resources, real network
- Effort: ~4 hours (refactor, test both paths)

### Option C: Network / Login
- Implement enough HTTP/SSL stubs for OkHttp/Retrofit to make API calls
- Or use app_process64 where real OkHttp works
- McDonald's APIs return menu data, offers, etc.
- Effort: ~6 hours (SSL certs, cookie handling, auth flow)

## Working Configuration (for reference)
```bash
# Bionic dalvikvm (renders golden arches)
D=/data/local/tmp/westlake
BCP=$D/core-oj.jar:$D/core-libart.jar:$D/core-icu4j.jar:$D/aosp-shim.dex
./dalvikvm -Xbootclasspath:$BCP -Ximage:boot.art -Xverify:none -Xint \
  -Xgc:nonconcurrent -Xms256m -Xmx768m \
  -Dwestlake.apk.package=com.mcdonalds.app \
  -Dwestlake.apk.activity=com.mcdonalds.mcdcoreapp.common.activity.SplashActivity \
  -Dwestlake.apk.path=$D/mcd_classes.dex \
  -Dwestlake.apk.resdir=$D/mcd_res \
  -classpath $MCD_CP com.westlake.engine.WestlakeLauncher

# Host app: WestlakeActivity with raw ImageView + Bitmap (not Compose SurfaceView)
# Pipe reader: readPipeAndRenderLocked with Bitmap target
```
