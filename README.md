# Westlake — stock Android apps on OpenHarmony

Run **unmodified Android APKs** on an OpenHarmony (OHOS) DAYU200 / RK3568 board,
via the **`appspawn-x`** AOSP-app adapter (a Zygote-style fork that loads Android
apps under an Android-compatible BOOTCLASSPATH + a custom `libart`).

This repo reproduces two apps end-to-end:

| App | Package | Upstream | State |
|---|---|---|---|
| **Material Components Catalog** | `io.material.catalog` | `material-components-android` 1.9.0 | Launches from the launcher icon, 32 categories navigable L1→L5, 0 functional failures in a full sweep |
| **noice** | `com.github.ashutoshgngwr.noice` | `trynoice/android-app` 2.5.1 | Renders, D-pad + touch interactive, 9/10 pages crash-free |

**Both apps are stock.** Their source was *not* changed. The only edits to the
app binaries are a handful of optional **cosmetic smali patches** on the compiled
APK (`catalog-smali-patches/`, `noice-smali-patches/`) — each app runs without
them. Everything that makes Android apps run on OHOS lives in the **adapter**
(`libart`, the bridges, the BCP jars, the boot image, the launcher `entry.hap`).

> Honest status of exactly what works / what doesn't: **[`STATUS.md`](STATUS.md)**.

> **★ Latest (2026-06-28): a single unified generation now runs BOTH apps at once** (one libart +
> one framework + one boot image), plus boot-time auto-start of appspawn-x, the noice launcher icon,
> and a first-run-wizard skip. Start here: **[`UNIFIED-CONFIG-REPRODUCE.md`](UNIFIED-CONFIG-REPRODUCE.md)**
> (uses `framework-smali-patches/unified-connectivity/`, `config/asx-autostart/`, `entry-hap/noice/`).

---

## Reproduce — the ordered master flow

Do these phases **in order**. Each links to the doc with the real commands. The
adapter (Phases 1–3) must be built and deployed before either app will run; the
two apps (Phase 4) are independent and can be built in parallel.

### Phase 0 — Prerequisites → [`docs/REPRODUCTION-GUIDE.md`](docs/REPRODUCTION-GUIDE.md) §2
- Hardware: DAYU200 / RK3568 board (DC-powered; mock battery — reboots are always safe, see [`notes/device-safety.md`](notes/device-safety.md)).
- Host (WSL2 / Linux): `hdc`; the OHOS source tree + clang; `dex2oat64` (OAT v230); `restool`; Android SDK + Gradle + JDK 17; baksmali/smali (`scripts/SmaliAssemble.java`).

### Phase 1 — Flash the OHOS base → [`docs/REPRODUCTION-GUIDE.md`](docs/REPRODUCTION-GUIDE.md) §2
- Flash the project's OHOS base image onto the board. Factory OHOS has no `/system/android` — the adapter creates it.

### Phase 2 — Build the adapter → [`docs/REPRODUCTION-GUIDE.md`](docs/REPRODUCTION-GUIDE.md) §4
Source/patches in this repo; large outputs are listed by md5 + provenance in [`ARTIFACT-INVENTORY.txt`](ARTIFACT-INVENTORY.txt) (rebuild, don't pull):
- **`libart`** — the W-series vtable fixups + IMT / fault-handler / nterp patches. **Buildable form in [`libart-build/`](libart-build/)** (patched sources + `build_libart_pathA.sh` + `BASE-MANIFEST.md` pinning AOSP `platform/art @ 814cc93`, 24Q4 / OAT v230); reviewable diffs in `libart-patches/`.
- **Bridges** — `bridge-src/` (`liboh_adapter_bridge`: input/touch/D-pad dispatch, `createHardwareBitmap`; `liboh_ime_helper`: the IME bridge).
- **BCP jars** — `framework.jar` + the adapter jars, with the smali patches in `framework-smali-patches/` (catalog: metaData / ConnectivityManager / ContentResolver guards, `OhImeBridge`; noice: ShortcutManager / AlarmManager fetchers).
- **Boot image** — `dex2oat64` over the 10-jar BOOTCLASSPATH (exact order in the guide).

### Phase 3 — Deploy the adapter + demo configs → [`docs/REPRODUCTION-GUIDE.md`](docs/REPRODUCTION-GUIDE.md) §5, §7
- Stage to `/system/android/...` with the correct SELinux labels; install the boot image and `appspawn_x.cfg`.
- Demo-enabling configs: `SELINUX=permissive` for hands-off cold boot ([`notes/`](notes/)); the minimal `config/hm_symbol_config_next.json` (fixes the cold-boot fontconfig O(n²) freeze); the `.app`→`.apk` byte-patch in `libappexecfwk_common` (enables `bm install`).

### Phase 4 — Build the two apps from source → [`BUILD-FROM-SOURCE.md`](BUILD-FROM-SOURCE.md)
- **Catalog**: clone `material-components-android`, `git checkout 1.9.0`, `./gradlew :catalog:assembleDebug`.
- **noice**: clone `trynoice/android-app`, checkout 2.5.1, `./gradlew assembleDebug`.
- Optional: re-apply the cosmetic smali patches (`catalog-smali-patches/`, `noice-smali-patches/`).

### Phase 5 — Register + deploy the apps → [`CATALOG-REPRODUCE.md`](CATALOG-REPRODUCE.md) §1–2 · [`REPRODUCE.md`](REPRODUCE.md) §4D
- **Catalog**: `bm install -p catalog.apk` (enabled by the Phase-3 `.app`→`.apk` patch), then deploy its `entry.hap` launcher icon (build inputs in `entry-hap/`).
- **noice**: drop into the bundle dir + its `entry.hap`. noice-specific lower-layer setup (CA store `ca-store/`, native TLS `native-tls/`, inet/eBPF socket grant `bpf-analysis/`) is in `REPRODUCE.md`.

### Phase 6 — Run + verify → [`docs/REPRODUCTION-GUIDE.md`](docs/REPRODUCTION-GUIDE.md) §9 · [`STATUS.md`](STATUS.md)
- Power on → boots Permissive → swipe-unlock → **tap the launcher icon** → the app renders → navigate.
- Verify: appspawn-x reaches Phase 4, `bm dump` shows the app registered, child stderr shows `drew=1`, 0 crash tombstones. Helper scripts in `scripts/` (`livetest.sh`, `navtour.sh`).

---

## Repo map

| Path | What |
|---|---|
| [`STATUS.md`](STATUS.md) | Honest what-works / what-doesn't, per app, with deployed md5s |
| [`BUILD-FROM-SOURCE.md`](BUILD-FROM-SOURCE.md) | Build either stock APK from upstream |
| [`CATALOG-REPRODUCE.md`](CATALOG-REPRODUCE.md) · [`REPRODUCE.md`](REPRODUCE.md) | Per-app: register, deploy, the fixes, the demo flow |
| [`docs/REPRODUCTION-GUIDE.md`](docs/REPRODUCTION-GUIDE.md) | The adapter: full build + deploy + fixes (802 lines) |
| [`docs/`](docs/) | Root-cause history (per-wall deep dives) |
| [`ARTIFACT-INVENTORY.txt`](ARTIFACT-INVENTORY.txt) | Every binary by md5 + provenance (so nothing large is committed) |
| [`libart-build/`](libart-build/) | **Buildable ART** — patched libart sources + build script + base-pin manifest (`platform/art @ 814cc93`, OAT v230) |
| `libart-patches/` · `framework-smali-patches/` · `bridge-src/` | Adapter source/patches (libart diffs, framework smali, bridge C++) |
| `catalog-smali-patches/` · `noice-smali-patches/` | The optional cosmetic APK patches |
| `entry-hap/` | Launcher-icon HAP build inputs |
| `config/` · `notes/` | Cold-boot fontconfig; SELinux + device-safety footguns |
| `ca-store/` · `native-tls/` · `bpf-analysis/` | noice connectivity stack |
| `scripts/` · `screenshots/` · `test-fixtures/` | Deploy/test helpers; evidence; fixtures |

## The two foundational walls (shared, adapter-level — not per-app bugs)

1. **Synthetic input → Android `InputConnection` not bridged** — IME text-commit
   and the BACK key don't route through injected input (a physical keyboard is the
   untested real test; the IME window + focus path is proven).
2. **Adapter app windows don't hold durable OHOS WMS focus** — some modals
   composite intermittently per boot; this is the single highest-leverage
   remaining item for both apps.

See `STATUS.md` for the full list, including audio output (noice, not wired) and
the live-HTTPS okhttp/conscrypt wall.
