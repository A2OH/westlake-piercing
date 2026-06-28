---
name: westlake-repro-repo-state
description: A2OH/westlake noice reproduce package state + the true (collect-only) reproduction gap; local clone was stale
metadata: 
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

The canonical noice-on-OHOS reproduce repo is **A2OH/westlake**, dir
`westlake-noice-ohos/` (local clone: `$HOME/android-to-openharmony-migration`,
default branch `main`). The westlake repo `westlake/westlake-noice-ohos`
(`$HOME/westlake-repo`) is an older/smaller subset — A2OH is authoritative.

**LESSON (2026-06-08):** the local A2OH clone was **stale** (origin/main was at
`11e90faf` locally but the remote HEAD was `d1e81182`). I wrongly concluded "the
noice package isn't in A2OH" from the stale checkout. **Always `git fetch` before
asserting what's pushed.** After fetch, `d1e81182` already had the full package:
HANDOFF-2026-06-07.md, REPRODUCE.md, REPRODUCE-CLEAN-WSL.md, MANIFEST.md,
collect-artifacts.sh, committed libart (`runtime-proxy-fix/libart.so.7b856a2d`),
all BCP jars (`prebuilt-jars/{oh-adapter-framework.jar.300581d1,
adapter-runtime-bcp.jar.d5d39a05, adapter-mainline-stubs.jar.41834c1f}`), shims
(`prebuilt-native/lib{dnshook,jdnshook,netlog,setgidhook,w14supp}.so`), native-tls,
sslsockets-fix, chooser share-fix, screenshots.

**The TRUE reason a fresh checkout can't boot** (not a missing-push problem — it's
inherent): 4 artifacts are un-rebuildable or too large for git and must be pulled
from a live device via `collect-artifacts.sh`:
1. `liboh_android_runtime.so` `f82d8cdc` — un-rebuildable blob (no source).
2. `libhwui.so` `8b8f84ec` — build env not reproducible on clean WSL.
3. boot image ~143MB / 30 segments — size; must be the SAME dex2oat pair as the
   committed libart (else LinkageError).
4. `framework.jar` `8524dc56` ~15MB — size (rebuildable from framework-smali-patches).

**Pushed 2026-06-08 (commit `0cf5ae07` on main):** this session's deltas —
`share-fix/oh_ability_manager_client.cpp`(+.h, activity_manager_adapter.cpp) =
pasteboard sendData→clipboard layer; `share-fix/PASTEBOARD-SHARE-BUILD-NOTES.md`;
`scripts/start_asx.sh` (full G5 bringup) + `scripts/launch_noice.README.md`;
`SESSION-2026-06-08.md`. Noted: committed binaries are slightly behind live but
rebuildable — ohaf `300581d1`(committed)→`efd3f740`(live, +clamp); bridge
`82b0d82a`(committed)→`d2e50209`(scroll)→`60126181`(live, pasteboard). See
[[noice-share-crash-fixed-verified]].

**Repo commit identity:** use `westlake <westlake@users.noreply.github.com>`
(GitHub noreply; id [id]). DO NOT use `[REDACTED-EMAIL]` (user instruction
2026-06-08) and DO NOT use `[REDACTED-EMAIL]`/`[REDACTED-EMAIL]` (GitHub
email-privacy rejects them). Repo-local git config in
`$HOME/android-to-openharmony-migration` is set to the noreply. Push to
A2OH, NOT westlake, NOT [org] org. (Commit `0cf5ae07` was wrongly authored with
[REDACTED-EMAIL] then re-authored to `ee98864c` with the noreply + force-pushed.)

**The baseline is LOCAL — the device is NOT needed to assemble it.** All deployed
files were built/staged on this machine. Verified md5-match to deployed:
runtime **`16e08711`** = `docs/engine/V3-NOICE-G38-EVIDENCE/liboh_android_runtime.g38.so`
(the un-rebuildable G3.8 blob); libart **`7b856a2d`** = `$HOME/libart-pathA-work/out/libart.so`;
libhwui **`8b8f84ec`** = `docs/engine/V3-NOICE-DPAD-FINDINGS/libhwui.8b8f84ec-NSFIX-nored-gatedreads.so`;
bridge **`60126181`** = `$HOME/bridge-build/out/adapter/liboh_adapter_bridge.so`.
Boot image + efd3f740-clamp ohaf were in /tmp (cleared on reboot) but are
locally rebuildable (boot via `/tmp/tagsoup-boot/regen.sh` from the 10 BCP jars;
efd3f740 via the smali clamp patch). framework.jar candidates at
`openharmony/tmp_j2/device-jars/framework.jar` + `westlake-deploy-ohos/v3-hbc/jars/`.

**Baseline tarball ASSEMBLED 2026-06-08 (no device needed):**
`$HOME/westlake-baseline-300581d1-20260608.tar.gz` (74MB, 51 files, md5
`74f67dbb`), staging at `$HOME/westlake-baseline/` with `MANIFEST.md5` +
`README-BASELINE.md`. Self-consistent **300581d1-era** set: lib/ (runtime
16e08711, libart 7b856a2d, libhwui 8b8f84ec, bridge 60126181, 5 shims, libtlsjni,
libv4force) + framework/ (jars 300581d1/d5d39a05/41834c1f + framework.jar
e1dae174 + tlsjni-extra.dex) + framework-arm/ (27-seg banked boot image,
pre-clamp) + tmp/ (bpfgrant, apk_install, noice-base.apk) + scripts/start_asx.sh.
Banked boot came from Windows staging `C:\Users\dspfa\Dev\ohos-tools\bf_boot-*`.
Documented deltas vs LIVE (all rebuildable, not boot-coupled): ohaf
300581d1→efd3f740 (clamp), framework.jar e1dae174→8524dc56, libtlsjni/libv4force
committed≠live. To get exact-current: apply clamp smali + regen boot via
`docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh`. **TODO:** upload tarball as a
GitHub release asset on A2OH/westlake (too big for git; gh auth for westlake was
timing out 2026-06-08 — retry).

**COMPLETE from-zero reproduction set PUBLISHED 2026-06-08** as release assets on
A2OH/westlake tag `baseline-300581d1-20260608` (4 assets, all state=uploaded):
(1) `westlake-complete-bundle-20260608.tar.gz` 247MB md5 `14eaa14f` — the full
3-layer set: `overlay/` = the v3-hbc CONSISTENT complete adapter (appspawn-x +
56 libs incl. ART companions + 14 jars + matched bcp/ boot + etc/ configs +
deploy scripts incl. DEPLOY_SOP.md) + `current-fixes/` (runtime 16e08711, libart
7b856a2d + paired 300581d1 boot, hwui 8b8f84ec, bridge 60126181, jars, shims) +
`device-tmp/` (start_asx.sh, launch_noice.sh [REAL script from Win staging],
bpfgrant, apk_install, noice-base.apk) + README-COMPLETE.md + MANIFEST.md5 (163
files); (2) `ohos-base-system.img.gz` 606MB md5 `fca2f09e` (DAYU200/RK3568 base,
gunzip→flash); (3) `ohos-base-updater.img` 20MB md5 `f3d15a6b`; (4)
`westlake-baseline-300581d1-20260608.tar.gz` 74MB md5 `74f67dbb` (curated subset).
(5) `ohos-dayu200-flash-extras.tar` 496MB md5 `984e1c3e` — the REST of the RK3568
flash set (MiniLoaderAll.bin, parameter.txt, config.cfg, uboot/boot_linux/ramdisk/
resource/vendor/chip_*/sys_prod/eng_system/updater.img + MD5SUMS.txt; untar →
RKDevTool folder). `userdata.img` (1.4GB) omitted (wipe-on-flash). Repo MANIFEST.md
lists all 5 (origin/main 95299831). **Gotcha: `tar -czf` (gzip) of multi-GB sets
gets CPU-killed ~200MB in this env — use `tar -cf` (no gzip) for big image sets;
github uploads also intermittently drop (gh RC may be 1 even when the asset lands
fully — verify by asset size==local size + state=uploaded).** Sources: v3-hbc bundle
`westlake-deploy-ohos/v3-hbc/`, base images `hbc-v7-images/`, appspawn-x
`v3-hbc/bin/`, launch_noice.sh `C:\Users\dspfa\Dev\ohos-tools\`. NOTE: v3-hbc
overlay is libart eadd3926 generation (self-consistent); current-fixes upgrades
to 7b856a2d + its boot — apply libart+boot together (dex2oat pair).

**FUTUREWEI EMAIL PURGE done 2026-06-08:** audited all westlake commits — 8 on
origin/main used `[REDACTED-EMAIL]`; rewrote via filter-branch to the noreply,
force-pushed. origin/main now `113444e1`, verified CLEAN of [org].

**CATALOG REPRO GAP CLOSED 2026-06-23 (commit `f7c5f1ca` on origin/main).** Added
`westlake-noice-ohos/catalog-fix/` so another agent can reproduce Material Catalog
(io.material.catalog) incl. 2nd-level demo Activities on the same noice baseline.
Two committed deltas (prebuilt + source): (1) metaData NPE — `adapter-runtime-bcp.jar.6e32a253`
(from `/mnt/c/Users/<user>/Dev/ohos-tools/bcprecv/`) + `PackageInfoBuilder.smali`,
BCP→needs boot regen (`regen_boot.sh` included); (2) 2nd-level createHardwareBitmap
SIGBUS — `libhwui.so.0c82b1db` (from Win staging `new_libhwui_0c82b1db.so`) +
`liboh_adapter_bridge.so.20ab65a6` (from Win `new_bridge_20ab65a6.so`) + sources
`android_graphics_HardwareRenderer.cpp`/`surface_oh_helper.cpp`, no boot regen.
+ `REPRODUCE-CATALOG.md`, evidence, md5 manifest; START-HERE.md + MANIFEST.md updated.
**GOTCHAS for next time:** (a) `bridge-build/out/adapter/liboh_adapter_bridge.so` is
VOLATILE — md5 drifted 20ab65a6→f6cfbf66 mid-session (concurrent rebuild?); always
commit from the Win-staged validated `new_bridge_20ab65a6.so`, md5-verify before commit.
(b) libhwui source rebuilds to `01f1900f` (clean, AImageReader path) NOT deployed
`0c82b1db` — functionally equiv, but ship the prebuilt 0c82b1db for exact match.
(c) github 443 push intermittently times out (~135s) then succeeds on retry — known.
See [[catalog-2nd-level-canvascontext-wall]], [[material-catalog-metadata-fix]].

**CATALOG NOW TURNKEY (no dex2oat) 2026-06-23, commit `8816bc7f` + release `catalog-20260623`.**
Closed the last 2 catalog repro gaps (unpublished APK + unpublished boot/dex2oat). Published
2 release assets on A2OH/westlake tag `catalog-20260623`: (1) `catalog-overlay-20260623.tar`
(173MB, md5 `5bcdf3e3`) = COHERENT 10-jar BCP set + matching 30-segment boot image
(`framework/`+`framework-arm/`+README+MANIFEST.md5); (2) `catalog-io.material.catalog.apk`
(15MB, md5 `8cfd28db`) = STOCK catalog (io.material.catalog, MainActivity; the `catalog_ohti.apk`
variant has +1 OHTouchInjector class — NOT needed, bridge noice_tap control-channel drives nav).
**Boot regen method (reusable):** assembled `/tmp/catalog-boot/out/{aosp_fwk/7jars, adapter/3jars}`
= bcprecv's 8 pre-arb jars (match baseline) + arb `6e32a253` + ohaf `300581d1` (from release);
ran `docs/engine/V3-5APP-V2-EVIDENCE/regen_boot.sh` (WORK=/tmp/catalog-boot, dex2oat64
`$HOME/tools/dex2oat64` + sigchain, 13.7s). **BRICK-SAFETY PROVEN:** regenerated
`boot-framework.oat` == bcprecv known-good `ad790fe9` BYTE-IDENTICAL (cmp) → pairs with libart
`7b856a2d`. **KEY GOTCHA:** the noice RELEASE boot (`current-fixes/framework-arm/boot-framework.oat`
= `fa5ef039`) was built from a DIFFERENT framework.jar gen than the catalog boot (`ad790fe9`,
framework.jar `8524dc56`) — so you CANNOT drop just the arb boot segment onto the release boot;
must ship/deploy the WHOLE coherent 10-jar+boot overlay together (that's why the tar bundles all
10 jars incl framework.jar 8524dc56, not just arb). bcprecv `/mnt/c/Users/<user>/Dev/ohos-tools/bcprecv/`
= the metadata-fix work dir (has arb 6e32a253 + the known-good boot-framework.oat). dex2oat reads
faster from local /tmp than /mnt/c. Reproduction is now: flash base → noice release → catalog
overlay tar + 2 .so (committed) + APK → reboot. noice AND catalog both fully reproducible.
