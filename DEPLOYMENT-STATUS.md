# Bridge deployment + appspawn-x/framework run — status (2026-07-08)

## ✅ appspawn-x + framework RUN on the board; framework loads the bridge in-process
Ran the staged appspawn-x (/data/local/tmp/asx/) with the framework:
```
cd /data/local/tmp/asx
ANDROID_ROOT=/system ICU_DATA=/data/local/tmp/asx \
LD_LIBRARY_PATH=/data/local/tmp/asx:/system/lib64:/system/lib64/platformsdk:/system/lib64/chipset-sdk:/system/lib64/chipset-sdk-sp \
LD_PRELOAD=/data/local/tmp/asx/liblog_shim.so setsid ./appspawn-x >asx.out 2>asx.err </dev/null &
```
Result: **Phase 1 (security+socket) OK → Phase 2 ART VM OK → framework preload OK**
("Preloaded 30 classes", "Cached 11 adapter classes", "Preload complete in ~1470ms",
"Preload completed successfully") → **Phase 4 "Ready to accept spawn requests"**, listening
on /dev/unix/socket/AppSpawnX. The VM + 24Q4 framework run on art-15 arm64.

## ✅ Bridge loads in-process (the __h fix works) — resolving deps one layer at a time
The framework's preload calls `Runtime_nativeLoad path=liboh_adapter_bridge.so`. With the
__h toolchain fix, the bridge's OHOS symbols ALL resolve. Fixed in sequence:
1. `JNI_GetCreatedJavaVMs not found` → **added libart.so to the bridge's NEEDED** (libart
   defines it; ART's nativeLoad dlopen didn't put it in scope). FIXED.
2. Now: `android::EmptyAssetsProvider::Create not found` → the bridge needs the **AOSP
   native support libs** (androidfw/base/utils/binder), currently arm32-only.

## Remaining: build the AOSP native support stack for arm64
The bridge's *_aosp.cpp (AssetManager/ApkAssets/ResXML/Parcel) are JNI wrappers around
AOSP C++ libs. Undefined by lib:
- **libandroidfw** (AssetsProvider ×10, ApkAssets, ResXMLTree/Parser ×13, ResStringPool)
- **libbase** (×15), **libutils** (String8/16 ×17), **libbinder** (sp/IBinder/RpcSession ×16)
- (for render later: **libhwui + skia + libminikin/ft2/harfbuzz** — 11 skia syms already seen)
These are arm32 in /home/dspfac/bridge-build/out/aosp_lib/ (machine ARM). Build recipe:
`bridge-build/build/build_aosp_lib.sh` (targets libbase/libutils/libandroidfw/libhwui/...).
Adapt to arm64 (OHOS clang, --target=aarch64, like the bridge) → deploy → bridge loads fully.

## Read
The whole runtime chain WORKS on the board: appspawn-x → VM → framework preload → bridge
in-process load (OHOS+VM syms resolved). The ONLY remaining gap is the AOSP native support
libs (androidfw/base/utils/binder for assets; hwui/skia for render), which need an arm64
build (recipe exists). ART TOLERATES the bridge clinit failure so appspawn-x stays up.
Next: arm64 build of the aosp_lib stack → bridge fully loads → app reg + aa start → UI.

## AOSP support-stack build (2026-07-08): SOURCE-VERSION gap found
Started adapting build_aosp_lib.sh → build_aosp_lib_arm64.sh (OHOS clang + aarch64 +
6.1 SDK sysroot; bridge-build-arm64/build_aosp_lib_arm64.sh, bld fn works). BLOCKER:
- bridge-build/aosp is HEADERS-ONLY (no .cpp for libbase/libutils/androidfw — those
  built on ECS). It's a NEWER AOSP (android-12/13: AssetManager2::SelectedValue ×10,
  EmptyAssetsProvider, separate AssetsProvider.h).
- Local aosp-android-11 HAS the .cpp sources but is OLDER (SelectedValue ×0, headers
  differ) → building libandroidfw from aosp-11 gives a DIFFERENT ABI than the bridge
  (compiled vs bridge-build/aosp headers) expects → symbols still won't match.
- aosp-art-15 / aosp-libcore-15 have no androidfw/libbase/libutils sources.

**To unblock**: targeted sync of the MATCHING AOSP (~android-13) sources for just
system/libbase + system/core/libutils + system/core/libcutils + frameworks/base/libs/
androidfw + frameworks/native/libs/binder (+ later frameworks/base/libs/hwui + skia),
then build_aosp_lib_arm64.sh compiles them (toolchain proven). Dep chain:
bionic_compat→log→base→cutils→utils→ziparchive→androidfw; binder separately.

## Net (deployment phase)
The runtime chain WORKS to the AOSP-support-lib boundary: appspawn-x→VM→framework
preload→in-process bridge load (all OHOS+VM syms resolve, libart NEEDED). The final
gap is the AOSP native support libs, which need their matching (android-13-era)
sources synced before the arm64 build (recipe + toolchain ready). That source sync is
the next prerequisite.
