# Artifact map — what lives where on the board

Board: `5cdbf6af00000000000000000923012c`, aarch64, OHOS 6.1.0.31. Staging root `/data/local/tmp/asx`.

## Built by us, pushed to the device

| Artifact | Built by | Device path |
|---|---|---|
| `liboh_adapter_bridge.so` | `scripts/build/build_bridge_arm64.sh` (compiles **all** of `bridge-build/src/**.cpp` except appspawn-x into ONE .so) | `/data/local/tmp/asx/liboh_adapter_bridge.so` |
| `libminikin.so` | `scripts/build/build_minikin_stack_arm64.sh` (source `$WLROOT/aosp-14-minikin`) | `/data/local/tmp/asx/libminikin.so` |
| `oh-adapter-framework.jar` | `recipes/build-ohaf-jar.sh` (javac + d8 → replaces **classes2.dex**) | `/data/local/tmp/asx/fw/oh-adapter-framework.jar` |
| `framework.jar` (patched) | `recipes/patch-framework-jar.sh` | `/data/local/tmp/asx/fw/framework.jar` |
| `noice.apk` (patched) | `recipes/patch-noice-apk.sh` | `/data/local/tmp/asx/noice.apk` |
| `touchfwd` | `native-tools/touchfwd.c` | `/data/local/tmp/touchfwd` |
| `run406.sh` | `scripts/device/run406.sh` | `/data/local/tmp/run406.sh` |

## Backups already on the board (rollback targets)
```
/data/local/tmp/asx/noice.apk.pre440                 pre proxy-dex-surgery APK
/data/local/tmp/asx/fw/framework.jar.bak-pre464      pre framework dex surgery
/data/local/tmp/asx/fw/oh-adapter-framework.jar.pre441
/data/local/tmp/asx/libminikin.so.bak-pre457         pre font fix
/data/local/tmp/run406.sh.bak-pretouch               pre touchfwd
/data/local/tmp/asx/libart.so.bak-e1af9bb5           the ONLY known-good libart (md5 e1af9bb570b6ff1cae1419b9ea86c6cf)
```
⛔**Do not rebuild libart.** It is not reproducible from source: a full rebuild reaches exact marker
and undefined-symbol parity yet still SIGSEGVs the child at startup. Treat the deployed binary as
the artifact. Every fix in this session was deliberately done *without* touching it.

### ★The known-good libart also exists on the HOST
`$WIN_DEV_ROOT/wlstage/libart_good.so` — md5 `e1af9bb570b6ff1cae1419b9ea86c6cf`, 22,552,176 bytes,
byte-identical to the device backup. This matters: the board has been wiped before in this project, and
a device-only copy of a non-reproducible binary is one accident from being gone for good.
⚠️**Do not substitute any other `libart*.so` on the host.** There are ~20 of them and they are all
different builds — including `bridge-build-arm64/out/libart.so` (`3dc1fab9`, 22,212,104 B),
`out/board_libs/libart.so` (`dc0f8220`, 13,734,896 B — a different arch/config entirely) and several
plausibly-named ones like `libart.so.WORKING` and `libart-arm64-sleepfix-STABLE.so`. **Match the md5.**
Neither copy is in version control; make a durable backup before touching the board.

## Device-side prerequisites the app depends on
- `/etc/ssl/certs/cacert.pem` — real 181 KB CA bundle, used by the TLS layer.
- `/system/lib64/platformsdk/libssl_openssl.z.so` + `libcrypto_openssl.z.so` — OpenSSL 3.x.
- `/data/local/tmp/asx/libicuuc.so`, `libicui18n.so` — ICU **66**; exported symbols carry a `_66`
  suffix (`uregex_open_66`, `ucnv_open_66`). `ICU_DATA=/data/local/tmp/asx` holds `icudt66l.dat`.
- Two touch nodes: `event2` (`gsl680_tp`), `event5` (`VSoC touchscreen`) — run a forwarder on each.

## ⚠️Recipes that exist here but are deliberately NOT deployed

These were written while diagnosing the audio stall. They are hypothesis tests and instrumentation,
not fixes, and each changes behaviour beyond the bug being chased. Do not deploy them casually.

| Recipe / tool | What it changes | Why it is not deployed |
|---|---|---|
| `recipes/patch-framework-noop.sh` (§478) | blanks `MediaRouter.updateWifiDisplayStatus`, `getWifiDisplayStatusCode`, `isWifiDisplayEnabled` | it edits the platform purely to stop inert throws saturating libart's 40-slot probe — instrumentation, not a fix |
| `recipes/patch-noice-room.sh` (§485/§486) | makes the sound-metadata DAO skip `inTransaction`, then bypass `CoroutinesRoom` entirely | alters the app's DB transaction semantics; **neither variant worked** |
| `recipes/patch-noice-trace.sh` (§480) | injects ~34 trace call sites into the app dex | a debug build; useful to re-apply while investigating, never to leave on the device |

Device state was restored from the on-device backups and verified by md5:
```
noice.apk     <- noice.apk.pre480          e8ba750683d4b8392ae915074552a484
framework.jar <- framework.jar.bak-pre478  aef019e7e2599e2374ee2fe292e7b004
```
Both match what was deployed before this session's diagnosis, with the §440 app-dex and §464
framework surgery still in place.

**Still deployed, and intended to be** — each blocked the play path and supplies a genuinely missing
platform piece rather than changing behaviour: §470 charset handle adoption, §471
`android.media.MediaServiceManager`, §473 generated impls in place of Proxies, §474 `SecureRandom`,
§476 `power`+`thermalservice`, §479 `media_router`, **§504 `libcore.io.Memory`**.

## ★§504 — `libcore.io.Memory` was never bound (2026-08-04)

`libcore.io.Memory` lives in `libjavacore.so` upstream, which this port does not ship, and the bridge
never shimmed it. So `java.nio.DirectByteBuffer`'s bulk copies had no implementation:

```
UnsatisfiedLinkError: No implementation found for
  void libcore.io.Memory.pokeByteArray(long, byte[], int, int)
```

That is the copy `MediaCodecRenderer.feedInputBuffer()` performs to move an MP3 sample from the
extractor's `byte[]` into the direct input `ByteBuffer` from `MediaCodec.getInputBuffer()`. Three
things conspired to make it invisible for weeks:

1. `UnsatisfiedLinkError` is an **`Error`**. `ExoPlayerImplInternal.handleMessage` catches
   `ExoPlaybackException` / `IOException` / `RuntimeException` — not `Error` — so it escaped
   `Looper.loop()` and killed the `ExoPlayer:Playback` HandlerThread.
2. `ThreadGroup.uncaughtException` is a **PFCUT no-op** in this runtime, so the death was never
   reported. A `Thread.setDefaultUncaughtExceptionHandler` probe (§503) therefore counted zero —
   the handler is simply never consulted. The thread just vanished from `/proc/<pid>/task`.
3. Downstream the app had already taken input buffers 0..5 and now returned none, so the OH decoder
   spun forever on `AVBufferQueue: wait for free buffer, timeout = 50000`. The codec looked alive
   and healthy while `queueInputBuffer` stayed at zero.

Fix: `bridge-full-src/framework/android-runtime/src/libcore_io_Memory.cpp` binds `pokeByteArray`,
`peekByteArray` and `memmove` — pure ABI-boundary shim, no app, framework or libart change. Verify
with `[WESTLAKE-504] libcore.io.Memory bound 3/3` in the child stderr.

★**`grep -a JNIMISS` names every unbound native the runtime actually tried to call.** It had
`pokeByteArray` on the list the whole time. Read that list before theorising.

★Registration is done **one method at a time**: `RegisterNatives` fails the entire batch if any one
method is absent from the class, and this libcore is a partial port.

After §504 the `ExoPlayer:Playback` thread survives and the DAO query that §480-§482 blamed **does
resume** (`cache lambda: Room DAO query RESUMED (row in hand)` → `loadFromNetwork lambda RAN`) —
observed for the first time. ⚠️**Audio is still not audible**; the run did not reach codec creation.

## ⚠️Measure liveness before you measure anything else

A second run appeared to show the opposite — the DAO query never resuming, the DB "completely idle"
(`BEGIN`/`COMMIT`/`SQLiteTime` frozen at 15/11/851 for 180 s), and four leaked transactions. **All of
that was an artifact: the app had already died.** It was killed at `t=212s` by a single SIGSEGV, and
the measurements were taken ~85 minutes later against a dead pid.

```
[WESTLAKE-CHILDSEGV] #0 sig=11 code=2 addr=0x1b5ccd78 tid=12895
   art::interpreter::DoCall<false>  <-  InstructionHandler<...>::INVOKE_INTERFACE
   ... nested INVOKE_INTERFACE -> DoCall -> ArtInterpreterToInterpreterBridge -> ...
```

That is the **§436 invoke-interface wall** at a site neither §440 (3 app-dex sites) nor §464 (6
framework sites) covers. It is **intermittent** — pid 16901 ran over an hour and pid 8161 survived a
full interactive session, so it is not a deterministic gate and it is unrelated to §504.

Practical rules that follow, both learned the hard way here:
* ★A dead process makes *every* stall signature appear at once — frozen SQL counts, a suspend that
  never resumes, unbalanced `BEGIN`/`COMMIT`. **Never report a hang without proving the process was
  alive at the moment of measurement** (`ps -A | grep -c '[a]shu'`, or `alive=` in `run_*.log`).
* ★Unbalanced `BEGIN`/`COMMIT` is a *consequence* of dying mid-transaction, not a cause. Check the
  per-database split too: `androidx.work.workdb`'s lone `BEGIN EXCLUSIVE` is a separate DB and a red
  herring. The app DB is in **WAL** (`-wal`/`-shm` present), so an open write transaction does not
  block readers anyway.
* `scripts/host/wl_applog.sh` and `audio-run.sh`-style liveness asserts exist for this reason.
