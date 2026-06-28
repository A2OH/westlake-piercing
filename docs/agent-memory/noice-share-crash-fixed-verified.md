---
name: noice-share-crash-fixed-verified
description: "noice \"Share with friends\" crash is FIXED + verified by real screen capture (5/5 taps survive); residual \"无法打开此文件\" is OHOS no-share-target, not a bug"
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

noice "与朋友分享 / Share with friends" (支持开发 / Support-Development page) **no longer crashes** — verified on device 2026-06-06 by REAL screen capture, not synthetic proof (see [[always-read-screen-final-validation]]).

**Verification:** navigated account-tab(control-channel `echo '5'`) → 支持开发(360,866) → tapped 与朋友分享 button (360,900) **5 times in a row**; noice pid 2122 **SURVIVED every tap** (5/5 OK), zero exceptions in the per-child stderr delta. Real capture `docs/engine/V3-NOICE-DPAD-FINDINGS/share-robust.jpeg` (+ supportdev.jpeg) shows the 支持开发 page + the graceful OHOS system dialog **"无法打开此文件 / 知道了"** (Unable to open this file / Got it) with noice alive underneath.

**The crash root cause + fix (deployed, in boot image):**
- noice share = `Intent.createChooser(ACTION_SEND)` → ACTION_CHOOSER intent (real ACTION_SEND nested as EXTRA_INTENT). Fix 1 = chooser-unwrap in `IntentWantConverter.intentToWant` (ohaf): if action==ACTION_CHOOSER, replace intent with its EXTRA_INTENT → ACTION_SEND maps to `ohos.want.action.sendData`.
- After unwrap, sendData has no OHOS handler on this board → `nativeStartAbility` returns NEGATIVE → AOSP `Instrumentation.checkStartActivityResult` would throw `ActivityNotFoundException` → crash. Fix 2 = negative-result clamp in `ActivityManagerAdapter.bridgeStartAbility` (ohaf): `if-gez` else `const/4 p1,0x0` (clamp to START_SUCCESS).
- Deployed: **ohaf `efd3f740`** (both fixes; boot vdex confirms `clamping to START_SUCCESS`). Bridge **`d2e50209`** (carries the scroll fix; unrelated to share).

**The residual "无法打开此文件" is NOT a bug:** it is OHOS correctly reporting that no app on this DAYU200 board can receive a text share (no messaging/social/browser app registered for `ohos.want.action.sendData` text/plain) — identical to a stock Android device with no share-target apps. On a real OHOS phone the unwrapped sendData would surface the OHOS share sheet with apps.

**Decision (NOT done):** a bridge-level intercept of `nativeStartAbility_impl` (activity_manager_adapter.cpp, in liboh_adapter_bridge.so — plain push, no boot regen) could copy the share text to the OHOS pasteboard (`-lpasteboard_client.z`, lib at `/system/lib/platformsdk/libpasteboard_client.z.so`) to make the share genuinely DO something. NOT implemented — risks the working stable bridge d2e50209, needs C++ JSON parse of parametersJson for `android.intent.extra.TEXT` + PasteData construction, and gives no clean on-screen feedback. Deferred per "stabilize / lottery not acceptable". Cross-ref [[noice-dpad-consumer-keystub]].
