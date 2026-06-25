# Material Catalog APK smali patches

The Material Catalog (`io.material.catalog`) runs **stock** on the adapter — the
single APK patch here is **cosmetic** (it does not affect basic functionality; the
catalog launches and all 32 widget categories work without it).

## `ContainerTransformConfigurationHelper.smali` — the morph-animation enabler

**What it solves.** The shared-element container-transform *morph* (the Container
Transform demos) snapped instead of animating.

**Root cause.** OHOS app processes boot with
`android.animation.ValueAnimator.sDurationScale == 0` — **all** animations are
globally disabled (AOSP default is 1.0f, but a caller in framework `classes4.dex`
sets it to 0 at init; the adapter's `animator_duration_scale` prime isn't wired to
it). With scale 0, every one-shot animator jumps to its end value on frame 1.

**The patch.** Inject `ValueAnimator.setDurationScale(1.0f)` at the very top of
`io.material.catalog.transition.ContainerTransformConfigurationHelper.configure()`
(this runs right before each morph). The two injected instructions:

```smali
.method configure(Lcom/google/android/material/transition/MaterialContainerTransform;Z)V
    .registers 10
    ...
    const/high16 v0, 0x3f800000    # 1.0f                 <-- injected
    invoke-static {v0}, Landroid/animation/ValueAnimator;->setDurationScale(F)V   <-- injected
    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z
    ...
```

This `.smali` is the **complete patched class** as baksmali'd from the deployed,
patched catalog APK (md5 `a9df5518`). The class lives in **`classes3.dex`** of the
catalog APK.

**Apply / deploy.** See `BUILD-FROM-SOURCE.md` §1.3 (the baksmali→drop-in→
SmaliAssemble→rezip→debug-sign round-trip) and `CATALOG-REPRODUCE.md` §F (the
deploy path — `/data/app/el1/bundle/public/io.material.catalog/android/base.apk`,
clear `oat/arm/*`; `bm install` wipes the bundle dir so re-deploy after install).

**Status: FIXED + VERIFIED** — Container Transform "View" demo morphs frame-by-frame
(`docs/engine/V3-CATALOG-L3-MORPH-EVIDENCE/`).

> The cross-activity morph also needs the `ActivityOptions` Bundle carried from the
> source activity to the destination's `EnterTransitionCoordinator`. That is
> **adapter-side** plumbing (the `TransitionOptionsHolder` class added to
> `adapter-runtime-bcp` + stash/resolve edits in `ActivityTaskManagerAdapter` /
> `AppSchedulerBridge`), shipped under `framework-smali-patches/catalog/` — not an
> APK patch. The durationScale patch here is the only edit to the catalog's own
> bytes.
