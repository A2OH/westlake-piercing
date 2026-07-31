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
§476 `power`+`thermalservice`, §479 `media_router`.
