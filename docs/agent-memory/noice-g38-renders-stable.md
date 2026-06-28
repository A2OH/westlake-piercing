---
name: noice-g38-renders-stable
description: G3.8 breakthrough — noice renders STABLY on OHOS (AppIntro welcome + MainActivity). The "~8s AMS reap" was a misdiagnosis; the real wall was an ASurfaceControl_release use-after-free SIGSEGV in RenderThread teardown, fixed by a 1-instruction binary patch.
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

**2026-06-02: noice RENDERS STABLY on OHOS DAYU200. Canonical goal met + made persistent.** Captured: noice's AppIntroActivity welcome slide (欢迎 + the **noice** wordmark logo + tagline 通过自然平静的噪音集中注意力、冥想和放松。 + 向左滑动探索所有功能! + 跳过/› controls, dark theme) AND MainActivity (声音库 toolbar + 5-tab bottom nav [library/★presets/⏳/⏰alarm/👤] + shuffle FAB). Both stable, interactive, live (clock updates). Evidence: `docs/engine/V3-NOICE-G38-EVIDENCE/noice-appintro-welcome-STABLE.jpeg` + `noice-mainactivity-rendered.jpeg`.

## The real root cause (the "~8s AMS reap" was a MISDIAGNOSIS)
[[noice-egl-rootcause-g34]]'s final theory (AMS reaps noice ~8s in during heavy class-load) was WRONG. With the full graphics chain live, noice reaches RESUMED + VISIBLE (lifecycleState=3, decorVisibility=0, mVisibleFromServer=true, both EGL surfaces created), THEN a competing STOPPED ClientTransaction arrives (one of noice's 2 windows: AppIntro session 14 + MainActivity session 13) → window teardown → `OH_GfxShim: Surface.release` → **SIGSEGV in RenderThread**. AMS's `[FRAMEWORK,PROCESS_KILL] FOREGROUND=1` + "Ability on scheduler died" is AMS CLEANING UP the already-crashed process, not killing a live one. The faultlog (LiteProcessDumper, cppcrash-2091) is decisive:
```
SIGSEGV(SEGV_MAPERR)@0xe79aa024  Tid:RenderThread
#00 pc 00023048 liboh_android_runtime.so (ASurfaceControl_release+20)  r0=fault addr
```

## Why G3.7 (delete→bx lr) didn't fix it, and what G3.8 is
`ASurfaceControl_release` (vaddr 0x23034, ARM, in liboh_android_runtime.so) layout:
- +0x00 `push {r4,lr}` ; +0x04 `cmp r0,#0`/beq ; +0x0c `mov r4,r0` ; +0x10 `add r0,r0,#4` (=&sc->refcount)
- **+0x14 `ldrex r1,[r0]` ← CRASH**: derefs a `sc` that points to an UNMAPPED page (SEGV_MAPERR) = use-after-free
- ... decrement refcount; if hit 0, `destroy(handle)` ; +0x60 `bx lr` (G3.7 patched the old `delete sc` here)
The crash at +0x14 happens BEFORE the +0x60 delete site, so G3.7 was necessary-but-insufficient. The struct (`{handle@0,refcount@4,flag@8}`, 12-byte operator-new in ASurfaceControl_create @0x22f0c) is freed by ANOTHER path (Java-side nativeRelease during the 2-window teardown) while hwui's RenderThread asynchronously calls ASurfaceControl_release on the now-dangling pointer.
**G3.8 fix = make ASurfaceControl_release a full no-op:** patch entry (file off 0x22034) `10 40 2d e9` (push) → `1e ff 2f e1` (`bx lr`). RenderThread never derefs the freed struct → no UAF. SC structs+OH handles leak (bounded; RSSurfaceNode owns the real surface lifecycle anyway, per OH_GfxShim "no-op" comment) — acceptable for render. Same proven binary-patch technique as G3.7. Verified with capstone.

## Device state (HEALTHY, G3.8 live)
runtime `liboh_android_runtime.so` = **16e08711** (G3.8 = f82d8cdc/G3.7 + ASurfaceControl_release no-op). Backup of f82d8cdc on device at `/data/local/tmp/runtime.pre-g38.bak`; patched .so banked at `docs/engine/V3-NOICE-G38-EVIDENCE/liboh_android_runtime.g38.so`. Rest of chain unchanged: bridge 7d0c471d (G3.4b+G3.6), ohaf 079593d8 (G2.8b+G3.6), libart 56f3caea (W20), gaihook+w14supp in LD_PRELOAD. HelloWorld still renders. Deploy = push to /data/local/tmp + verified-send, remount rw, cp to /system/lib + chcon system_lib_file, restart appspawn-x (no full reboot needed — only appspawn-x+children load this .so).

## Remaining frontiers (post-render)
1. **MainActivity sound list is EMPTY** — noice fetches its sound library over the network; getaddrinfo is stubbed (libgaihook.so) so the list has no items (white content area). To populate: give the child real network OR stub the library API to return a local manifest. This is the gap between "renders" and "shows full content".
2. **Bimodal bad-boot spin (intermittent):** on some cold boots noice's MAIN thread busy-loops 157s+ at 99% CPU inside `AppSpawnXInit.installSettingsContentProviderStub` (logs "enter", never "exit"; no FIX-VTABLE/W20-FIX markers; dumpcatcher/SIGQUIT can't safepoint it → native loop). The method does `Proxy.newProxyInstance(IContentProvider)` → libart synthesizes+links a $Proxy for the large IContentProvider interface → a pure-CPU class-linking runaway (NOT alloc — RSS stable 83MB, distinct from W20). Good boots skip it. Reboot to reroll. Root fix would be in libart vtable/iftable linking (operator-gated) or avoiding the IContentProvider Proxy.
3. Keyguard covers noice after idle (`power-shell timeout -o 600000` + `wakeup` + swipe-up `uinput -T -m 360 1050 360 250` to dismiss).

## Recovery note (post-reboot the board comes up bare)
After any reboot: SELinux resets to Enforcing, appspawn-x down, `/dev/memcg/perf_sensitive` cgroup missing (AP-3). Recover: `mkdir -p /dev/memcg/perf_sensitive; setenforce 0; nohup /data/local/tmp/start_asx.sh &`. See [[appspawnx-recovery-traps-2026-05-28]]. Launch noice FIRST (clean AMS) — kill-9/force-stop thrash degrades AMS into not-spawning (reboot to clear).

See also [[noice-egl-rootcause-g34]] (graphics chain), [[westlake-wall-map]].
