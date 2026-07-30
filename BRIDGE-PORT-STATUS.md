# liboh_adapter_bridge.so arm64/OHOS-6.1 — 83/90 compiled + LINKED (2026-07-08)

**83/90 compile clean → LINKED** to a valid aarch64 .so (2.1MB, 150 JNI exports).
Build: `build_bridge_arm64.sh` (recipe bridge_incs_all.txt + overlay + shims).

## The breakthrough: correct AOSP tree
The "AOSP-version gap" was using the wrong AOSP. The adapter bundles its OWN newer
AOSP at **/home/dspfac/bridge-build/aosp** (585M — has AssetManager2::SelectedValue,
PointerCoords::isResampled, InputEventLookup, real JNIPlatformHelp.h + binder/OS.h,
math/, GLES/). Using it (AOSP=bridge-build/aosp) + the adapter's own
android-runtime/src on the include path fixed the whole asset/input/AndroidRuntime
cluster. 59→83.

## Fixes (all preserve arm32 via #if defined(__aarch64__) guards or per-file shims)
- 6.1 IPC override drift: app_scheduler_adapter.h (drop isShellCall) + session_stage_
  adapter.h (drop needUpdateViewport + 4 methods 6.1's ISessionStage lacks) — guarded.
- skia m133: SkOpts::hash→SkChecksum::Hash32 shim; SkTypeface::MakeFromStream→nullptr.
- boringssl PKCS7_verify→1 (NOVERIFY|NOSIGS anyway). ToolType, GLES stubs.
- correct AOSP tree + adapter android_runtime path first (AndroidRuntime collision).

## Remaining 7 — the AOSP/OHOS version-nuance tail (NONE block first render)
| file | issue | note |
|---|---|---|
| android_input_Input, android_view_MotionEvent | duplicate AOSP native input vs newer Input.h | **touch input — NOT needed for first render** |
| android_util_AssetManager | `GetResourceValue` nuance (l.649) | 1 method vs bundled-AOSP |
| window_callback_adapter | OccupiedAreaChangeInfo via OHOS 6.1 window zidl | window resize callback |
| oh_skia_ahb_shim | GLES/skia AHardwareBuffer types | graphics buffer shim |
| oh_bundle_mgr_client, apk_bundle_parser | extend_resource_manager path + BundleType::APP_ANDROID (6.1 enum lacks it) | bundle parse |

## Read
Bridge **92% compiled + LINKED** (150 JNI exports), core subsystems in — activity/
window/surface/binder/AMS/asset(mostly)/graphics. Remaining 7 are per-file version
nuances (2 are input, not needed to render). Next: deploy bridge+framework+appspawn-x
→ app registration (.app→.apk + bm install 6.1) → aa start → runtime integration.
