---
name: OHOS MVP pipeline state
description: MVP-0/1/2 PASS on DAYU200 rk3568 board, DRM/KMS scan-out proven, what's next for noice/McD on OHOS
type: project
originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
status: SUPERSEDED-BY V3 (2026-05-16) for OHOS path; kept verbatim
---

> **SUPERSEDED 2026-05-16** for the OHOS production path by
> `docs/engine/V3-ARCHITECTURE.md` + `project_v3_hbc_reuse_direction.md`.
>
> The MVP-0/1/2 work + M6-OHOS daemon + Java M6 client documented below
> proved that Westlake-owned code CAN drive DRM/KMS scanout on DAYU200,
> but V3 supersedes the **dalvik-kitkat-on-OHOS** path entirely: [build-host]'s
> AOSP-14 ART + real libhwui + real Skia + real `framework.jar` cross-
> built to OHOS musl replaces dalvik-kitkat + SoftwareCanvas + drm_inproc_
> bridge for the OHOS target. The Westlake-owned `dalvik-port/compat/
> m6-drm-daemon/` and Java `M6DrmClient` will be archived (not deleted)
> under `archive/v2-ohos-substrate/` per W12/W13 plan.
>
> The infrastructure lessons (DRM/KMS scanout on rk3568, `composer_host`
> as DRM master, 32-bit ARM userspace constraint, XR24 BGRA layout,
> fb0 as compat node) are PRESERVED — V3 inherits the same hardware
> facts ([build-host] also runs 32-bit ARM on DAYU200).
>
> Content below preserved verbatim for traceability. Read
> `project_v3_hbc_reuse_direction.md` for the V3 OHOS path.

OHOS Phase 2 reached **visible-pixel** milestone on DAYU200 / rk3568 board (Cortex-A55 ×4, OHOS 7.0.0.18 Beta1) as of 2026-05-14.

## Milestones landed

| MVP | Commit | What proves |
|---|---|---|
| MVP-0 | `2664900a` | dalvikvm aarch64 runs Java on board (HelloOhos.dex marker prints) |
| MVP-1 | `2d00f89f` (#619) | `MainActivity.onCreate` runs via V2 substrate (`OhosTrivialActivity.onCreate reached pid=5013`) |
| MVP-2 | `44686464` | Red square scans out on DSI-1 panel via DRM/KMS; kernel debugfs confirms `plane[78] Smart0-win0 crtc=video_port1 fb=160 allocated by = drm_present` |
| M6-OHOS-Step1 | `c32a219e` | Long-lived `m6-drm-daemon`: self-test 346 page-flips in 5 s @ **14.48 ms (69.05 Hz)**, end-to-end AF_UNIX/memfd round-trip 120 frames RED/BLUE with `composer_host` alive throughout. Kernel debugfs: `allocated by = m6-drm-daemon` mid-flight. |
| M6-OHOS-Step2 | (this commit) | Java-side `M6DrmClient` (in dalvikvm Activity) submits 120 BGRA frames RED/BLUE to the daemon via AF_UNIX SCM_RIGHTS memfd handoff @ **28.97 ms/frame (34.52 Hz)** — within **0.04% of C-baseline 28.96 ms**. composer_host pid stable through entire run. |

## Reproducer (top-4 commands)

```bash
cd $HOME/android-to-openharmony-migration
bash scripts/run-ohos-test.sh trivial-activity       # MVP-1
bash scripts/run-ohos-test.sh red-square-drm         # MVP-2 (panel goes red for 12 s)
bash scripts/run-ohos-test.sh m6-drm-daemon          # M6-OHOS-Step1 (self-test + 120 frames AF_UNIX, C client)
bash scripts/run-ohos-test.sh m6-java-client         # M6-OHOS-Step2 (120 frames AF_UNIX, JAVA client)
```

## Hardware facts that bit us

- **Board userspace is 32-bit ARM on aarch64 kernel.** `render_service`, `libace_napi.z.so`, `libnative_window.so` all 32-bit; no `/system/lib64/`. Why: it's an early DAYU200 ROM. Consequence: dalvikvm (64-bit) cannot share address space with XComponent surface, so direct `OH_NativeWindow_*` calls from dalvikvm are unworkable until either (a) dalvikvm ports to ARM32 (Phase 1 has `ohos-sysroot-arm32` + `libc_static_fixed.a`) OR (b) cross-arch binder bridge.
- **`/dev/graphics/fb0` is a compat node.** rk3568 OHOS 7.0 scans DSI panel via DRM/KMS+dmabuf, not fbdev. Writes to fb0 succeed (pixels readback intact) but are invisible.
- **DRM master is `composer_host`** (binary actually `hdf_devhost` supervised by `hdf_devmgr`). Killing it releases master cleanly; respawns automatically. Use this for now; production path is a long-lived child holding master + memfd handoff (Phase 1 M6 daemon pattern).
- **rk3568 dumb BO format is XRGB8888 (XR24) LE.** Pure red = `00 00 FF FF` BGRA bytes.

## Pixel pipeline (working)

```
RedView.onDraw → SoftwareCanvas (Java) → BGRA byte[]
    → DrmPresenter (Java, JNI to libcore.io.Libcore.os)
        → drmOpen /dev/dri/card0 → drmSetMaster → drmModeAddFB2 → drmModeSetCrtc
            → DSI-1 / encoder 158 / CRTC 92 / plane Smart0-win0
```

## M6-OHOS-Step1 daemon details

```
/data/local/tmp/m6-drm-daemon --self-test 5 --no-kill-composer
  → 346 page-flips @ avg 14.48 ms = 69.05 Hz (panel native vsync)
    min 14.39 ms, max 14.57 ms — jitter < 200 µs

/data/local/tmp/m6-drm-daemon --accept-client --no-kill-composer
  + /data/local/tmp/m6-drm-daemon --test-client --frames 120 --split 60
  → 120 frames @ avg 28.96 ms = 34.5 Hz (= 2 × vsync, sync send/ack pipeline)
    AF_UNIX SOCK_SEQPACKET + SCM_RIGHTS memfd handoff
    composer_host pid unchanged through entire test
```

Wire (intentionally fresh, no Phase 1 DLST compat):
- client→daemon: 12-byte payload `[uint32 magic='M6FR'][uint32 seq][uint32 size]` + SCM_RIGHTS memfd
- daemon→client: 12-byte payload `[uint32 magic='M6AK'][uint32 seq][uint32 status]`

Socket path: `/data/local/tmp/westlake/m6-drm.sock` (filesystem),
fallback to abstract `@m6-drm.sock` if SELinux/fs blocks.

**Coexistence with composer_host: working.** `SET_MASTER` succeeds without
killing composer_host because composer_host only takes master when it
actively composites (which it doesn't in idle). On `DROP_MASTER` it can
reclaim. No SELinux denials encountered.

## Next for noice/McD on OHOS

Only one gate remains before noice/McD UI can render via M6 on OHOS:

1. ~~**Java-side `M6DrmClient`**~~ **DONE 2026-05-14** (M6-OHOS-Step2 above).
   - `shim/java/com/westlake/compat/UnixSocketBridge.java` — generic AF_UNIX +
     memfd + SCM_RIGHTS surfaces (8 native methods). Lives on BCP via the
     rebuilt `aosp-shim-ohos.dex` (4.88 MB).
   - `ohos-tests-gradle/m6-test/src/main/java/com/westlake/ohostests/m6/`:
     `M6DrmClient.java` (~190 LOC), `M6FramePainter.java`, `M6ClientTestActivity.java`.
   - Driver: `bash scripts/run-ohos-test.sh m6-java-client`.
   - Build helpers: `scripts/build-shim-dex-ohos.sh` (rebuild BCP shim
     with new bridge class), `dalvik-port/build-ohos.sh compile && link`
     (rebuild dalvikvm with new natives registered in libcore_bridge.cpp).
2. **NoiceInProcessActivity / McdInProcessActivity wired through M6** —
   they already render to `SoftwareCanvas` (proven on Android Phase 1).
   Now that `M6DrmClient` exists, point them at it instead of `Fb0Presenter`.
   The Java client's per-frame cost matches the C client at ~29 ms/frame
   = 2× vsync sync-ACK pipeline; for vsync-rate streaming the client just
   pipelines (send next while ACK is pending). Estimated <0.5 day.

Phase 1 had a similar 2-step path (host APK + dlst pipe consumer). On
OHOS both the consumer (daemon) AND the producer Java client now exist;
only the per-app wiring remains.

## File pointers

- Daemon source: `dalvik-port/compat/m6-drm-daemon/m6_drm_daemon.c` (one file, ~700 LOC, self-contained)
- Daemon build: `dalvik-port/compat/m6-drm-daemon/build.sh` (clang --target=aarch64-linux-ohos --static, ~74 KB ELF)
- Driver: `scripts/run-ohos-test.sh` subcommands: `status / push-bcp / hello / trivial-activity / red-square-drm / m6-drm-daemon / m6-java-client`
- BCP on board at `/data/local/tmp/westlake/`: `boot-aosp-shim.{art,oat,vdex}` + `boot-core-icu4j.{art,oat,vdex}` + `aosp-shim-ohos.dex` (4.88 MB non-stripped, ships Bundle/ContextThemeWrapper/Process AND `com.westlake.compat.UnixSocketBridge`) + `core-android-x86.jar` (1.2 MB, has CopyOnWriteArrayList unlike core-kitkat.jar) + `direct-print-stream.jar`
- Launcher: `ohos-tests-gradle/launcher/OhosMvpLauncher.java` (110 LOC, ZERO Unsafe/setAccessible/per-app branches)
- DRM presenter (single-shot): `ohos-tests-gradle/red-square/src/main/java/.../DrmPresenter.java` + helper at `dalvik-port/compat/drm_present.c`
- M6 Java client: `ohos-tests-gradle/m6-test/src/main/java/com/westlake/ohostests/m6/{M6DrmClient,M6FramePainter,M6ClientTestActivity}.java`
- POSIX bridge (BCP-side): `shim/java/com/westlake/compat/UnixSocketBridge.java` (8 native methods) + natives in `dalvik-port/compat/libcore_bridge.cpp` (`gUnixSocketBridgeMethods[]`)
- Shim rebuild for OHOS: `scripts/build-shim-dex-ohos.sh` (NON-stripped variant; sibling of `scripts/build-shim-dex.sh`)
- Artifacts: `artifacts/ohos-mvp/mvp{0,1,2}-*/`, `artifacts/ohos-mvp/m6-drm-daemon/<TS>/`, `artifacts/ohos-mvp/m6-java-client/<TS>/`
- Workstream doc: `docs/engine/OHOS_MVP_WORKSTREAMS.md`

## Limitations of the MVP-2 PASS

- No physical phone-camera photo (WSL harness has no camera). Evidence is kernel debugfs `plane=...fb=160 allocated by drm_present` mid-flight + pixel-source BGRA proof PNG decoded to (255,0,0,255) everywhere.
- composer_host is killed during the 12-s presentation, then respawns. Not production; production needs page-flip coexistence or daemon ownership.
- Single-shot static frame, not vsync'd animation. **M6-OHOS-Step1 lifted all three: long-lived daemon, vsync, composer_host coexists. M6-OHOS-Step2 lifts the LAST gap by adding the Java-side producer.**

**Strategic read:** the OHOS visible-pixel story is now end-to-end Java-source-to-DRM-scanout. Full noice on OHOS is the next ~2-3 weeks; full McD ~3-4 weeks (Hilt CR56 still open even on Android; GMS API surface unknown; rest is plumbing).
