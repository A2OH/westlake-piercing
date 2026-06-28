---
name: McDonald's BaseActivity.onCreate Progress
description: Tracing through BaseActivity.onCreate NPE chain. Layout infrastructure working, view tree rendering from real APK.
type: project
---

## BaseActivity.onCreate Progress (April 4 late evening)

### Fixed
- `InitializersEntryPoint.s()` → NPE → fixed by creating InitializationConfig/InitializationConfigRepository shim classes
- `clearV1OrderData()` at BaseActivity.onCreate:97 → `OrderModuleInteractor.resetCartInfo()` NPE
  - Root cause: `OrderModuleInteractor` is an ABSTRACT CLASS (0x0401), NOT an interface → Proxy.newProxyInstance can't work
  - 181 abstract methods, extends `McdModuleInteractor` (also abstract, 2 abstract methods)
  - `sun.misc.Unsafe.allocateInstance()` not available in this ART build (no JNI impl)
  - Workaround: `runPostCrashSetup()` calls remaining onCreate steps after NPE is caught
- Added ConstraintLayout shim class → LayoutInflater creates proper ConstraintLayout views
- Fixed SIGBUS crash from corrupt OP_IMAGE/OP_ARGB entries in display list
- Fixed sFqnClassMap to use real ConstraintLayout instead of FrameLayout fallback
- Removed demo screen override — renders actual app view tree

### Current State
- `setPageLayout()` succeeds → inflates `activity_base` with DrawerLayout, McDToolBarView, ConstraintLayout search UI
- `setPageView()` fails → NPE (null cause) — probably `page_content_holder` FrameLayout cast or missing view IDs
- Frame 1: Golden arches splash ✅
- Frame 2: White screen — real layout structure but:
  - McDToolBarView only 2px tall (dp→px conversion issue in layout params)
  - Content areas empty (no fragments)
  - Background white (theme/style colors not applied)

### Key Files Changed
- `WestlakeInstrumentation.java`: `runPostCrashSetup()` method, skip second recovery path
- `WestlakeActivityThread.java`: per-field try-catch, Unsafe.allocateInstance attempt for abstract classes
- `LayoutInflater.java`: ConstraintLayout mapping fix
- `WestlakeLauncher.java`: removed demo screen, renders dashboard view tree
- `ohbridge_stub.c`: display list corruption validation + truncation
- `shim/java/androidx/constraintlayout/widget/ConstraintLayout.java`: new shim class

### Next Steps
1. Fix `setPageView()` NPE — needs ConstraintLayout or page_content_holder cast fix
2. Fix toolbar height (dp→px conversion for layout dimensions)
3. Apply theme/style background colors to layout
4. Investigate fragment loading for content areas
5. Fix OP_ARGB corruption at ~44K bytes (ImageView with invalid bounds 924K×924K)
