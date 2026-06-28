---
name: bcp-first-jar-wins-2026-05-25
description: "Engine invariant — BCP uses first-jar-wins class resolution at dex2oat time. When adding a class to one BCP jar to SHADOW a duplicate in another BCP jar, the shadowing jar must come EARLIER in the BCP list. appspawn-x's kBootClasspath and gen_boot_image.sh must use the same order or boot image crashes with runtime.cc:699 class mismatch."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**BCP uses first-jar-wins class resolution at dex2oat time.** When a class exists in multiple BCP jars, the FIRST jar in BCP order wins.

When adding a class to BCP jar A to shadow a duplicate in BCP jar B:
- Jar A must come BEFORE jar B in BCP order
- Both `appspawn-x` runtime `kBootClasspath` AND build-time `gen_boot_image.sh` jar list MUST use the same order
- A mismatch produces a loadable but layout-mismatched boot image → `runtime.cc:699 ClassLinker::CheckSystemClass: Class mismatch for L<class>;` abort

## Why this matters

**Why:** When shadowing across BCP jars (e.g., relocating `PackageInfoBuilder` from `oh-adapter-framework.jar` to `adapter-runtime-bcp.jar` via Scope C), the relocation only takes effect if BCP order puts the new home FIRST.

**How to apply:** Whenever you add OR move a class in BCP:
1. Identify which BCP jar should be authoritative for that class
2. Confirm runtime BCP order in `framework/appspawn-x/src/main.cpp::kBootClasspath`
3. Confirm build-time order in `build/inner/gen_boot_image.sh` JARS variable
4. If they disagree, edit BOTH to match — typically put your authoritative jar EARLIER in both lists
5. Regen boot image with matching order

## Concrete example — Scope C BCP reorder (2026-05-25)

Scope C moved `PackageInfoBuilder` from `oh-adapter-framework.jar` to `adapter-runtime-bcp.jar`. To make the new PIB authoritative:
- Original Scope B BCP order: `...framework, oh-adapter-framework, adapter-runtime-bcp` (Scope B's classes were unique, so order didn't matter)
- Scope C required: `...framework, adapter-runtime-bcp, oh-adapter-framework` (new PIB must shadow old PIB)
- Patched in:
  - `framework/appspawn-x/src/main.cpp` (~3 lines) — runtime BCP order
  - `build/inner/gen_boot_image.sh` (~1 line in JARS variable) — build BCP order

Result: new appspawn-x md5 `3abe3bde17b53a021d3078f070a7f7bd`. Boot image regenerated with correct order. PIB-TIER2 marker fires on device (proves shadowing works).

## Anti-example — Tier 3 local-build failure (2026-05-25 late)

Pass 2 Tier 3 attempt built boot image LOCALLY using `gen_boot_image.sh` which still had Scope C's correct BCP order. But the local agent used a different invocation path that defaulted to OLD order (`...framework, oh-adapter-framework, adapter-runtime-bcp`). Built jar deployed → appspawn-x crashed with `runtime.cc:699 Class mismatch for Ljava/lang/String;` → full rollback.

Root cause: BCP order in build invocation didn't match runtime kBootClasspath.

## Future agent checklist

Before any BCP jar build or boot image regen:
- [ ] Read current `appspawn-x` `kBootClasspath` from source (or extract from running binary)
- [ ] Confirm build script JARS variable matches
- [ ] If shadowing a class: ensure new jar comes BEFORE old jar in both
- [ ] After build, verify class layout matches by deploying to a TEST device first (or compare boot.oat structure to baseline)

## Risk mitigation

`runtime.cc:699` aborts give a clear error message (`Class mismatch for L<class>;`) but only AFTER appspawn-x has tried to load. By then the dalvik-cache may be polluted. Always:
1. Snapshot ALL 30 boot image segments before deploy (not just the few you're changing — cross-jar checksums change too)
2. Pre-snapshot dalvik-cache dir: `cp -r /data/misc/appspawnx/dalvik-cache /data/local/tmp/dalvik-cache.pre-<scope>`
3. If `runtime.cc:699` fires, ROLLBACK immediately — don't try to patch forward

## See also

- [[v3-scope-c-pib-relocation-2026-05-25]] — the landing where this rule was discovered
- [[v3-scope-b-success-2026-05-24]] — Scope B (which didn't need this rule because no shadowing)
- [[feedback-no-builds-on-hbc-or-alex-2026-05-25]] — directional rule that pushed builds local where this issue surfaces
- [[reference-local-build-infra-2026-05-25]] — local build pipeline (where to verify BCP order)
