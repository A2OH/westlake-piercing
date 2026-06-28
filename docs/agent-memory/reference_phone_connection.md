---
name: Phone connections for Westlake testing
description: OnePlus 6 (rooted, LineageOS 22) and Pixel 7 Pro connected via Windows ADB
type: reference
---

## Current Phone: OnePlus 6 (enchilada)
- Model: ONEPLUS A6003, Snapdragon 845, ARM64, kernel 4.9.337
- OS: LineageOS 22 (Android 15, SDK 35)
- Root: Magisk, SELinux **Permissive**
- Serial: cfb7c9e3
- **IMPORTANT**: Must use bionic dalvikvm (`build-bionic-arm64`), NOT OHOS/musl build (hangs on kernel 4.9)
- JIT works (root + permissive = no SIGILL)
- MCD dashboard onCreate: 184ms (vs 20s interpreter mode on Pixel 7 Pro)

## Previous Phone: Pixel 7 Pro
- Serial: 2B151FDH3006QW
- Android 16 (SDK 36), no root
- Must use interpreter mode (`-Xint -Xusejit:false`) — JIT SIGILL under untrusted_app SELinux

## ADB
- Windows ADB: `/mnt/c/Users/<user>/Dev/platform-tools/adb.exe`
- ADB key already authorized
