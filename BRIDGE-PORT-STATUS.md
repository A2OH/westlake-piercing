# liboh_adapter_bridge.so arm64/OHOS-6.1 port — status (started 2026-07-08)

The bridge = 90 .cpp / ~37K LOC (input/surface/window/binder/AMS/audio JNI shims).
Triage compile against OHOS 6.1 SDK (arm64 sysroot) + synced 6.1 inner-API headers
+ AOSP-11 frameworks/base: **50 / 90 compile CLEAN**. Recipe: bridge_incs_all.txt
(include set) + OHOS 6.1 SDK clang, --target=aarch64-linux-ohos.

## Remaining = missing-header (23) + genuine 6.1 API drift (~12)

### Missing headers (need more repos synced / paths)
- `Sk*.h` (Skia — SkColorSpace/SkData/SkBmpDecoder, 6 files): sync third_party/skia
  (or graphic_2d's bundled skia) — the Typeface/graphics files.
- `values_bucket.h` (8): sync distributeddatamgr/relational_store.
- `third_party/zlib/zlib.h` (3): path prefix — add -Ithird_party root for zlib.
- singles: dynamic_cache.h, clone_param.h, app_quick_fix.h (ability_runtime subpaths),
  androidfw/ResourceTimer.h, android/hardware_buffer.h (NDK), binder/BpBinder.h (AOSP).

### Genuine 6.1 / AOSP API drift (per-file, the real porting work)
| file | issue | kind |
|---|---|---|
| apk_installer, oh_adapter_install_apk | `mode_t` undeclared | trivial: +#include <sys/stat.h> |
| android_util_XmlBlock/StringBlock | `LOG_ALWAYS_FATAL_IF` | trivial: android-base/logging include |
| window_manager_adapter, window_session_adapter | `PixelMap::Unmarshalling` gone | OHOS 6.1 Media drift |
| display_manager_adapter | `OHOS::Media::Size` gone | OHOS 6.1 Media drift |
| window_callback_adapter | `OccupiedAreaChangeInfo` | OHOS 6.1 window type |
| android_view_MotionEvent | `ToolType` unknown | MMI/AOSP input type |
| android_input_Input | `isResampled` | AOSP input field |
| android_input_InputEventLabels | `InputEventLookup` (was InputEventLabel) | AOSP rename |
| android_content_res_ApkAssets | `Guarded<unique_ptr>` ctor | AOSP template |

## Read
The bridge is NOT fundamentally broken on 6.1 — 55% compiles clean, the rest is a
bounded, catalogued set: a few more repo syncs (skia, relational_store) + ~12
specific symbol fixes (2 trivial, ~6 OHOS-6.1 Media/window drift, ~4 AOSP input).
Next: sync skia+relational_store, then patch the drift files one by one, then link.
Full fail list: bridge-fails.txt.
