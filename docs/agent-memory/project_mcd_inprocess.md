---
name: MCD in-process rendering via host app
description: Loading MCD APK resources in-process using createPackageContext. Real MCD layouts inflate with real Views. Next step is running MCD Activity lifecycle via am instrument.
type: project
---

## In-Process Approach (2026-04-10)

**Key insight**: Don't create a subprocess. Load MCD in the HOST APP process which already has full framework access.

### What Works
- `createPackageContext("com.mcdonalds.app", INCLUDE_CODE | IGNORE_SECURITY)` → real MCD context
- `packageManager.getResourcesForApplication("com.mcdonalds.app")` → full resources with splits
- `LayoutInflater.inflate(mcd_layout_id)` → real MCD Views (McDTextView, ImageView, etc.)
- `mcd_toolbar` layout: RelativeLayout + ImageView + McDTextView + McDAppCompatTextView
- `activity_splash_screen` layout: LinearLayout + RelativeLayout + ImageView + McDTextView
- Real MCD drawables: archus, splash_screen, ic_location, ic_drive_thru, back_chevron, close
- All rendered on screen via host Activity's real Window

### Blocker
- MCD Activity.onCreate() fails: Hilt/AppCompat superclass chain needs full Activity.attach()
- Just `attachBaseContext()` isn't enough — ComponentActivity expects internal fields set by attach()
- `am instrument` approach DID get `Activity.attach() OK!` + real PhoneWindow
- But instrumentation process exits after finish(), killing the Activity

### Next Steps
1. Make `am instrument` keep the Activity alive (don't call `finish()`)
2. Or: bypass Hilt — instantiate MCD Fragments directly (they don't need the full Activity chain)
3. Or: use VirtualDisplay to capture the real MCD app's rendering

### Key Files
- `McdInProcessActivity.kt` — host Activity that loads MCD resources in-process
- `WestlakeBridgeInstrumentation.kt` — instrumentation that runs in registered process
- `ohbridge_stub.c` → `libframework_stubs.so` — native stubs for subprocess path

### Architecture
```
Host App (com.westlake.host)
  → createPackageContext("com.mcdonalds.app")
    → MCD resources + classloader
    → LayoutInflater.inflate(mcd_layout) → real Views
    → setContentView() → on screen
```
No subprocess. No stubs. Phone's real framework handles everything.
