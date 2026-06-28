---
name: Boot Image Loading & SELinux Fix
description: How to make ART11 speed boot image load on Huawei Mate 20 Pro — framework/arm64 path discovery + SELinux PROT_EXEC blocker
type: project
---

## Boot Image Version
- dalvikvm (AOSP Android 11) expects image version `085`, OAT version `183`
- Must use `$HOME/art-universal-build/build/bin/dex2oat` (NOT art-latest)
- Speed filter works: `--compiler-filter=speed --base=0x70000000 -j16`
- Must pass `--runtime-arg -Xverify:none` and `--android-root=/tmp/fake-android-root`

## Boot Image Path Discovery (CRITICAL)
ART does NOT load `-Ximage:` as a direct file path. It uses `-Ximage:` as a **logical location name** and looks for the image at `$ANDROID_ROOT/framework/$ISA/boot.art`.

**Working setup:**
```
ANDROID_ROOT=/data/local/tmp/westlake
-Ximage:boot.art   ← relative name, NOT absolute path!
```
ART finds: `/data/local/tmp/westlake/framework/arm64/boot.art`

**Also needed:**
- `BOOTCLASSPATH=$BCP` env var — prevents ART from using phone's system BCP
- `DEX2OATBOOTCLASSPATH=$BCP` — ditto for image generation fallback
- Fake dex2oat at `$ANDROID_ROOT/bin/dex2oat` (script that `exit 1`) — prevents hang on regen attempt
- `--oat-location=` during compilation to embed correct runtime paths

## Boot Image Compilation Command
```bash
$HOME/art-universal-build/build/bin/dex2oat \
  --dex-file=ohos-deploy/core-oj.jar --dex-location=/data/local/tmp/westlake/core-oj.jar \
  --dex-file=ohos-deploy/core-libart.jar --dex-location=/data/local/tmp/westlake/core-libart.jar \
  --dex-file=ohos-deploy/core-icu4j.jar --dex-location=/data/local/tmp/westlake/core-icu4j.jar \
  --oat-file=/tmp/out/boot.oat --image=/tmp/out/boot.art \
  --oat-location=/data/local/tmp/westlake/arm64/boot.oat \
  --instruction-set=arm64 --compiler-filter=speed --base=0x70000000 \
  --runtime-arg -Xverify:none --android-root=/tmp/fake-android-root -j16
```

## SELinux PROT_EXEC Blocker (EMUI)
**From adb shell (shell user):** Boot image loads fine. JIT compiler loads. McDonald's DI completes in seconds.

**From host app (untrusted_app_27):** `mmap(PROT_EXEC)` on .oat files DENIED by SELinux regardless of file location:
- `shell_data_file` (adb-pushed files): Permission denied
- `app_data_file` (app's private dir): Also Permission denied (empty error)

**Root cause:** EMUI's SELinux policy for `untrusted_app_27` blocks `mmap(PROT_EXEC)` on data files. Only `dalvikcache_data_file` and system files are allowed.

**Proven workaround:** Run dalvikvm from adb shell (pipe stdout/stderr to host app for rendering).
**Proper fix needed:** Either patch dalvikvm to not need PROT_EXEC, or find a way to get files into dalvikcache_data_file context.

## Security Properties (UUID fix — incomplete)
Created `/data/local/tmp/westlake/etc/security/security.properties` with:
```
security.provider.1=sun.security.provider.Sun
```
**Status:** File exists but NOT being loaded by core-oj.jar's Security class. The Providers error still occurs but is caught. UUID$Holder still fails permanently → SplashActivity.getExtraIntentData crashes.
