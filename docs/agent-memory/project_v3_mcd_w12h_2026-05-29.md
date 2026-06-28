---
name: v3-mcd-w12h-2026-05-29
description: "2026-05-29 — W12-H libart fix CLOSED the ContextThemeWrapper.getSystemService LinkageError (the 2026-05-28 handoff Path A wall, verified real on clean substrate via STEP 1). Root cause: W9 vtable shadow-routing matched by name+SHORTY, but shorty erases ref types so getSystemService(String) and getSystemService(Class) both shorty to 'LL' → W9 routed String overload onto Class slot → ValidateSuperClassDescriptors 'Parameter 0 type mismatch'. Fix: require full Signature::operator== in W9 match. McD now passes Activity.attach, runs its OWN SplashActivity.onCreate + setContentView. New wall W13 = AppCompat theme (activityInfo.theme=0x0). Substrate libart bb7a2f97, HW-clean."
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## What happened (2026-05-29, resuming handoff_2026-05-28_end)

Executed the handoff's STEP 1 + Path A. STEP 1 verified the `ContextThemeWrapper.getSystemService`
LinkageError is **real on clean substrate** (not a DiagHL artifact). Fixed it (W12-H). McD broke
through ~2-3 phases: now runs its own `SplashActivity.onCreate` → `setContentView`.

## Root cause + fix (W12-H)

- **Culprit pass: W9** (`[VTA-1-W9]` subclass-driven shadow-routing in `class_linker.cc` ~9251),
  NOT the VTABLE-A v1 pass (v1 patch was attempt v1 — did not fire).
- W9 matched candidate super slots by **name + shorty**. **Shorty erases reference types**:
  `getSystemService(String)` and `getSystemService(Class)` BOTH shorty to `"LL"`. W9 routed the
  String overload onto the Class overload's slot (129) → `ValidateSuperClassDescriptors` threw
  `Parameter 0 type mismatch: Class<String> vs Class<Class>`.
- **Fix:** in W9's match, after name+shorty, require **full `Signature::operator==`** (cross-dex
  by descriptor) before overwriting a slot. Same-sig cases (Kotlin create/invokeSuspend, W9's
  original purpose) unaffected.
- Diagnosis was by **reading the live child stderr** (found the `[VTA-1-W9] … getSystemService
  routed to slot 129` line), not theorizing — handoff HARD RULE held.

## Substrate (LIVE, verified)

- **libart.so `bb7a2f97`** (W12-H) — was `82429901` (W12-G). libart-only swap, NO boot regen (not in BCP).
- adapter-runtime-bcp `e0f01b23`, oh-adapter-framework `f80cf012`, adapter-mainline-stubs `38ff18ed`,
  framework `a7b6f91c`, core-oj `e923545e`, boot.oat `f7e83ad9`, appspawn-x `3abe3bde` — all unchanged.
- Pre-snapshot for rollback: `/data/local/tmp/pre-w12h/libart.so.82429901`.
- HW renders clean (onResume + first-frame, 0 aborts) on W12-H.

## New wall W13 — AppCompat theme NOT RESOLVED (diagnosed 2026-05-29; theme-ID is NOT the fix)

`AppCompatActivity.setContentView` → `IllegalStateException: You need to use a Theme.AppCompat
theme (or descendant)` at `SplashActivity.onCreate:119`. Initial theory (theme-id not propagated;
native `apk_manifest_parser.cpp` emits `appTheme=0x0`) was **DISPROVEN by diagnostic**:

Smali-patched the deployed `adapter-runtime-bcp.jar` (e0f01b23) to FORCE both
`appInfo.theme=0x7f16008b` AND `activityInfo.theme=0x7f16008b` (McD's real AppCompat theme),
+ 30-segment boot regen, deployed. **Result: both theme IDs confirmed set in logs, but the
AppCompat wall STILL fires** (same `setContentView`/`onCreate:119`). So **W13 is NOT theme-id
propagation** — it's that the theme **resource doesn't RESOLVE/APPLY**: `AppCompatDelegateImpl`'s
`obtainStyledAttributes(R.styleable.AppCompatTheme)` on the themed context finds nothing.

**Real W13 = Resources/AssetManager/theme-application subsystem (Tier C/D), NOT a quick adapter
field-set.** Next: confirm whether McD's `resources.arsc` + bundled androidx-appcompat resources
are loaded in the Activity's `AssetManager`, whether `Activity.setTheme()` is called by Westlake's
launch path, and whether ANY McD resource resolves. Related to the known "OH→Android resource ID
translation" gap (buildActivityInfoFromAbility zeroes iconId/labelId for the same reason).

**Diagnostic hardcode was rolled back** — substrate clean at libart `bb7a2f97` + adapter-runtime-bcp
`e0f01b23`. Full diag: `docs/engine/V3-DIAG-W13-THEME-2026-05-29.md`.

Toolchain win: reproducible smali-patching of deployed BCP jars now established — baksmali via
smali-baksmali-3.0.3 (+guava+jcommander), assemble via `brut.androlib.src.SmaliBuilder` from
`apktool.jar` (the standalone smali jars lack the assembler). Regression-safe vs the drifted
source tree (adapter-src is stale — pre-W11/W12; build source-of-record was ephemeral /tmp).

## Reusable lesson (candidate engine rule)

**Shorty is too coarse to distinguish reference-type overloads.** Any vtable pass that matches
methods by name+shorty (W9; possibly the A2-ABSTRACT pass too) will confuse `foo(String)` vs
`foo(Class)` vs `foo(Object)` (all shorty `LL`). Use full `Signature::operator==` when correctness
across overloads matters. Watch for the same latent bug in the A2-ABSTRACT pass for other
overloaded abstract methods.

## See also

- [[handoff-2026-05-28-end]] — the handoff this resumed (Path A / Path B fork; STEP 1 chose Path A)
- `docs/engine/V3-FIX-W12H-2026-05-29.md` — full fix doc + evidence
- [[r8-inlined-stacks-hide-root-cause-2026-05-28]] — same "read the real signal, don't theorize" discipline
