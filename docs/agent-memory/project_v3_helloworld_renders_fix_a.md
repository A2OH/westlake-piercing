---
name: v3-helloworld-renders-fix-a-2026-05-22
description: "First Android APK rendering on OHOS DAYU200 V7. Fix: 1-line Java preload in AppSpawnXInit forces parent eager-resolution of AppSchedulerBridge so child fork inherits via COW instead of broken-link stub. HelloWorld interactive UI with lifecycle (CREATED+RESUMED). McD progresses further but hits separate multi-dex LinkageError."
metadata:
  node_type: memory
  type: project
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Fact

**HelloWorld Android APK renders fully on OHOS DAYU200 V7 with interactive UI.** Achieved 2026-05-22 via Fix A — single Java method modification in [build-host]'s `AppSpawnXInit.preload()`.

Screenshot evidence: `docs/engine/V3-SESSION-2026-05-22-EVIDENCE/fix-a-132-helloworld-rendering.jpeg` shows:
- Title bar "Hello World"
- Centered "Hello World!" text
- CHANGE COLOR button
- Lifecycle log with `[LIFECYCLE] CREATED` + `[LIFECYCLE] RESUMED` (app's own onCreate/onResume markers)
- 3 action buttons: START SECONDACTIVITY, BIND HELLOSERVICE, UNBIND HELLOSERVICE

Process pid 6207 alive 4+ minutes post-launch (vs prior 10s LIFECYCLE_TIMEOUT wall). Child stderr grew to 232,596 lines (vs 117 baseline).

## Why

### Root cause of prior wall (per agent 131's instrumentation)

Parent appspawn-x successfully loads + registers natives on AppSchedulerBridge (`[P2-Bv2] RegisterNatives(AppSchedulerBridge, 3) OK`). But after fork:
- CHILD sees the class as a "broken link" stub
- `Class.forName(name, true, loader)` throws NoClassDefFoundError + zero `<clinit>` lines fire
- C++ `adapter_bridge_load_class` returns a non-null jclass (phantom stub)
- `GetStaticMethodID` returns null → `[B39-LA] method not found`
- bindApplication chain never fires → AMS LIFECYCLE_TIMEOUT → app killed before UI

Concurrent evidence of class duplication: `AppSpawnXInit$HiLogOutputStream` has two different Class addresses (0x70fe4cc8 vs 0x70fe4d40) for what should be the same `java.io.OutputStream`.

### Fix A (the actual change)

In `AppSpawnXInit.preload()` add explicit eager resolution:
```java
try {
    Class<?> bridgeClass = Class.forName("adapter.activity.AppSchedulerBridge", true,
                                         AppSpawnXInit.class.getClassLoader());
    System.err.println("=== Preloaded AppSchedulerBridge: " +
                       bridgeClass.getDeclaredMethods().length + " methods");
    Class.forName("adapter.activity.AppSchedulerBridge$OhTokenRegistry", true,
                  bridgeClass.getClassLoader());
} catch (Throwable t) {
    System.err.println("=== AppSchedulerBridge preload FAILED: " + t);
    t.printStackTrace(System.err);
}
```

This forces parent to fully resolve AppSchedulerBridge + its nested classes BEFORE the spawn listener accepts requests. Child inherits the fully-resolved class via COW fork → no broken-link state → GetStaticMethodID succeeds → bindApplication → handleLaunchActivity → MainActivity.onCreate → setContentView → ViewRootImpl.draw → UI renders.

### What fires after the fix (hilog)

```
[FIX-A-132] resolved adapter.activity.AppSchedulerBridge methods=73 fields=4 loader=PathClassLoader[...]
[FIX-A-132] nativeOnScheduleLaunchApplication OK: public static void ...
[FIX-A-132] notifyForegroundDeferred OK: public static void ...
[B43-LA] nativeOnScheduleLaunchApplication ENTRY bundle=com.example.helloworld
[B43-BIND] handleBindApplication returned OK
[B47-SLA] BEFORE/AFTER scheduleTransaction OK className=com.example.helloworld.MainActivity
[LIFECYCLE] CREATED  (from app's own MainActivity.onCreate())
[LIFECYCLE] RESUMED  (from app's own MainActivity.onResume())
[T=8000ms][G2.14as-VIS] mVisibleFromClient=true mVisibleFromServer=true lifecycleState=3 decorSize=720x1280
```

## How to apply

### Quick re-test recipe (on the board in current state)

1. Confirm Fix A jar deployed: `md5sum /system/android/framework/oh-adapter-runtime.jar` should be `7740c2dc03df7527201ed98359a8cd24`
2. Restart appspawn-x: `pkill -9 appspawn-x; sleep 2; rm -rf /data/misc/appspawnx/dalvik-cache/arm/*; /data/local/tmp/start_asx.sh &`
3. Wait 30s; verify Phase 4 listening: `tail /data/local/tmp/asx_run.err`
4. Verify socket: `chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX`
5. SELinux Permissive: `setenforce 0`
6. Wake + unlock screen
7. `aa start -a com.example.helloworld.MainActivity -b com.example.helloworld -m entry`
8. Snapshot: `snapshot_display -f /data/local/tmp/hw.jpeg`

### Rebuild the patched jar

Per agent 132: [build-host]'s build chain (JDK17 + d8) compiles `AppSpawnXInit.fix-a-132.java` against AOSP framework classpath. Source diff in `docs/engine/V3-SESSION-2026-05-22-EVIDENCE/AppSpawnXInit.fix-a-132.diff`.

### Upstream to [build-host]

Single-file Java diff, ~50 LOC, no API change, no behavior change in happy path — purely eager-resolution warmup. Should be uncontroversial to merge into [build-host]'s appspawn-x source.

## McD next wall (different bug family)

With Fix A applied, McD progresses MUCH further:
- Process pid 6413 spawned successfully
- `[B43-LA]` markers fire
- `[B47-SLA] AFTER scheduleTransaction OK` for SplashActivity
- THEN dies on `LinkageError on kotlinx.coroutines.Empty` in classes13.dex

Root cause: return-type mismatch between two `java.lang.Object` Class instances. **Different class-duplication family** than appspawn-x parent/child resolution Fix A addresses. McD has 33 dex files (multi-dex); the multi-dex loader may create a second PathClassLoader scope where Object resolves differently than BCP's Object.

Needs separate diagnostic (~½ day estimate) — possibly a Fix-A-style eager-resolution at app-side LoadedApk creation, or a multi-dex loader unification.

## Cross-references

- `docs/engine/V3-FIX-A-PRELOAD-RESULT-2026-05-22.md` — full agent 132 report
- `docs/engine/V3-JNI-LOOKUP-INSTRUMENTED-RESULT-2026-05-22.md` — agent 131 root cause finding
- `docs/engine/V3-[build-host]-SHIM-DEEP-STUDY.md` — agent 128 architecture map (Phase 1-4 plan, now partially superseded by Fix A direct path)
- `docs/engine/V3-SESSION-2026-05-22-EVIDENCE/` — all session artifacts
  - `AppSpawnXInit.fix-a-132.java` — patched Java source
  - `AppSpawnXInit.fix-a-132.diff` — diff vs original
  - `fix-a-132-helloworld-rendering.jpeg` — screenshot evidence
  - `hilog-critical.txt` — full hilog capture
- [[v3-h1-confirmed-file-contexts-brick-2026-05-22]] — the substrate-level brick fix that preceded this
- [[noice-inprocess-breakthrough]] — V2 in-process McD breakthrough (Android phone, FROZEN)

## Significance

This is the **first Android APK rendering on OHOS via [build-host]'s adapter project**, validated on our DAYU200 V7 board. Before today's session, V3 status was "10 bricks, no progress". End-of-day status: substrate solved + brick-safe deploy + APK install pipeline + Android process spawn + Java executing + UI rendering with interactive lifecycle.

The fix is elegant and minimal — 1 explicit eager-resolution in parent preload. This pattern should apply to other classes that exhibit the same parent/child resolution mismatch.
