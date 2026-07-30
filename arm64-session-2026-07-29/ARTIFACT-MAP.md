# Artifact map — what lives where on the board

Board: `5cdbf6af00000000000000000923012c`, aarch64, OHOS 6.1.0.31. Staging root `/data/local/tmp/asx`.

## Built by us, pushed to the device

| Artifact | Built by | Device path |
|---|---|---|
| `liboh_adapter_bridge.so` | `scripts/build/build_bridge_arm64.sh` (compiles **all** of `bridge-build/src/**.cpp` except appspawn-x into ONE .so) | `/data/local/tmp/asx/liboh_adapter_bridge.so` |
| `libminikin.so` | `scripts/build/build_minikin_stack_arm64.sh` (source `/home/dspfac/aosp-14-minikin`) | `/data/local/tmp/asx/libminikin.so` |
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

## Device-side prerequisites the app depends on
- `/etc/ssl/certs/cacert.pem` — real 181 KB CA bundle, used by the TLS layer.
- `/system/lib64/platformsdk/libssl_openssl.z.so` + `libcrypto_openssl.z.so` — OpenSSL 3.x.
- `/data/local/tmp/asx/libicuuc.so`, `libicui18n.so` — ICU **66**; exported symbols carry a `_66`
  suffix (`uregex_open_66`, `ucnv_open_66`). `ICU_DATA=/data/local/tmp/asx` holds `icudt66l.dat`.
- Two touch nodes: `event2` (`gsl680_tp`), `event5` (`VSoC touchscreen`) — run a forwarder on each.
