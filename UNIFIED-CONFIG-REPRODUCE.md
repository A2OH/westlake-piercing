# Unified config — ONE adapter generation runs BOTH catalog AND noice

**Result (2026-06-28):** a single, self-consistent adapter configuration on one DAYU200/RK3568
board launches *and navigates* **both** `io.material.catalog` and `com.github.ashutoshgngwr.noice`.
Previously each app needed its own generation (different libart / framework / boot image), which
appeared to be an irreconcilable conflict. It was not — the "conflict" was two myths:

1. **The noice "Function2-arity wall" was an artifact of the wrong libart branch.** noice's proven
   libart is `7b856a2d` (the *catalog* branch). The reflashed board shipped a divergent `42d2d8e8`
   libart, and a `/system/lib` vs `/system/android/lib` load-path bug silently dropped every
   `7b856a2d` deploy → I kept testing the wrong binary. On `7b856a2d` the arity wall is simply gone.
2. **`libart 2813065e` (catalog's W22-proxy-skip build) BROKE noice.** W22 skips vtable fixup for
   *all* proxy classes — including **annotation proxies** — so AndroidX Navigation's
   `@Navigator.Name` read crashed (`AnnotationFactory.invoke → $ProxyN.value() →
   IllegalArgumentException: …Object.clone()` → `FragmentContainerView` InflateException). The
   pre-W22 `7b856a2d` serves both; W22 was only a catalog *perf* optimisation (LinkMethods O(n²)),
   never a correctness requirement (catalog rendered on the `7b856a2d`-branch build `275eb104`).

## The unified set (deploy these; md5 = first 8 of md5sum)

| component | md5 | device path | notes |
|---|---|---|---|
| libart | `7b856a2d` | `/system/lib/libart.so` | serves BOTH. ⚠ this board loads `/system/lib`, NOT `/system/android/lib` — verify with `/proc/<pid>/maps`. Build: `libart-build/` (this is the same branch as `275eb104`/`2813065e`). |
| framework | `1c334902` | `/system/android/framework/framework.jar` | = catalog `f5fd86ef` + the ConnectivityManager graft (below). |
| boot-framework.oat | `4376897e` | `/system/android/framework/arm/` | regenerated from the unified framework (see below). |
| adapter-runtime-bcp | `c026e80c` | `/system/android/framework/` | catalog metaData fix. |
| adapter-mainline-stubs | `41834c1f` | `/system/android/framework/` | carries the stub `android/net/ConnectivityManager` + SSLSockets. |
| oh-adapter-framework | `300581d1` | `/system/android/framework/` | |
| liboh_android_runtime | `16e08711` | `/system/android/lib/` | the noice runtime (un-rebuildable G3.8 blob; in the binary baseline). |
| libhwui | `1d04a56e` | `/system/android/lib/` | |
| liboh_adapter_bridge | `60126181` | `/system/lib` + `/system/android/lib` | |
| fontconfig | `425290bd` | `/system/fonts/hm_symbol_config_next.json` | cold-boot anti-hang (see `docs/`). |

Binaries are intentionally not in git (see `.gitignore` / `ARTIFACT-INVENTORY.txt`); they live in the
binary baseline. This repo holds the **recipes + source/smali** to build the deltas.

## 1. The framework graft (catalog `f5fd86ef` → unified `1c334902`)

catalog's framework returns **null** for `getSystemService(CONNECTIVITY_SERVICE)`; noice's MainActivity
does `... as ConnectivityManager` (Kotlin non-null cast) → NPE. The catalog and noice frameworks
differ in **exactly one class**: `android/app/ContextImpl`. The noice framework (`8524dc56`) adds a
fallback that constructs the stub `ConnectivityManager` (which already lives in
`adapter-mainline-stubs.jar` `41834c1f`). Graft it in:

See `framework-smali-patches/unified-connectivity/` for the recipe + the exact smali
(`ContextImpl-grafted-methods.smali`, `JobSchedulerStub.smali`).

## 2. Regenerate the boot image

Any BCP-jar change requires a boot-image regen (else dex-checksum mismatch). Use the 10-jar BCP
recipe (`scripts/` / `libart-build/`): pull the 10 live BCP jars, swap in the unified `framework.jar`,
run `dex2oat64` with the documented BCP order, deploy the `boot-*.{art,oat,vdex}` segments to
`/system/android/framework/arm/`. The libart that loads it must be the same branch (`7b856a2d`).

## 3. noice networking — the INET-GID gate

After the graft, noice renders, then a background OkHttp thread crashes:
`SecurityException: missing INTERNET permission → getaddrinfo EPERM`. noice's uid (`13731`) was set
to **no-internet (val=0)** in the cgroup-eBPF `oh_sock_permission_map`. Grant it:
`bpfgrant 13731 oh_sock_permission_map` (see `bpf-analysis/` + `noice-network-inet-gid-fix`). After
the grant noice no longer crashes — it falls back to its own graceful offline UI and the bottom-nav
navigates. (Live library fetch is gated by the deeper INetConnService bridge — out of scope here.)

## 4. Boot-time auto-start of appspawn-x (no more white screen after reboot)

appspawn-x does not reliably auto-start, so a post-reboot icon tap shows a stuck white loading window.
`config/asx-autostart/` adds a **non-critical oneshot** init service that brings up appspawn-x with the
preload shims + socket relabel + bpfgrant. Key detail: `secon: u:r:su:s0` (shell domain) so the
sh→appspawn-x exec does not domain-transition → `AT_SECURE` stays 0 → `LD_PRELOAD` is honoured.

## 5. noice launcher icon + first-run wizard

- **Icon:** the adapter never creates the bundle `entry.hap` the launcher resolves icons from. Build one
  for noice (icon rasterized from its adaptive icon, label "Noice"): see `entry-hap/noice/`.
- **First-run wizard:** noice's `AppIntroActivity` is a second-level Activity that hits the adapter
  input-routing wall (taps not delivered → stuck). Skip it by pre-seeding the SharedPreferences flag
  `has_user_seen_app_intro=true` (see `entry-hap/noice/README.md`).

## Validation

Cold launch via AMS (the icon-tap path):
- catalog: `aa start -b io.material.catalog -a io.material.catalog.main.MainActivity` → Material 3 grid
  → tap a cell (e.g. Buttons) → demo detail.
- noice: `aa start -b com.github.ashutoshgngwr.noice -a com.github.ashutoshgngwr.noice.activity.MainActivity`
  → Sound Library (声音库) → bottom-nav routes (Library / Alarm / Account).
