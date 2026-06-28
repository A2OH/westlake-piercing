---
name: v3-direction-hbc-runtime-reuse-westlake-app-hosting-engine
description: 2026-05-16 strategic commitment to V3 ([build-host]-runtime + Westlake app-hosting engine) for the OHOS path. Supersedes V2-OHOS direction (yesterday). V2 Android-phone path UNCHANGED. Pointer doc; full architecture in docs/engine/V3-ARCHITECTURE.md.
metadata: 
  node_type: memory
  type: project
  date: 2026-05-16
  supersedes: project_v2_ohos_direction.md
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Decision

**2026-05-16:** Westlake Phase 2 OHOS pivots from full V2-substrate port
to **V3 ([build-host]-runtime reuse)**. Westlake stops owning a Java substrate /
dalvikvm / libhwui / boot image for the OHOS path; reuses [build-host]'s existing
runtime substrate (AOSP-14 ART cross-built to OHOS musl + real
`framework.jar` + `appspawn-x` + adapter framework) and refocuses
Westlake-owned engineering on the **app-hosting engine** that lives on
top of it.

The V2 Phase-1 Android-phone path is **UNCHANGED** by V3.

## Why (TL;DR)

CR-EE / CR-FF audits of [build-host]'s adjacent work tree (`~/adapter/`)
identified [build-host] has independently solved OHOS Android-app hosting at
multi-engineer-month cost. [build-host]'s HelloWorld reaches `MainActivity.onCreate`
line 83 today — significantly past Westlake's V2-OHOS milestone (which
was stuck at launcher-white panel pixel per CR67 / CR-AA after the CR-W
→ CR-X → CR-Y → CR-Z chain). Reusing [build-host]'s artifacts is the rational
move: same company, same product effort, no IP concern.

The `feedback_additive_shim_vs_architectural_pivot.md` rule fired
(2nd time): the layer itself was wrong, not the next shim. V3 commits to
reusing the layer [build-host] has paid the cost on.

## Authoritative docs (read in this order)

1. **`docs/engine/V3-ARCHITECTURE.md`** — V3 layer stack + ownership
   table + what V3 deletes / keeps / borrows
2. **`docs/engine/V3-WORKSTREAMS.md`** — W1-W13 with acceptance criteria
3. **`docs/engine/V3-SUPERVISION-PLAN.md`** — dispatch order + DAG + risks
4. **`docs/engine/CR61_1_AMENDMENT_LIBIPC_VIA_HBC.md`** — CR61 amendment
   for V3 path (libipc/samgr permitted via [build-host] adapter)
5. **`docs/engine/CR-EE-HANBINGCHEN-ARCHITECTURE-ANALYSIS.md`** — [build-host]
   stack structural overview
6. **`docs/engine/CR-FF-[build-host]-BORROWABLE-PATTERNS.md`** — tactical patterns

## Workstreams W1-W13 (issues #626-#638 in `A2OH/westlake`)

| W | Title | Issue | Effort (PD) | Depends on |
|---|---|---|---|---|
| W1 | [build-host] artifact inventory + pull | #626 | 2-3 | — |
| W2 | Boot [build-host] runtime standalone on DAYU200 | #627 | 3-5 | W1 |
| W3 | Replace OhosMvpLauncher with appspawn-x | #628 | 3-4 | W1, W2 |
| W4 | Adapter customization for Westlake scope | #629 | 5-8 | W2, W3 |
| W5 | Mock APK validation | #630 | 2-3 | W3 |
| W6 | noice on OHOS via V3 | #631 | 5-8 | W4, W5 |
| W7 | McD on OHOS via V3 | #632 | 4-6 | W4, W5, W6 |
| W8 | SceneBoard bring-up | #633 | 5-10 | — |
| W9 | Borrow [build-host] Tier-1 patterns | #634 | 2-3 | W1 |
| W10 | Memory + handoff refresh | #635 | 1 | W1 |
| W11 | V2 carryforward audit | #636 | 1-2 | — |
| W12 | CR61.1 amendment downstream disposition | #637 | 1 | — |
| W13 | Archive V2-OHOS substrate | #638 | 1-2 | W1, W3 |

**Critical path:** W1 → W2 → W3 → W4 → W6 → W7 ≈ 22-34 PD. **Total:**
35-55 PD across W1-W13. Wall-clock 4-6 weeks well-coordinated, 6-8 weeks
conservative.

## V3 layer stack (one-paragraph)

App (unmodified APK) → **Westlake app-hosting engine** (intent rewriting,
lifecycle, multi-app coordination, per-app constants table) → **[build-host]
runtime substrate** (AOSP-14 ART + libhwui + Skia RTTI shim +
libbionic_compat + real framework.jar + 4 L5 patches + appspawn-x + 5
forward bridges + 6 reverse bridges + OHEnvironment dual-classloader) →
**OHOS platform** (libipc_core, samgr_proxy, render_service,
composer_host, legacy WMS) → **Hardware** (DAYU200 rk3568, 32-bit ARM).

## What transfers from V2 (per W11)

Per `V3-ARCHITECTURE.md` §6.5 and `V3-V2-CARRYFORWARD-AUDIT.md` (W11):

- **PHONE-ONLY** (V2 Android-phone path stays): `westlake-host-gradle`,
  V2 substrate Java, `aosp-libbinder-port/out/{musl,bionic}/`, M5 + M6
  daemons for Android phone, all V2 phone launchers + regression suite
- **V3-INHERITS-VERBATIM** (concept reuse): CR59 lifecycle drive (apply at
  [build-host]'s `AbilitySchedulerBridge` Handler.post seam), CR60 bitness
  discipline (32-bit ARM on DAYU200), macro-shim contract (narrower scope),
  no-per-app-branches rule, fail-loud pattern, subtraction-not-addition,
  5-pillar pattern conceptual lessons
- **V3-OBSOLETED** (replaced by [build-host]): aosp-shim-ohos.dex, SoftwareCanvas,
  drm_inproc_bridge.c, OhosMvpLauncher, dalvik-port build-ohos-* targets,
  M5/M6 OHOS-target daemons, aosp-libbinder-port arm32-OHOS-target

## What V3 deletes from Westlake's OHOS path (per V3-ARCHITECTURE §4)

~70K+ LOC of substrate work + ~16K LOC of daemon work archived (not
deleted). The work isn't wasted — it was the learning that led to the
architectural pivot. But it doesn't ship under V3. See per-component
disposition in `docs/engine/V3-W12-CR61.1-CODE-DISPOSITION.md`.

## CR61 amendment (CR61.1)

Per `docs/engine/CR61_1_AMENDMENT_LIBIPC_VIA_HBC.md`: under V3, platform-
level adapters MAY link `libipc_core` and `libsamgr_proxy` via their
**innerAPI variants** ([build-host]'s `liboh_adapter_bridge.so` already does this).
Westlake-owned code never `dlopen`s libipc/libsamgr directly. The Android-
phone V2 path is unaffected — CR61 stands there.

## Macro-shim contract — V3 scope (narrower)

V2 scope: Westlake-owned classes covering nearly the entire AOSP framework
surface. **V3 scope:** Westlake-owned classes covering ONLY the app-hosting
engine surface. Real `framework.jar` from [build-host] means we don't own (and must
not shim) any framework class. The contract applies at the **integration
seam** between our engine and [build-host]'s runtime.

**Forbidden under V3:** Unsafe.allocateInstance on framework classes,
setAccessible on framework internals, per-app branches, modifying [build-host]
adapter sources, catching NoSuchMethodError/LinkageError from framework
class reflection.

**Permitted under V3:** API methods on Westlake-owned engine classes with
(a) AOSP-default verbatim, (b) safe-primitive, or (c) delegation bodies.

## Status (2026-05-16, end of day)

- V3 docs landed (commit `073059c2`)
- 13 GitHub issues opened (#626-#638)
- W1 ([build-host] artifact pull) in flight, dedicated agent
- W10 (this doc + handoff_2026-05-16.md + MEMORY.md refresh) in flight
- W11 (V2 carryforward audit) in flight by agent 44
- W12 (CR61.1 downstream code disposition plan) in flight by this agent
- W8 (SceneBoard board-config decision) can run independently from day 1

## Status update (2026-05-16, late — W2 soft-brick)

- **W2 (boot [build-host] runtime standalone on DAYU200) — BLOCKED.** Agent 49
  attempted full Stage 0-3 deploy per
  `westlake-deploy-ohos/v3-hbc/scripts/DEPLOY_SOP.md`. Stages 0/1/3 PASS;
  Stage 3.5/4 NOT REACHED — `hdc shell` went silent post-Stage-3 while
  `hdc file recv` continued working; subsequent `hdc target boot` left
  DAYU200 non-enumerated over USB. Device has been offline >10min;
  awaiting operator hard power-cycle (DAYU200 has no remote reset).
  Recovery plan: 13 device-side `.orig_20260516` backups + bootloader
  intact, so factory recoverable on next contact.
- **W2 checkpoint:** `docs/engine/V3-W2-BOOT-[build-host]-RUNTIME-REPORT.md`
  (commit `f25412f8`).
- **W2 postmortem:** `docs/engine/V3-W2-POSTMORTEM.md` (this agent 52).
  Top-2 hypotheses: H1 SELinux respawn storm from silent-chcon, H2
  Windows `hdc.exe` stdout-channel regression. SOP gaps G1-G7 +
  recovery procedure R0-R5 documented.
- **Memory lesson:** `feedback_soft_brick_w2_2026-05-16.md` — probe
  `hdc shell` echo-sentinel BETWEEN every Stage; never `|| true` a
  chcon; never silent-SKIP a required artifact; run new SOPs in
  increments first time. Adds rule 5: a borrowed SOP encodes the
  author's mid-bringup board state, not a clean board — add a
  fresh-bringup gate before adopting verbatim.
- **Downstream impact:** W3 (appspawn-x integration), W4 (adapter
  customization), W5 (mock APK), W6 (noice), W7 (McD) all gate on W2
  PASS. Critical path delayed by however long the operator power-cycle
  + W2 retry takes. NOT pivot-evidence per
  `feedback_two_pivots_in_two_days.md` — this is normal W-level
  engineering rework.

## Status update (2026-05-19 — chroot containment is the Phase 1 deploy model)

V3 direction ([build-host]-runtime reuse) is **UNCHANGED at the substrate /
architectural level**. The deploy-layer model evolved in response to
W2 soft-brick: Phase 1 is now **brick-impossible chroot containment**
under `/data/local/tmp/v3-hbc-chroot/`; Phase 2 (/system writes) is
deferred until Phase 1b/1c lands engine evidence.

- **W2 Phase 1a substrate validation PASSED** on factory-clean DAYU200
  (commit `0c9e7532`). 5 mounts (3 RW: proc/sys/dev; 2 RO: lib/system/lib).
  SELinux Enforcing throughout. `[v3-chroot-launch]` marker emitted.
  See [[project_v3_chroot_phase1a_validated]].
- **W2 Phase 1b/1c (engine evidence) NOT YET COMPLETE.** Phase 1a
  validates substrate ONLY; reaching `MainActivity.onCreate` from chroot
  needs Phase 1b ($V3_CHROOT_HELLO_CMD recipe; ½-day C work) or Phase 1c
  ([build-host] AMS replacement; ½-week+).
- **hdc 3.2.0b CLEARED** (commit `f158cf58`) — H2 hypothesis from W2
  postmortem DIES. No `KNOWN_GOOD_HDC_VERSIONS` pin needed.
- **W4 split** into W4-empty (pre-spike) + W4-engine (conditional);
  new W6-prep, W6-perf, W7-prereq workstreams (commit `77c9540e`).
- **3 anti-patterns surfaced and bounded** during chroot bring-up:
  hdc_shell silent-NOOP via host-exit-code laundering (now
  [[feedback_hdc_shell_check_pattern]]), per-binary ldd walking
  (replaced by RO bind-mount per [[feedback_chroot_dynamic_elf_ro_bind]]),
  `|| true` on chcon/setup steps (already bounded by
  [[feedback_soft_brick_w2_2026-05-16]] rule 2).
- **Chroot containment proposal** (commit `53a78196`,
  `docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md`, 805 LOC) is the
  authoritative Phase 1 deploy model. ~91% of [build-host] bundle is
  TRIVIAL_RELOCATE.
- **Westlake-Island borrow map** (commit `9705487c`,
  `docs/engine/WESTLAKE-ISLAND-BORROW-MAP.md`, 628 LOC) — stay V3 +
  borrow 5 Island operational patterns. Coverage gap: camera2 / location
  / keystore have zero runtime evidence across [build-host]/Island/03-Req
  (tracked as new W7-prereq).
- **03-Requirement corpus indexed** (commit `caa3fd56`,
  `docs/engine/03-REQUIREMENT-INDEX.md`, 445 LOC) — 50 packages all
  DISCOVERY_REQUIRED; only `wifi` V7-Pilot-Ready.
- **Pivot discipline held** — 3 retries in different layers, all in
  brick-safe domain, is iteration not symptom-rotation per
  [[feedback_two_pivots_in_two_days]]. No third pivot.

## Cross-references

- `docs/engine/V3-ARCHITECTURE.md` — authoritative architecture
- `docs/engine/V3-WORKSTREAMS.md` — W1-W13
- `docs/engine/V3-SUPERVISION-PLAN.md` — dispatch + DAG + risks
- `docs/engine/CR61_1_AMENDMENT_LIBIPC_VIA_HBC.md` — CR61 amendment
- `docs/engine/CR-DD-CANDIDATE-C-VS-V2-OHOS-RECONSIDERED.md` — pivot trigger
- `docs/engine/CR-EE-HANBINGCHEN-ARCHITECTURE-ANALYSIS.md` — [build-host] overview
- `docs/engine/CR-FF-[build-host]-BORROWABLE-PATTERNS.md` — tactical patterns
- `docs/engine/V3-W12-CR61.1-CODE-DISPOSITION.md` — per-component archive
  plan (this agent's W12 deliverable)
- `docs/engine/V3-V2-CARRYFORWARD-AUDIT.md` — V2-to-V3 disposition table
  (W11 deliverable by agent 44)
- `handoff_2026-05-19.md` — current V3 5-minute orientation (NEW)
- `handoff_2026-05-16.md` — predecessor; W2 soft-brick era (superseded)
- `project_v3_chroot_phase1a_validated.md` — Phase 1a milestone (NEW)
- `feedback_hdc_shell_check_pattern.md` — control-flow safety (NEW)
- `feedback_chroot_dynamic_elf_ro_bind.md` — Stage-3 lib bind (NEW)
- `feedback_soft_brick_w2_2026-05-16.md` — origin postmortem; bounded
  by 2026-05-19 follow-up rules
- `feedback_two_pivots_in_two_days.md` — reflective discipline note
- `feedback_additive_shim_vs_architectural_pivot.md` — rule that fired
- `project_v2_ohos_direction.md` — SUPERSEDED by V3 (kept verbatim)
- `project_binder_pivot.md` — V2 binder pivot; Phase-1 Android-phone path
  still authoritative; OHOS path SUPERSEDED by V3
