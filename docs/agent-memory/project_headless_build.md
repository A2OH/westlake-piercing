---
name: headless_build_fixes
description: Fixes required to build qemu-arm-linux-headless product (standard system for QEMU) — 98% complete, foundation running with ability framework
type: project
---

Building `qemu-arm-linux-headless` (standard system, ARM32) requires 70+ patches to the OpenHarmony source tree.

**Why:** The headless config disables graphics/UI features, but many BUILD.gn files and C++ sources assume graphics is always enabled.

**How to apply:** These patches must be re-applied after any `repo forall -c 'git checkout -- .'` or repo sync.

## Current Status (2026-03-15)
- Build: **98% (4757/4851 targets)**
- **Foundation running** with SA 180 (ability_manager), 401 (bundle_manager), 501 (app_manager)
- **libabilityms.z.so built** — ability lifecycle management
- **466 shared libraries** on the system
- 1 target fails: `libui_extension.z.so` (not needed for headless)
- 70+ source files patched across ability_runtime, graphic_2d, window_manager, arkcompiler

## Key Patch Categories

### GN Build System (321+ files)
- `defines = []` clobbering: 837 redundant resets removed
- Missing `defines = []` initialization in config blocks
- Conditional deps moved outside `ability_runtime_graphics` guard for `libdm`/`libwm`

### EGL/GL/HDI Stubs (sysroot)
- EGL/GLES stub headers in `out/qemu-arm-linux/obj/third_party/musl/usr/include/arm-linux-ohos/`
- HDI display headers (v1_0, v1_1) generated from IDL sources
- RenderContext, CmdList, GPUContext stubs in render_service_base

### Skia GPU Guards (10 files in render_service_base)
- All GPU calls wrapped with `#ifdef RS_ENABLE_GL`
- LTO vtable fix: `RSB_EXPORT` on template specialization, LTO disabled for render_service_client

### ability_runtime SUPPORT_GRAPHICS (30+ files)
- ability_context_impl, dialog_ui_extension_callback, ability_manager_service
- mission_data_storage (stubbed), mission_list_manager, mission_info_mgr
- js_ability_context, js_ui_ability, ui_ability, extension_ability_thread
- napi_context, napi_atmanager, idle_time, js_runtime, js_worker
- ability_manager_client (PrepareTerminateAbility stubbed)

### Window Manager Guards (5 files)
- window_transition_info, scene_session_manager, session_listener_controller
- picture_in_picture_controller

### Ruby 3.4 Compatibility
- `require 'ostruct'`, `require 'stringio'`
- `File.exists?` → `File.exist?`

## Foundation Configuration
Place at `system/profile/foundation.json`:
```json
{
    "process": "foundation",
    "systemability": [
        { "name": 180, "libpath": "libabilityms.z.so", "run-on-create": true, "distributed": true },
        { "name": 401, "libpath": "libbms.z.so", "run-on-create": true, "distributed": true },
        { "name": 501, "libpath": "libappms.z.so", "run-on-create": true }
    ]
}
```
