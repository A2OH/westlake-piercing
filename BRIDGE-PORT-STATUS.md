# liboh_adapter_bridge.so arm64/OHOS-6.1 port — status (2026-07-08)

Bridge = 90 .cpp / ~37K LOC. Compile vs OHOS 6.1 SDK arm64 + synced 6.1 inner-API
headers + AOSP-11 frameworks/base: **59 / 90 CLEAN** (was 50; +skia+relational_store+
image_framework repos, skcms symlink, corrected includes). Recipe: bridge_incs_all.txt
+ CF flags: -include signal.h/sys/stat.h/log/log.h -Wno-narrowing -Wno-c++11-narrowing.

## Remaining 31 = header-path (16) + genuine drift (15)

### Header-path (16) — add paths / create symlinks (mechanical)
- impl_interface/{data_impl,matrix44_impl}.h (4): graphic_2d 2d_graphics impl_interface subpath.
- sync_fence.h (2): graphic_surface — files include "sync_fence.h" bare; add its exact dir.
- third_party/zlib/zlib.h (3): add a root where third_party/zlib/ resolves (skia has a copy, or symlink).
- android/hardware_buffer.h (1): NDK header (SDK sysroot).
- ResourceTimer.h, app_quick_fix.h, BpBinder.h, clone_param.h, dynamic_cache.h (5): OHOS/AOSP subpaths.

### Genuine 6.1 / AOSP API drift (15) — per-file source patches (overlay)
| file(s) | issue | fix direction |
|---|---|---|
| activity_manager_adapter, adapter_bridge×2, app_scheduler_adapter, oh_callback_handler, oh_environment | `override hides virtual` | **6.1 IPC interface signature drift** — update the override method sig to match the 6.1 inner_api interface (e.g. IAppScheduler::ScheduleMemoryLevel). The core drift set. |
| android_util_StringBlock/XmlBlock | `registerNativeMethods` missing in AndroidRuntime | adapter AndroidRuntime API — use the right registration call |
| oh_typeface_init | `SkTypeface::MakeFromStream` gone | skia m133 API — use SkFontMgr::makeFromStream |
| android_view_MotionEvent | `ToolType` unknown | AOSP input enum — qualify/def |
| android_input_InputEventLabels | `InputEventLookup` (was InputEventLabel) | AOSP rename — adapt source |
| android_content_res_ApkAssets | `Guarded<unique_ptr>` ctor | AOSP template |
| android_view_KeyCharacterMap, android_input_Input | redefinition (AndroidRuntime/clear) | header double-include guard |
| window_callback_adapter | `OccupiedAreaChangeInfo` | +#include occupied_area_change_info.h |

## Read
Bridge is ~66% clean, remaining fully catalogued. The 6 `override hides virtual`
files are the real 6.1 IPC drift (interface sigs changed); the rest are paths +
mechanical AOSP/skia adaptations. Next: finish header paths → patch the 15 drift
files (overlay) → link liboh_adapter_bridge.so → app registration → aa start → UI.
