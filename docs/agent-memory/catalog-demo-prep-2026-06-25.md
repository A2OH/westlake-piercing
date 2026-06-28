# Catalog demo-prep (2026-06-25): libhwui + libart both had REGRESSED off the device → re-fixed; full L1-L7 sweep

Asked to demo-prep io.material.catalog (DAYU200) for "tomorrow": clean-boot → click-run → deep L1–L7
sweep + fix sluggishness (Task 2) + "hard to X/close" (Task 3), conservatively.

## ★ TWO PRIOR FIXES WERE NOT ON THE DEVICE (re-deployed both; both verified)
The device came up with the createHardwareBitmap fix REVERTED and the date-picker libart REGRESSED — a
reflash or perftrim swap had dropped them. **Always verify libhwui/libart md5 on-device before asserting
catalog state.**

1. **libhwui was clean `8b8f84ec` (NOT the createHardwareBitmap fix).** → 2nd-window demo Activities crash
   (Adaptive→"List View Demo"=AdaptiveListViewDemoActivity = "back to launcher", no tombstone — RenderThread
   SIGBUS). **FIX: redeploy libhwui `1d04a56e`** (AImageReader_* readback, the Codex-reviewed final). The
   DEPLOYED bridge `9b2a9727` ALREADY has the 13 `AImageReader_*` symbols (it's a superset: IME + AImageReader
   + tap/key), so NO bridge change. Brick-safe: `readelf --dyn-syms` UND diff vs 8b8f84ec = **0 new UND**
   (637=637). Source/staged: `$HOME/bridge-build/out/aosp_lib/libhwui.so` (==1d04a56e). VERIFIED: Inbox
   email-list Activity opens+renders, catalog alive, grid+Fragment demos unaffected.

2. **libart was the perftrim `ba40f173` which REGRESSES the Date Picker calendar.** Tapping "Launch Material
   Date Picker" → calendar dialog → CRASH (MaterialCalendarGridView, the deep super-vtable W9 case; hard crash,
   no stderr trace). The perftrim (memory: "gated fflush logging, catalog drew=1, 0 crash") only ever tested
   the GRID — it MISSED the date-picker regression. **`275eb104` (the verified W9 date-picker libart, also on
   device at /data/local/tmp/libart-275eb104.so) renders the calendar fine.** FIX: redeploy `275eb104`. Even
   though `ba40f173`'s source still has the W9 gate (`super_vtable_length > 100000`), it empirically crashes the
   date picker while `275eb104` doesn't — so for a demo, **use `275eb104` (correctness > the perftrim's modest,
   logging-only cold-start gain).** Measured: warm relaunch ~3.8s on BOTH; cold ~6.6s (275eb104) vs ~7.3s
   (ba40f173) = negligible. (Open question why ba40f173 differs functionally if only logging changed — didn't
   chase; 275eb104 is the safe known-good.)

## FINAL DEPLOYED CONFIG (persisted /system, verified boot cycle 4)
libhwui **1d04a56e** + libart **275eb104** + fontconfig **425290bd** (held) + bridge 9b2a9727 (unchanged).
Backups: `/data/local/tmp/demo-prep-bak/{libhwui.so.orig-8b8f84ec, libart.so.orig-ba40f173,
bridge.so.orig-9b2a9727, hm_symbol_config_next.json.orig-425290bd}` (each with md5 in filename).

## PERF (Task 2) — pre-warm is the lever, NOT AOT (AOT=dead end, per catalog-perf-jit-aot-findings)
- Cold launch via icon tap: **~6.6–7.4s** to grid (MUCH better than historical ~25–30s — fontconfig fix +
  lighter logging help). boot→completed ~38s. No catalog freeze across boots (fontconfig fix holds).
- Warm relaunch: **~3.8s** (very consistent 3.77–3.80s ×3). In-app nav L1→L2 ~1.5–2s, L2→L3 ~0.9s.
- **Recommended demo procedure: PRE-WARM** = after boot, icon-tap the catalog, let grid paint (~7s),
  `aa force-stop io.material.catalog`. Then the demo launch is the ~3.8s warm path (framework pages+JIT warm).

## L1–L7 SWEEP — works deeply at human pace
Launch = icon tap (`uinput -T -c 500 320`, the user's real path — WORKS, no MMI limit). In-app nav =
`echo "X Y" > /data/local/tmp/noice_tap` (the bridge tap channel; file "doesn't exist" via ls but the bridge
consumes it — it works). BACK = `uinput -K -d 2 -u 2` (OHOS keycode 2). Grid jpeg via snapshot_display ==
exactly 43668 bytes (handy launch detector; launcher ~75K, starting-window ~29K).
- L1 grid (~31 cats) ✓ scroll; L2 category pages ✓ (back-arrow top-left); L3 demo fragments ✓ (X + gear
  corners); L4 Dialog modal ✓ (action taps + dismiss), Date Picker calendar ✓ (day-select→title, month-nav,
  Cancel+snackbar), Bottom Sheet ✓ (drag collapse↔expand, in-sheet switch); L5 tab-switch/slider-drag/switch
  ✓ (all + snackbar). AdaptiveListViewDemoActivity (Inbox) opens (after libhwui fix). Evidence
  `docs/engine/V3-CATALOG-DEMO-PREP/` (README + ~20 jpegs).
- Text fields RENDER but typing doesn't work (soft keyboard summons then torn down ~65ms = the WMS-focus wall,
  catalog-ime-bridge-impl) — presenter tip: don't try to type.

## TASK 3 "hard to X/close"
- Fragment demos: closes 3 ways — tap X/back-arrow (small ~40px target at ~55,64 → TAP PRECISELY = the user's
  "hard to X"), OHOS BACK key, or settings→back. All verified.
- Separate-Activity demos (Adaptive sub-demos): NO on-screen back AND BACK key does NOT reach them. Cause: the
  bridge `dispatchKeyViaViewRoot` (`$HOME/bridge-build/src/framework/window/jni/oh_input_bridge.cpp`
  ~L411-428) picks the ViewRootImpl via `view.hasWindowFocus()`, but the sub-window Activity (WMS window type
  1001) isn't Android-focus-synced (WMS-focus wall) → BACK goes to the parent. **Did NOT apply the bridge fix
  (dispatch BACK to topmost ViewRootImpl) — too risky to rebuild the bridge the night before a demo.** Workaround:
  `aa force-stop io.material.catalog` + relaunch to escape, or just don't open Adaptive's sub-demos.

## STABILITY
At human pace (≥2–3s between taps) catalog is STABLE through deep nav. Machine-gun input DURING window
transitions crashes it (surface-teardown race, multi-window family) — reproduced ×2 with automated bursts,
NEVER at deliberate pace. Presenter tip: pause between taps (natural). Recovery from any crash =
`aa force-stop` + relaunch.

## ★ DEVICE STATE AT HANDOFF — USB hdc link DROPPED (not a brick)
During the final cold-boot validation the WSL↔USB hdc link dropped ("[Empty]" / "need connect-key"). Device
is ALIVE (UART COM3–8 "Ready"), usb.config=hdc_debug (NEVER touched), all fixes persisted on /system. This is
the known WSL↔USB transport flakiness (see catalog-perf-jit-aot-findings) — needs a PHYSICAL USB replug to
resume hdc (hdc kill/start + 3min poll did NOT recover it). NOT caused by any change I made.
