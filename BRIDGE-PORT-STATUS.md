# liboh_adapter_bridge.so arm64/OHOS-6.1 — 74/90 compiled + LINKED (2026-07-08)

Bridge = 90 .cpp. **74/90 compile clean → LINKED** to a valid aarch64 .so (1.69MB,
87 JNI exports). Recipe: bridge_incs_all.txt + overlay (bridge-overlay/{inc,cpp}) +
shims (bridge-stubinc/). CF: -include signal.h sys/stat.h log/log.h pkcs7_stub.h
aosp_bridge_compat.h; per-skia-file +skopts_hash_compat.h; -Wno-narrowing.

## Fixes applied (70→74 + the earlier 50→70)
- 6.1 IPC override drift: app_scheduler_adapter.h (drop isShellCall) fixed 7 files;
  session_stage_adapter.h (drop needUpdateViewport) — partial.
- skia m133: SkOpts::hash → SkChecksum::Hash32 shim (skopts_hash_compat.h, 4 render
  files); SkTypeface::MakeFromStream → nullptr stub (oh_typeface_init; TODO SkFontMgr).
- boringssl: PKCS7_verify → macro=1 (flags were NOVERIFY|NOSIGS; pkcs7_stub.h).
- ToolType → int32_t (aosp_bridge_compat.h). + many header paths + stubs (OS.h,
  ResourceTimer.h, zlib prefix).

## Remaining 16 = AOSP-version gap (asset/input/bundle subsystem) + few overrides/paths
The adapter's *_aosp.cpp files use NEWER-AOSP APIs than aosp-android-11 provides:
| file | issue | note |
|---|---|---|
| android_content_res_ApkAssets | `Guarded<>` ctor | newer androidfw |
| android_util_AssetManager | `AssetManager2::SelectedValue` | newer androidfw |
| android_util_StringBlock/XmlBlock | `AndroidRuntime::registerNativeMethods` | adapter API |
| android_view_MotionEvent | `PointerCoords::isResampled` | newer AOSP input |
| android_view_KeyCharacterMap, android_input_Input | redefinition (AndroidRuntime/clear) | header/aosp clash |
| android_input_InputEventLabels | `InputEventLookup` (hdr declares InputEventLabel) | hdr/src mismatch |
| Parcel_aosp_native | `binder/OS.h` | newer AOSP binder |
| oh_bundle_mgr_client, apk_bundle_parser, oh_app_mgr_client | clone_param/app_quick_fix path + override | bundle |
| window_callback_adapter, oh_window_manager_client, session_stage_adapter | OccupiedAreaChangeInfo namespace + override | window session |
| oh_egl_surface_adapter, oh_skia_ahb_shim | GLES/EGL headers | graphics NDK |

## Read
Bridge 82% compiled + LINKED. Remaining 16 are a bounded AOSP-version-adaptation
sub-effort (the *_aosp.cpp asset/input files target a newer AOSP than aosp-11) +
a couple window-session overrides + graphics-NDK header setup. The linked .so has
the core (activity/window/surface/binder/AMS) — the asset/input parsing + some
graphics files are the gap. Next: adapt the *_aosp.cpp to aosp-11 (or sync newer
androidfw) → full link → deploy → app registration → aa start → UI.
