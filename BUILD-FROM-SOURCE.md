# Building the two apps from upstream source

Both apps that this repo runs on the OpenHarmony (OHOS) `appspawn-x` adapter —
**Material Components Catalog** and **noice** — are **stock, unmodified Android
applications**. Their source was **not** changed for OHOS. Everything that makes
them run on OHOS lives in the *adapter* (libart, the bridges, the BCP jars, the
boot image, the launcher `entry.hap`) — see `REPRODUCE.md` (noice) and
`CATALOG-REPRODUCE.md` (catalog).

The only edits ever applied to the app *binaries* are a handful of **cosmetic /
behaviour-polish smali patches on the compiled APK** (not source changes); they
are shipped under `noice-smali-patches/` and `catalog-smali-patches/` and are
**optional** (each app runs without them — they fix transition polish / a
background-coroutine crash, not basic functionality). This document explains how
to rebuild each app from its public upstream so you can reproduce the exact APK
the adapter was tested against, and (optionally) re-apply the cosmetic patches.

> Deployed APK identities (what the adapter was tested against):
>
> | App | Package | versionName / versionCode | min/target/compile SDK | Stock label |
> |---|---|---|---|---|
> | Material Catalog | `io.material.catalog` | `1.0` / `1` | 14 / 33 / 33 (Android 13) | `M3 Catalog` |
> | noice | `com.github.ashutoshgngwr.noice` | `2.5.1` / `66` | 21 / 33 / 33 (Android 13) | `Noice` |
>
> (Read off the deployed APKs with
> `aapt2 dump badging <apk> | grep -E 'package:|sdkVersion|targetSdk|application-label'`;
> `aapt2` is in `$HOME/android-sdk/build-tools/30.0.3/`.)

---

## 0. Common host toolchain

A standard Android app build environment. The reference host is WSL2 (Ubuntu);
a native Linux or macOS host works identically.

| Need | Reference on the build host | Notes |
|---|---|---|
| JDK | JDK 17 (Catalog/MDC and noice both build on 17) | A JDK 11 also works for these versions; 17 is what was used. |
| Android SDK | `$HOME/android-sdk` | `platforms/android-33`, `build-tools/30.0.3` + `34.0.0`, `cmdline-tools/latest`. |
| Gradle | the project's bundled Gradle wrapper (`./gradlew`) | Do **not** install Gradle globally — use the wrapper each repo ships. |
| Git | any | To clone the two upstreams. |

Set `ANDROID_SDK_ROOT` (or create a `local.sdk.dir` / `local.properties`
`sdk.dir=`) to your SDK before invoking Gradle. The first build downloads the
matching Android Gradle Plugin + dependencies from Maven Central / Google's Maven
(network required once).

Both projects produce an **unsigned-or-debug-signed** APK from
`assembleDebug` — exactly what the adapter consumes (the adapter does not verify
app signatures; APKs are installed via `bm install`, see `CATALOG-REPRODUCE.md`
§1, or dropped into the bundle dir for noice, see `REPRODUCE.md` §4D).

---

## 1. Material Components Catalog (`io.material.catalog`)

The catalog is the `catalog` Gradle module of the **Material Components for
Android** repository.

### 1.1 Clone the matching version

The deployed APK is `versionName 1.0 / versionCode 1`, built against
`compileSdk 33` (Android 13). Material Components has used the same internal
`versionCode 1 / versionName 1.0` for the catalog across many releases, so the
**SDK level pins the version, not the catalog versionName**: pick the
`material-components-android` tag whose `compileSdkVersion` is **33**. That is the
**`1.9.0`** release line (the last to target/compile SDK 33; `1.10.0`+ moved to
SDK 34).

```bash
git clone https://github.com/material-components/material-components-android.git
cd material-components-android
git checkout 1.9.0          # compileSdk 33 == the deployed APK's build target
# (verify: grep -R "compileSdkVersion" build.gradle gradle/ | head)
```

If you need a byte-closer match, diff your built `AndroidManifest.xml` /
`resources.arsc` against the deployed `catalog.apk` and bisect tags around
`1.9.0`; for running on the adapter, any SDK-33 catalog build is functionally
equivalent (the fixes are adapter-side, not version-specific).

### 1.2 Build the catalog APK

```bash
# from the material-components-android checkout root:
./gradlew :catalog:assembleDebug
# output:
#   catalog/build/outputs/apk/debug/catalog-debug.apk
```

That APK is the stock catalog. Install it on the adapter per
`CATALOG-REPRODUCE.md` §1 (the `.app`→`.apk` byte-patch on
`libappexecfwk_common` + `bm install -p catalog-debug.apk`), then give it a
launcher icon per §2 (`entry.hap`).

### 1.3 (Optional) the one cosmetic catalog patch — `setDurationScale(1.0f)`

The single APK-level catalog patch is **not** required for the catalog to run; it
makes the shared-element *container-transform morph* animate instead of snapping.
Root cause: OHOS app processes boot with
`android.animation.ValueAnimator.sDurationScale == 0` (all animations globally
disabled — see `CATALOG-REPRODUCE.md` §F). The fix injects
`ValueAnimator.setDurationScale(1.0f)` at the top of the catalog's
`io.material.catalog.transition.ContainerTransformConfigurationHelper.configure()`.

The patched smali is `catalog-smali-patches/ContainerTransformConfigurationHelper.smali`
(class lives in **`classes3.dex`** of the catalog APK). The patch is the first two
instructions of `configure(MaterialContainerTransform, boolean)`:

```smali
.method configure(Lcom/google/android/material/transition/MaterialContainerTransform;Z)V
    .registers 10
    .param p1, "transform"    # Lcom/google/android/material/transition/MaterialContainerTransform;
    .param p2, "entering"     # Z

    const/high16 v0, 0x3f800000    # 1.0f                 <-- injected
    invoke-static {v0}, Landroid/animation/ValueAnimator;->setDurationScale(F)V   <-- injected

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z
    ...
```

To re-apply it to a freshly-built catalog APK:

```bash
# 1. extract classes3.dex, baksmali, drop in the patched class, reassemble:
unzip -o catalog-debug.apk classes3.dex -d /tmp/catx
java -cp $HOME/apktool.jar:/tmp/fwktools Baksmali2 /tmp/catx/classes3.dex /tmp/cat_sm
cp catalog-smali-patches/ContainerTransformConfigurationHelper.smali \
   /tmp/cat_sm/io/material/catalog/transition/ContainerTransformConfigurationHelper.smali
java -cp $HOME/apktool.jar:/tmp/fwktools SmaliAssemble /tmp/cat_sm classes3.dex 39
# 2. swap the dex back into the APK (keep META-INF), realign + debug-sign:
zip catalog-debug.apk classes3.dex
zipalign -p -f 4 catalog-debug.apk catalog-aligned.apk
apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android \
  --out catalog-durscale.apk catalog-aligned.apk
```

(`/tmp/fwktools` holds the `Baksmali2` / `SmaliAssemble` dexlib2 wrappers — see
`scripts/SmaliAssemble.java`; `apktool.jar` is apktool 2.9.x.)

> **★ Catalog deploy lesson (cost hours):** a running catalog loads its APK from
> `/data/app/el1/bundle/public/io.material.catalog/android/base.apk` (+ its
> `oat/arm/` cache) — **not** `/data/app/android/io.material.catalog/base.apk`.
> Deploy to the `el1/bundle` path, `chmod 0644`, clear `oat/arm/*`, relaunch.
> `bm install` WIPES the bundle dir — re-deploy the patched APK **and** the
> `entry.hap` after every install. (Full detail: `CATALOG-REPRODUCE.md` §F.)

---

## 2. noice (`com.github.ashutoshgngwr.noice`)

noice is a single-module Android app from `trynoice/android-app`.

### 2.1 Clone the matching version

The deployed APK is `versionName 2.5.1 / versionCode 66`, `targetSdk 33`.

```bash
git clone https://github.com/trynoice/android-app.git
cd android-app
git checkout 2.5.1          # versionCode 66 (tag matches versionName)
# (verify: grep -R "versionName\|versionCode" app/build.gradle*)
```

### 2.2 Build the noice APK

```bash
# from the android-app checkout root:
./gradlew assembleDebug
# output (single-module app):
#   app/build/outputs/apk/debug/app-debug.apk      (the "free" flavor if flavors exist)
```

If the project defines product flavors (free/paid), build the flavor that matches
your deployed APK (`./gradlew :app:assembleFreeDebug`, etc.); the F-Droid-style
build is the free flavor. The result is the stock noice APK — install it per
`REPRODUCE.md` §4D (drop into the bundle dir + clear `oat`, reboot).

### 2.3 (Optional) the one cosmetic noice patch — the coroutine-crash guard

noice's single shipped APK patch (`noice-smali-patches/kotlinx-coroutines/a.smali`)
is **not** required to run; it stops a background-coroutine exception (the
subscription page's `loadPlans` → `listPlans` network call) from killing the
process — it swallows/logs the exception unless it is on the main thread. Full
detail and the apktool round-trip to re-apply it are in `REPRODUCE.md` §4D.

(Other historical noice APK tweaks — tag rendering, a FlexboxLayout measure fix,
per-app ShortcutManager/AlarmManager guards — are now redundant with the
adapter's `framework.jar` null-service guards and are documented in
`docs/noice-*.md`; only the coroutine patch is shipped here.)

---

## 3. Summary — stock apps, adapter-side fixes

- **Both APKs are stock upstream builds.** `assembleDebug` from the tags above
  reproduces what the adapter runs.
- **No app source was modified.** The OHOS-compatibility work is entirely in the
  adapter (libart W-series + perf trim, the bridges + IME helper, the BCP-jar
  null-guards + metaData fix, the boot image, fontconfig, `entry.hap`,
  SELinux) — see `CATALOG-REPRODUCE.md`, `REPRODUCE.md`, and
  `ARTIFACT-INVENTORY.txt`.
- **The only APK edits are cosmetic** and live in `*-smali-patches/`: catalog =
  the `setDurationScale(1.0f)` morph-animation enabler; noice = the
  background-coroutine crash guard. Each app runs correctly without them.
