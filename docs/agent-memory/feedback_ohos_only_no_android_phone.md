---
name: ohos-only-no-android-phone-from-2026-05-19
description: "All future Westlake development is OHOS-only. No Android-phone dev (no OnePlus 6 cfb7c9e3, no Pixel, no V2 Android-phone iterations). Treat V2-Phone code as frozen baseline."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

From **2026-05-19 onward**, all Westlake development targets **OHOS only**. Stop all Android-phone development:

- No new builds, deploys, or runs against OnePlus 6 (`cfb7c9e3`)
- No new builds against Pixel 7 Pro (`2B151FDH3006QW`)
- No new V2 Android-phone CRs, milestones (M5-M13), or substrate work
- No "validate on Android first" regression sanity proposals
- No "let's check noice/McD still works on cfb7c9e3" as a quick win

The OnePlus 6 in-process Option 3 success (noice + McD both rendering in `com.westlake.host`, regression 14/14 PASS as of 2026-05-14, documented in [[project_noice_inprocess_breakthrough]]) is **preserved as historical proof** that the engine layer architecture works — but the development substrate is now OHOS DAYU200, not Android.

## Why

User decision on 2026-05-19, immediately after V3 W2 Phase 1a chroot substrate PASS ([[project_v3_chroot_phase1a_validated]], commit `0c9e7532`). Context: orchestrator proposed reverting to Android-phone path as a "regression sanity check" before pivoting V3 to `/system` hardened deploy. User declined and made the directional call: Android-phone is over, OHOS is the only target.

Product context: Westlake's end goal is "Android apps as first-class OHOS citizens managed by WindowManager alongside ArkTS apps." Android-phone work was always validation-on-the-easy-path, never the product. Phase 1a chroot substrate PASS on real DAYU200 hardware crosses the threshold where Android-phone validation no longer pays back its time cost.

## How to apply

**When proposing next work:**
- Filter all options through "does this advance OHOS first?" Discard Android-phone-first options.
- If something can ONLY be demonstrated on Android-phone today (e.g., engine-layer behavior that depends on factory Android system_server), surface that as a CONSTRAINT to design around, not as a path to take.
- Do NOT suggest "let's quickly run X on cfb7c9e3 to validate" as a sanity check. The sanity check goes on OHOS.

**When triaging stale tasks:**
- V2-Phone milestones #110-#118 (M5-M13), CR44, CR50+51, and similar V2-Android-only items are now permanently deferred. Mark as deleted or superseded rather than leaving them as `pending` (visual noise).

**When reading V2-Phone code:**
- `westlake-host-gradle/`, `aosp-libbinder-port/`, `art-latest/` etc. — treat as FROZEN baseline. Read for reference; do not modify for V2-Phone reasons.
- If V3 work needs to borrow a pattern from V2-Phone code, copy/adapt INTO the OHOS path. Don't go back to evolve the Android-phone code.

**When tracking dev infrastructure:**
- `/mnt/c/Users/<user>/Dev/platform-tools/adb.exe` (Android adb) — no longer the primary dev tool. Don't reach for it for "convenience."
- `/mnt/c/Users/<user>/Dev/ohos-tools/hdc.exe` (OHOS hdc, version 3.2.0b probe-validated per [[project_v3_chroot_phase1a_validated]]) — this is the dev bridge from now on.

**When something genuinely needs Android-phone today** (e.g., upstream investigation, debugging an AOSP behavior we want to mirror) — flag it explicitly to the user as an exception request, do not just do it.

## Cross-references

- [[project_v3_chroot_phase1a_validated]] — the milestone that crossed the threshold
- [[project_v3_hbc_reuse_direction]] — V3 architecture ([build-host]-reuse on OHOS)
- [[handoff_2026-05-19]] — current state at decision time
- [[project_noice_inprocess_breakthrough]] — historical Android-phone success, preserved
- [[project_binder_pivot]] — V2 Phase-1 Android-phone path, now frozen
- [[feedback_two_pivots_in_two_days]] — note: this directional call is a SCOPE narrowing (Android-phone → OHOS-only), not an architectural pivot; pivot-discipline bar does NOT apply
