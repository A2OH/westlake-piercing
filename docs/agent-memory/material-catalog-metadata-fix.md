---
name: material-catalog-metadata-fix
description: "Material Components Catalog (io.material.catalog) now runs on the adapter — fixed null ApplicationInfo.metaData NPE; the old \"AESKeyGenProbe crash\" diagnosis was WRONG"
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

**Material Components Catalog (`io.material.catalog`, v from F-Droid) RENDERS on the
Westlake adapter** (2026-06-22) — the Adaptive demos page (Description card, List
View Demo / Feed Demo / Single View Hero / Supporting Panel, Material list items +
play/favorite components). Evidence `docs/engine/V3-NOICE-DPAD-FINDINGS/catalog-rendered.jpeg`.
It previously crashed deterministically at init (0/15).

**ROOT CAUSE (corrected — the old AESKeyGenProbe theory was WRONG).** `AESKeyGenProbe`
is a NON-fatal adapter diagnostic (`adapter.diag.AESKeyGenProbe`) that runs in EVERY
child incl. the alive noice — it is NOT the catalog killer. The real fatal (captured
live in the catalog child stderr + AMS hilog): the app's own
`CatalogApplication.onCreate(:49) → overrideApplicationComponent(:70)` does
`getApplicationInfo().metaData.getString("io.material.catalog.application.componentOverride")`
→ **NPE because `ApplicationInfo.metaData` was null** → process killed before any UI.
(Decompiled catalog confirms: if that getString returns null it falls back to the
default Dagger component — so a NON-NULL Bundle is sufficient; real values not needed.)

**FIX (deployed + validated).** Make `ApplicationInfo.metaData` a non-null Bundle.
- The ACTIVE `PackageInfoBuilder` is in **`adapter-runtime-bcp.jar`** (PIB-TIER2),
  which is loaded BEFORE ohaf in the BCP and SHADOWS ohaf's copy (first-jar-wins).
  Patching ohaf's PIB did NOTHING (wrong/shadowed copy) — must patch arb's.
- Smali patch of `adapter/packagemanager/PackageInfoBuilder.buildApplicationInfo`:
  right after `new ApplicationInfo()` / `<init>`, add `new-instance Bundle; <init>;
  iput-object metaData` (v1 free, `.locals 8`). arb round-trip PRESERVED the TLS `$Sf`
  shim (verified DexClassLoader/loadClass/getDeclaredConstructor intact). arb
  `d5d39a05`→**`6e32a253`**.
- Also: bridge manifest parser (`apk_manifest_parser.cpp` + `apk_manifest_jni.cpp`)
  now extracts `<application>`-level `<meta-data>`→JSON (`appMetaData`) for FUTURE
  real-value population. Bridge built `4b4741f1` (NOT deployed — the empty-Bundle in
  PIB is what fixes the catalog; deployed bridge stays `60126181`).
- ohaf also got the same PIB patch (`3654ebc5`, shadowed/harmless) before I realized
  arb shadows it.

**BOOT REGEN (arb + ohaf are BCP).** Multi-image 10-jar regen via
`docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh` (WORK=/tmp/metaregen, host
`$HOME/tools/dex2oat64`, --base 0x70000000, 30 segments). **Brick-safe
validation: byte-compare regenerated `boot-framework.oat` to the deployed one — MATCH
(md5 ad790fe9) proves the dex2oat pairs with deployed libart 7b856a2d, so only the
intended segments differ.** Deployed boot-adapter-runtime-bcp.oat `e55a3e8a`. Device
booted fine both regens. Backups: `/data/local/tmp/{boot-pre-metadata, arb.pre-metadata,
ohaf.pre-metadata}` + the existing boot-pre-chooser/sslsockets.

**NO REGRESSION:** noice still renders the full populated library (68623, network/TLS
intact — the arb round-trip preserved the `$Sf` TLS shim). Evidence
`noice-after-metafix.jpeg`. Banked artifacts: `$HOME/metadata-fix/`.

**LESSON:** running it live corrected a wrong recalled root cause (AESKeyGenProbe).
Also: for BCP classes with duplicate copies, find which jar wins (BCP order /
first-jar-wins) before patching — ohaf's PIB is shadowed by arb's. Cross-ref
[[westlake-repro-repo-state]].
